#include "URadarContactSubsystem.h"
#include "UBaseManagerSubsystem.h"
#include "UFactionIntelSubsystem.h"
#include "URadarTerrainSubsystem.h"
#include "UStrategyCampaignSubsystem.h"
#include "UStrategyEventDispatcher.h"
#include "UMissionManagerSubsystem.h"
#include "UExplorationSubsystem.h"
#include "UStrategyBase.h"
#include "UStrategyVehicle.h"
#include "UAIControllerSubsystem.h"
#include "Engine/Engine.h"

/** Subsystem startup — logs readiness; contact maps start empty. */
void URadarContactSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogTemp, Display, TEXT("URadarContactSubsystem initialized — base passive radar contacts ready"));
}

/** Campaign fallback default for base passive radar range (512 px). */
float URadarContactSubsystem::GetBaseRadarRange()
{
    return 512.0f;
}

namespace RadarContactHelpers
{
    /** Estimates px/hour velocity from mission phase, waypoints, or home-base bearing when movement is ambiguous. */
    FVector2D EstimateVehicleVelocity(const UStrategyVehicle* Vehicle)
    {
        if (!Vehicle)
        {
            return FVector2D::ZeroVector;
        }

        const float Speed = Vehicle->CruiseSpeedPixelsPerHour > 0.0f
            ? Vehicle->CruiseSpeedPixelsPerHour
            : 180.0f;

        FVector2D Direction = FVector2D::ZeroVector;

        if (Vehicle->GetMissionPhase() == EVehicleMissionPhase::Returning)
        {
            if (Vehicle->HomeBase)
            {
                Direction = (Vehicle->HomeBase->MapLocation - Vehicle->CurrentPosition).GetSafeNormal();
            }
            else if (Vehicle->ReturningWaypoints.Num() >= 2)
            {
                Direction = (Vehicle->ReturningWaypoints.Last() - Vehicle->CurrentPosition).GetSafeNormal();
            }
        }
        else if (Vehicle->CurrentWaypoints.Num() >= 2)
        {
            int32 SegmentStart = 0;
            int32 SegmentEnd = Vehicle->CurrentWaypoints.Num() - 1;
            float BestDistSq = MAX_FLT;

            for (int32 Index = 0; Index < Vehicle->CurrentWaypoints.Num() - 1; ++Index)
            {
                const FVector2D SegStart = Vehicle->CurrentWaypoints[Index];
                const FVector2D SegEnd = Vehicle->CurrentWaypoints[Index + 1];
                const FVector2D SegDir = SegEnd - SegStart;
                const float SegLenSq = SegDir.SizeSquared();
                if (SegLenSq <= KINDA_SMALL_NUMBER)
                {
                    continue;
                }

                const float T = FMath::Clamp(
                    FVector2D::DotProduct(Vehicle->CurrentPosition - SegStart, SegDir) / SegLenSq, 0.0f, 1.0f);
                const FVector2D Closest = SegStart + SegDir * T;
                const float DistSq = FVector2D::DistSquared(Vehicle->CurrentPosition, Closest);
                if (DistSq < BestDistSq)
                {
                    BestDistSq = DistSq;
                    SegmentStart = Index;
                    SegmentEnd = Index + 1;
                }
            }

            Direction = (Vehicle->CurrentWaypoints[SegmentEnd] - Vehicle->CurrentPosition).GetSafeNormal();
            if (Direction.IsNearlyZero())
            {
                Direction = (Vehicle->CurrentWaypoints[SegmentEnd] - Vehicle->CurrentWaypoints[SegmentStart]).GetSafeNormal();
            }
        }

        if (Direction.IsNearlyZero() && Vehicle->HomeBase)
        {
            Direction = (Vehicle->CurrentPosition - Vehicle->HomeBase->MapLocation).GetSafeNormal();
        }

        return Direction * Speed;
    }

    /** Intersection of segment P0→P1 with the circle centered at Base with radius Range (first entry along the segment). */
    static bool IntersectSegmentCircle(const FVector2D& Base, float Range, const FVector2D& P0, const FVector2D& P1,
        FVector2D& OutEntry)
    {
        const FVector2D D = P1 - P0;
        const FVector2D F = P0 - Base;
        const float A = FVector2D::DotProduct(D, D);
        if (A <= KINDA_SMALL_NUMBER)
        {
            return false;
        }

        const float B = 2.0f * FVector2D::DotProduct(F, D);
        const float C = FVector2D::DotProduct(F, F) - Range * Range;
        const float Discriminant = B * B - 4.0f * A * C;
        if (Discriminant < 0.0f)
        {
            return false;
        }

        const float SqrtDisc = FMath::Sqrt(Discriminant);
        float T = (-B - SqrtDisc) / (2.0f * A);
        if (T < 0.0f || T > 1.0f)
        {
            T = (-B + SqrtDisc) / (2.0f * A);
            if (T < 0.0f || T > 1.0f)
            {
                return false;
            }
        }

        OutEntry = P0 + D * T;
        return true;
    }

    /** Point on the passive-radar ring where the track first entered (backtracked along flight path). */
    FVector2D ComputeRadarRingEntryPoint(const FVector2D& BaseLocation, float RadarRange,
        const FVector2D& VehiclePosition, const UStrategyVehicle* Vehicle)
    {
        if (RadarRange <= KINDA_SMALL_NUMBER)
        {
            return VehiclePosition;
        }

        const FVector2D Velocity = EstimateVehicleVelocity(Vehicle);
        FVector2D BacktrackStart = VehiclePosition;

        if (!Velocity.IsNearlyZero())
        {
            const float BacktrackDist = FMath::Max(RadarRange * 2.5f,
                FVector2D::Distance(VehiclePosition, BaseLocation) + RadarRange);
            BacktrackStart = VehiclePosition - Velocity.GetSafeNormal() * BacktrackDist;
        }
        else
        {
            const FVector2D ToVehicle = VehiclePosition - BaseLocation;
            if (!ToVehicle.IsNearlyZero())
            {
                BacktrackStart = BaseLocation + ToVehicle.GetSafeNormal() * (RadarRange + 50.0f);
            }
            else
            {
                BacktrackStart = BaseLocation + FVector2D(RadarRange + 50.0f, 0.0f);
            }
        }

        FVector2D EntryPoint;
        if (IntersectSegmentCircle(BaseLocation, RadarRange, BacktrackStart, VehiclePosition, EntryPoint))
        {
            return EntryPoint;
        }

        const FVector2D ToVehicle = VehiclePosition - BaseLocation;
        if (!ToVehicle.IsNearlyZero())
        {
            return BaseLocation + ToVehicle.GetSafeNormal() * RadarRange;
        }

        return VehiclePosition;
    }
}

/**
 * Intercept/map marker position for a radar track.
 * Prefers FirstDetectedPosition (radar entry point) when bHasFirstDetectedPosition is set;
 * otherwise uses LastPosition for ongoing tracks.
 */
FVector2D URadarContactSubsystem::GetContactInterceptPosition(const FRadarContact& Contact)
{
    if (Contact.bHasFirstDetectedPosition)
    {
        return Contact.FirstDetectedPosition;
    }

    return Contact.LastPosition;
}

/** Selects HumanContactsById or EnemyContactsById for read/write access. */
TMap<FGuid, FRadarContact>& URadarContactSubsystem::GetContactMap(EFactionType Faction)
{
    return (Faction == EFactionType::Human) ? HumanContactsById : EnemyContactsById;
}

/** Const variant of GetContactMap for query-only code paths. */
const TMap<FGuid, FRadarContact>& URadarContactSubsystem::GetContactMap(EFactionType Faction) const
{
    return (Faction == EFactionType::Human) ? HumanContactsById : EnemyContactsById;
}

/** Returns the stable vehicle-key to ContactId map for the given detecting faction. */
TMap<TWeakObjectPtr<UStrategyVehicle>, FGuid>& URadarContactSubsystem::GetVehicleIdMap(EFactionType Faction)
{
    return (Faction == EFactionType::Human) ? HumanVehicleContactIds : EnemyVehicleContactIds;
}

/** Resets all contact data, vehicle mappings, interception targets, and ping accumulator. */
void URadarContactSubsystem::ClearAllContacts()
{
    HumanContactsById.Empty();
    EnemyContactsById.Empty();
    HumanVehicleContactIds.Empty();
    EnemyVehicleContactIds.Empty();
    ContactsWithActiveInterception.Empty();
    AccumulatedPingHours = 0.0f;
}

/** Copies all contacts from the faction map into a TArray for Blueprint/query use. */
TArray<FRadarContact> URadarContactSubsystem::GetContactsForFaction(EFactionType Faction) const
{
    TArray<FRadarContact> Result;
    for (const TPair<FGuid, FRadarContact>& Pair : GetContactMap(Faction))
    {
        Result.Add(Pair.Value);
    }
    return Result;
}

/** Looks up ContactId in the faction contact map and copies the entry to OutContact. */
bool URadarContactSubsystem::GetContactById(EFactionType Faction, FGuid ContactId, FRadarContact& OutContact) const
{
    if (const FRadarContact* Found = GetContactMap(Faction).Find(ContactId))
    {
        OutContact = *Found;
        return true;
    }
    return false;
}

/** Scores untargeted contacts by distance and inbound-threat bonus; requires round-trip range from OriginBase. */
bool URadarContactSubsystem::FindBestContactForInterception(EFactionType Faction, UStrategyBase* OriginBase,
    const UStrategyVehicle* Vehicle, FRadarContact& OutContact) const
{
    if (!OriginBase || !Vehicle)
    {
        return false;
    }

    const FVector2D Origin = OriginBase->MapLocation;
    const FRadarContact* Best = nullptr;
    float BestScore = -1.0f;

    for (const TPair<FGuid, FRadarContact>& Pair : GetContactMap(Faction))
    {
        if (ContactsWithActiveInterception.Contains(Pair.Key))
        {
            continue;
        }

        const FRadarContact& Contact = Pair.Value;
        const float Dist = FVector2D::Distance(Origin, GetContactInterceptPosition(Contact));
        const float RoundTrip = Dist * 2.0f;
        if (!Vehicle->HasEnoughRangeForMission(RoundTrip))
        {
            continue;
        }

        float Score = 1000.0f - Dist;
        if (Contact.bIsInboundThreat)
        {
            Score += 500.0f;
        }

        if (Score > BestScore)
        {
            BestScore = Score;
            Best = &Contact;
        }
    }

    if (Best)
    {
        OutContact = *Best;
        return true;
    }

    return false;
}

/** True when ContactId is in ContactsWithActiveInterception. */
bool URadarContactSubsystem::IsContactAlreadyTargeted(FGuid ContactId) const
{
    return ContactsWithActiveInterception.Contains(ContactId);
}

/** Adds ContactId to ContactsWithActiveInterception to prevent duplicate intercept launches. */
void URadarContactSubsystem::MarkContactTargeted(FGuid ContactId)
{
    ContactsWithActiveInterception.Add(ContactId);
}

/** Removes ContactId from ContactsWithActiveInterception when the intercept mission ends. */
void URadarContactSubsystem::UnmarkContactTargeted(FGuid ContactId)
{
    ContactsWithActiveInterception.Remove(ContactId);
}

/** Matches TrackedVehicleName to an active enemy fleet vehicle within 96 px of LastPosition. */
UStrategyVehicle* URadarContactSubsystem::ResolveTrackedVehicle(const FRadarContact& Contact,
    EFactionType DetectingFaction) const
{
    UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>();
    if (!MissionMgr)
    {
        return nullptr;
    }

    const EFactionType EnemyFaction = (DetectingFaction == EFactionType::Human) ? EFactionType::Enemy : EFactionType::Human;

    const TArray<UMissionGroup*> MissionSnapshot = MissionMgr->ActiveMissions;
    for (UMissionGroup* Mission : MissionSnapshot)
    {
        if (!Mission || !Mission->OriginBase || !Mission->bMovementActivated)
        {
            continue;
        }

        if (Mission->OriginBase->OwningFaction != EnemyFaction)
        {
            continue;
        }

        for (UStrategyVehicle* EnemyVehicle : Mission->VehiclesInFleet)
        {
            if (!EnemyVehicle || EnemyVehicle->IsDestroyed())
            {
                continue;
            }

            const FString VehicleName = EnemyVehicle->VehicleDefinition
                ? EnemyVehicle->VehicleDefinition->VehicleName.ToString()
                : EnemyVehicle->GetName();

            if (VehicleName != Contact.TrackedVehicleName)
            {
                continue;
            }

            if (FVector2D::Distance(EnemyVehicle->CurrentPosition, Contact.LastPosition) <= 96.0f)
            {
                return EnemyVehicle;
            }
        }
    }

    return nullptr;
}

/** Returns offensive target base name, or the nearest friendly base name as a fallback label. */
FString URadarContactSubsystem::InferThreatenedBaseName(const UStrategyVehicle* EnemyVehicle, EFactionType FriendlyFaction,
    UBaseManagerSubsystem* BaseMgr)
{
    if (!EnemyVehicle || !BaseMgr)
    {
        return FString();
    }

    if (EnemyVehicle->CurrentMission)
    {
        if (EnemyVehicle->CurrentMission->MissionType == EMissionType::Offensive
            && EnemyVehicle->CurrentMission->TargetEnemyBase
            && EnemyVehicle->CurrentMission->TargetEnemyBase->OwningFaction == FriendlyFaction)
        {
            return EnemyVehicle->CurrentMission->TargetEnemyBase->BaseName.ToString();
        }
    }

    UStrategyBase* NearestBase = nullptr;
    float NearestDist = MAX_FLT;
    for (UStrategyBase* FriendlyBase : BaseMgr->GetBases(FriendlyFaction))
    {
        if (!FriendlyBase)
        {
            continue;
        }

        const float Dist = FVector2D::Distance(EnemyVehicle->CurrentPosition, FriendlyBase->MapLocation);
        if (Dist < NearestDist)
        {
            NearestDist = Dist;
            NearestBase = FriendlyBase;
        }
    }

    return NearestBase ? NearestBase->BaseName.ToString() : FString();
}

/** True for offensive missions toward friendly bases, interception missions, or close approach to friendly bases. */
bool URadarContactSubsystem::IsInboundThreatVehicle(const UStrategyVehicle* EnemyVehicle, EFactionType FriendlyFaction,
    UBaseManagerSubsystem* BaseMgr)
{
    if (!EnemyVehicle || !BaseMgr)
    {
        return false;
    }

    if (EnemyVehicle->CurrentMission)
    {
        if (EnemyVehicle->CurrentMission->MissionType == EMissionType::Offensive)
        {
            if (EnemyVehicle->CurrentMission->TargetEnemyBase
                && EnemyVehicle->CurrentMission->TargetEnemyBase->OwningFaction == FriendlyFaction)
            {
                return true;
            }
        }

        if (EnemyVehicle->CurrentMission->MissionType == EMissionType::Interception)
        {
            return true;
        }
    }

    const FVector2D EnemyPos = EnemyVehicle->CurrentPosition;
    float NearestFriendlyDist = MAX_FLT;
    for (UStrategyBase* FriendlyBase : BaseMgr->GetBases(FriendlyFaction))
    {
        if (!FriendlyBase)
        {
            continue;
        }

        NearestFriendlyDist = FMath::Min(NearestFriendlyDist,
            FVector2D::Distance(EnemyPos, FriendlyBase->MapLocation));
    }

    return NearestFriendlyDist < GetBaseRadarRange() * 1.25f;
}

/** Accumulates DeltaGameHours and runs ProcessBaseRadarPings each time the ping interval is reached. */
void URadarContactSubsystem::TickBaseRadar(float CurrentGameHours, float DeltaGameHours)
{
    if (DeltaGameHours <= 0.0f)
    {
        return;
    }

    UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();
    if (!Campaign || !Campaign->bBasePassiveRadarEnabled)
    {
        return;
    }

    AccumulatedPingHours += DeltaGameHours;
    const float PingInterval = FMath::Max(0.25f, Campaign->BaseRadarPingIntervalHours);

    while (AccumulatedPingHours >= PingInterval)
    {
        AccumulatedPingHours -= PingInterval;
        ProcessBaseRadarPings(CurrentGameHours);
    }
}

/** Prunes contacts past RadarContactExpiryHours, broadcasts expiry, and cleans vehicle/intercept bookkeeping. */
void URadarContactSubsystem::ExpireStaleContacts(float CurrentGameHours)
{
    UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();
    const float ExpiryHours = Campaign ? FMath::Max(1.0f, Campaign->RadarContactExpiryHours) : 6.0f;

    auto PruneFaction = [&](EFactionType Faction)
    {
        TMap<FGuid, FRadarContact>& ContactMap = GetContactMap(Faction);
        TMap<TWeakObjectPtr<UStrategyVehicle>, FGuid>& VehicleMap = GetVehicleIdMap(Faction);
        TArray<FGuid> ToRemove;

        for (const TPair<FGuid, FRadarContact>& Pair : ContactMap)
        {
            if (CurrentGameHours - Pair.Value.LastSeenGameHours > ExpiryHours)
            {
                ToRemove.Add(Pair.Key);
            }
        }

        for (const FGuid& Id : ToRemove)
        {
            if (const FRadarContact* ExpiredContact = ContactMap.Find(Id))
            {
                if (UStrategyEventDispatcher* Events = GetGameInstance()->GetSubsystem<UStrategyEventDispatcher>())
                {
                    Events->OnRadarContactExpired.Broadcast(Faction, *ExpiredContact);
                }

                UE_LOG(LogTemp, Verbose, TEXT("[BASE RADAR] %s contact expired: %s"),
                    *UEnum::GetValueAsString(Faction), *ExpiredContact->TrackedVehicleName);
            }

            ContactMap.Remove(Id);
            ContactsWithActiveInterception.Remove(Id);
        }

        TArray<TWeakObjectPtr<UStrategyVehicle>> StaleVehicleKeys;
        for (const TPair<TWeakObjectPtr<UStrategyVehicle>, FGuid>& Pair : VehicleMap)
        {
            if (!Pair.Key.IsValid() || ToRemove.Contains(Pair.Value))
            {
                StaleVehicleKeys.Add(Pair.Key);
            }
        }

        for (const TWeakObjectPtr<UStrategyVehicle>& Key : StaleVehicleKeys)
        {
            VehicleMap.Remove(Key);
        }
    };

    PruneFaction(EFactionType::Human);
    PruneFaction(EFactionType::Enemy);
}

/** Discovers sites in range (or refreshes intel for known sites) with optional terrain LOS blocking. */
void URadarContactSubsystem::ProcessBaseSites(UStrategyBase* Base, EFactionType Faction, float Range,
    float CurrentGameHours, UBaseManagerSubsystem* BaseMgr, UFactionIntelSubsystem* IntelMgr,
    URadarTerrainSubsystem* TerrainMgr)
{
    if (!Base || !BaseMgr)
    {
        return;
    }

    for (UStrategySiteDefinition* Site : BaseMgr->AllPotentialSites)
    {
        if (!Site)
        {
            continue;
        }

        if (FVector2D::Distance(Site->Location, Base->MapLocation) > Range)
        {
            continue;
        }

        if (TerrainMgr && !TerrainMgr->HasRadarLineOfSight(Base->MapLocation, Site->Location))
        {
            UE_LOG(LogTemp, Verbose, TEXT("[BASE RADAR LOS] %s blocked site ping to %s"),
                *Base->BaseName.ToString(), *Site->SiteName);
            continue;
        }

        const bool bAlreadyKnown = BaseMgr->IsSiteKnownToFaction(Faction, Site);
        if (!bAlreadyKnown)
        {
            BaseMgr->AddDiscoveredSite(Faction, Site, EDiscoveryReason::Radar);
        }
        else if (IntelMgr)
        {
            IntelMgr->ObserveSite(Faction, Site, EDiscoveryReason::Radar, CurrentGameHours);
        }
    }
}

/** Creates or updates a per-vehicle contact, locking FirstDetectedPosition on first sighting and estimating velocity. */
FRadarContact URadarContactSubsystem::UpsertVehicleContact(EFactionType DetectingFaction, UStrategyBase* DetectingBase,
    UStrategyVehicle* EnemyVehicle, float CurrentGameHours, bool bIsInboundThreat, float DetectingRadarRangePixels)
{
    FRadarContact Result;

    if (!DetectingBase || !EnemyVehicle || EnemyVehicle->IsDestroyed())
    {
        return Result;
    }

    TMap<FGuid, FRadarContact>& ContactMap = GetContactMap(DetectingFaction);
    TMap<TWeakObjectPtr<UStrategyVehicle>, FGuid>& VehicleMap = GetVehicleIdMap(DetectingFaction);
    const TWeakObjectPtr<UStrategyVehicle> VehicleKey(EnemyVehicle);

    FGuid ContactId;
    if (const FGuid* ExistingId = VehicleMap.Find(VehicleKey))
    {
        ContactId = *ExistingId;
    }
    else
    {
        ContactId = FGuid::NewGuid();
        VehicleMap.Add(VehicleKey, ContactId);
    }

    FRadarContact& Contact = ContactMap.FindOrAdd(ContactId);
    const FVector2D PreviousPosition = Contact.LastPosition;
    const bool bHadPrevious = Contact.LastSeenGameHours > 0.0f;
    const FVector2D CurrentPosition = EnemyVehicle->CurrentPosition;

    if (!bHadPrevious)
    {
        Contact.ContactId = ContactId;
        Contact.DetectingFaction = DetectingFaction;
        Contact.DetectingBaseName = DetectingBase->BaseName.ToString();
        const float EffectiveRange = DetectingRadarRangePixels > KINDA_SMALL_NUMBER
            ? DetectingRadarRangePixels
            : GetBaseRadarRange();
        Contact.FirstDetectedPosition = RadarContactHelpers::ComputeRadarRingEntryPoint(
            DetectingBase->MapLocation, EffectiveRange, CurrentPosition, EnemyVehicle);
        Contact.bHasFirstDetectedPosition = true;
        Contact.EstimatedVelocity = RadarContactHelpers::EstimateVehicleVelocity(EnemyVehicle);
    }
    else
    {
        const float DeltaHours = FMath::Max(0.01f, CurrentGameHours - Contact.LastSeenGameHours);
        const FVector2D MeasuredVelocity = (CurrentPosition - PreviousPosition) / DeltaHours;
        if (!MeasuredVelocity.IsNearlyZero())
        {
            Contact.EstimatedVelocity = MeasuredVelocity;
        }
        else if (Contact.EstimatedVelocity.IsNearlyZero())
        {
            Contact.EstimatedVelocity = RadarContactHelpers::EstimateVehicleVelocity(EnemyVehicle);
        }
    }

    Contact.LastPosition = CurrentPosition;
    Contact.LastSeenGameHours = CurrentGameHours;
    Contact.bIsInboundThreat = bIsInboundThreat;
    Contact.EstimatedHeadingDegrees = Contact.EstimatedVelocity.IsNearlyZero()
        ? 0.0f
        : FMath::RadiansToDegrees(FMath::Atan2(Contact.EstimatedVelocity.Y, Contact.EstimatedVelocity.X));

    if (UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>())
    {
        if (bIsInboundThreat)
        {
            Contact.ThreatenedBaseName = InferThreatenedBaseName(EnemyVehicle, DetectingFaction, BaseMgr);
        }
        else
        {
            Contact.ThreatenedBaseName.Empty();
        }
    }

    Contact.TrackedVehicleName = EnemyVehicle->VehicleDefinition
        ? EnemyVehicle->VehicleDefinition->VehicleName.ToString()
        : EnemyVehicle->GetName();

    const FVector2D EntryPos = GetContactInterceptPosition(Contact);
    const bool bIsNewContact = !bHadPrevious;

    if (bIsNewContact && Contact.bIsInboundThreat)
    {
        UE_LOG(LogTemp, Display, TEXT("[BASE RADAR] %s FIRST INBOUND at '%s': %s entered at (%.0f, %.0f) heading %.0f deg speed %.0f"),
            *UEnum::GetValueAsString(DetectingFaction),
            *DetectingBase->BaseName.ToString(),
            *Contact.TrackedVehicleName,
            EntryPos.X, EntryPos.Y,
            Contact.EstimatedHeadingDegrees,
            Contact.EstimatedVelocity.Size());

        if (UExplorationSubsystem* Exploration = GetGameInstance()->GetSubsystem<UExplorationSubsystem>())
        {
            Exploration->NotifyInboundThreatContact(DetectingBase, Contact, CurrentGameHours);
        }
    }
    else
    {
        UE_LOG(LogTemp, Display, TEXT("[BASE RADAR] %s at '%s' tracked %s at (%.0f, %.0f)%s"),
            *UEnum::GetValueAsString(DetectingFaction),
            *DetectingBase->BaseName.ToString(),
            *Contact.TrackedVehicleName,
            Contact.LastPosition.X, Contact.LastPosition.Y,
            Contact.bIsInboundThreat ? TEXT(" — INBOUND") : TEXT(""));
    }

    UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();
    if (UStrategyEventDispatcher* Events = GetGameInstance()->GetSubsystem<UStrategyEventDispatcher>())
    {
        Events->OnRadarContactUpdated.Broadcast(DetectingFaction, Contact);

        if (bIsNewContact && Contact.bIsInboundThreat && Campaign && Campaign->bNotifyPlayerOfEnemyRadarContacts
            && DetectingFaction == EFactionType::Enemy)
        {
            const FText AlertMessage = FText::FromString(FString::Printf(
                TEXT("Enemy radar may have detected %s near (%.0f, %.0f)"),
                *Contact.TrackedVehicleName, EntryPos.X, EntryPos.Y));
            Events->OnOpposingFactionRadarAlert.Broadcast(Contact, AlertMessage);
        }
    }

    return Contact;
}

/**
 * Appends to DeferredReactiveIntercepts when an inbound threat is pinged mid-cycle.
 * Dedupes by faction+ContactId and skips already-targeted contacts.
 */
void URadarContactSubsystem::QueueReactiveInterception(EFactionType Faction, UStrategyBase* Base, FGuid ContactId)
{
    if (!Base || !ContactId.IsValid() || ContactsWithActiveInterception.Contains(ContactId))
    {
        return;
    }

    for (const FDeferredReactiveIntercept& Pending : DeferredReactiveIntercepts)
    {
        if (Pending.Faction == Faction && Pending.ContactId == ContactId)
        {
            return;
        }
    }

    FDeferredReactiveIntercept Entry;
    Entry.Faction = Faction;
    Entry.Base = Base;
    Entry.ContactId = ContactId;
    DeferredReactiveIntercepts.Add(Entry);
}

/**
 * Drains the deferred reactive intercept queue after all bases have pinged.
 * Re-validates each entry (base alive, contact still inbound) before TryReactiveInterception.
 */
void URadarContactSubsystem::FlushDeferredReactiveInterceptions(UMissionManagerSubsystem* MissionMgr)
{
    if (!MissionMgr || DeferredReactiveIntercepts.Num() == 0)
    {
        DeferredReactiveIntercepts.Empty();
        return;
    }

    const TArray<FDeferredReactiveIntercept> Pending = DeferredReactiveIntercepts;
    DeferredReactiveIntercepts.Empty();

    for (const FDeferredReactiveIntercept& Entry : Pending)
    {
        if (ContactsWithActiveInterception.Contains(Entry.ContactId))
        {
            continue;
        }

        UStrategyBase* Base = Entry.Base.Get();
        if (!Base)
        {
            continue;
        }

        FRadarContact Contact;
        if (!GetContactById(Entry.Faction, Entry.ContactId, Contact) || !Contact.bIsInboundThreat)
        {
            continue;
        }

        TryReactiveInterception(Entry.Faction, Base, Contact, MissionMgr);
    }
}

/** Launches the first successful LaunchInterceptionAtContact from idle combat vehicles at Base. */
void URadarContactSubsystem::TryReactiveInterception(EFactionType Faction, UStrategyBase* Base,
    const FRadarContact& Contact, UMissionManagerSubsystem* MissionMgr)
{
    if (!Base || !MissionMgr || !Contact.bIsInboundThreat || ContactsWithActiveInterception.Contains(Contact.ContactId))
    {
        return;
    }

    UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();
    if (!Campaign || !Campaign->bAIReactiveInterceptionEnabled)
    {
        return;
    }

    UAIControllerSubsystem* AI = GetGameInstance()->GetSubsystem<UAIControllerSubsystem>();
    if (!AI)
    {
        return;
    }

    const bool bFactionAIEnabled = (Faction == EFactionType::Human)
        ? AI->IsSimulatingHumanAI()
        : AI->IsSimulatingEnemyAI();
    if (!bFactionAIEnabled)
    {
        return;
    }

    const TArray<UStrategyVehicle*> IdleVehicles = MissionMgr->GatherIdleVehiclesAtBase(Base);
    for (UStrategyVehicle* Vehicle : IdleVehicles)
    {
        if (!Vehicle || !Vehicle->VehicleDefinition)
        {
            continue;
        }

        if (!UAIControllerSubsystem::IsCombatVehicleType(Vehicle->VehicleDefinition->VehicleType))
        {
            continue;
        }

        if (MissionMgr->LaunchInterceptionAtContact(Base, Vehicle, Contact.ContactId))
        {
            UE_LOG(LogTemp, Display, TEXT("[INTERCEPT AI] %s reactive interception from '%s' → %s"),
                *UEnum::GetValueAsString(Faction),
                *Base->BaseName.ToString(),
                *Contact.TrackedVehicleName);
            return;
        }
    }
}

/** Tracks enemy vehicles in radar range; queues deferred reactive intercepts for inbound threat contacts. */
void URadarContactSubsystem::ProcessBaseVehicles(UStrategyBase* Base, EFactionType Faction, float Range,
    float CurrentGameHours, UMissionManagerSubsystem* MissionMgr)
{
    if (!Base || !MissionMgr)
    {
        return;
    }

    UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    URadarTerrainSubsystem* TerrainMgr = GetGameInstance()->GetSubsystem<URadarTerrainSubsystem>();
    if (!BaseMgr)
    {
        return;
    }

    const EFactionType EnemyFaction = (Faction == EFactionType::Human) ? EFactionType::Enemy : EFactionType::Human;

    const TArray<UMissionGroup*> MissionSnapshot = MissionMgr->ActiveMissions;
    for (UMissionGroup* Mission : MissionSnapshot)
    {
        if (!Mission || !Mission->bMovementActivated || !Mission->OriginBase)
        {
            continue;
        }

        if (Mission->OriginBase->OwningFaction != EnemyFaction)
        {
            continue;
        }

        for (UStrategyVehicle* EnemyVehicle : Mission->VehiclesInFleet)
        {
            if (!EnemyVehicle || EnemyVehicle->IsDestroyed()
                || EnemyVehicle->CurrentPhase == EVehicleMissionPhase::Docked)
            {
                continue;
            }

            if (FVector2D::Distance(EnemyVehicle->CurrentPosition, Base->MapLocation) > Range)
            {
                continue;
            }

            if (TerrainMgr && !TerrainMgr->HasRadarLineOfSight(Base->MapLocation, EnemyVehicle->CurrentPosition))
            {
                UE_LOG(LogTemp, Verbose, TEXT("[BASE RADAR LOS] %s blocked vehicle ping"),
                    *Base->BaseName.ToString());
                continue;
            }

            const bool bInbound = IsInboundThreatVehicle(EnemyVehicle, Faction, BaseMgr);
            const FRadarContact Contact = UpsertVehicleContact(Faction, Base, EnemyVehicle, CurrentGameHours, bInbound, Range);
            if (Contact.ContactId.IsValid() && Contact.bIsInboundThreat)
            {
                QueueReactiveInterception(Faction, Base, Contact.ContactId);
            }
        }
    }
}

/** Full ping pass for both factions: sites, vehicles, flush deferred intercepts, expire stale contacts. */
void URadarContactSubsystem::ProcessBaseRadarPings(float CurrentGameHours)
{
    UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    UFactionIntelSubsystem* IntelMgr = GetGameInstance()->GetSubsystem<UFactionIntelSubsystem>();
    URadarTerrainSubsystem* TerrainMgr = GetGameInstance()->GetSubsystem<URadarTerrainSubsystem>();
    UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>();
    if (!BaseMgr || !MissionMgr)
    {
        return;
    }

    UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();
    const float Range = Campaign ? Campaign->BaseRadarRangePixels : GetBaseRadarRange();

    for (const EFactionType Faction : { EFactionType::Human, EFactionType::Enemy })
    {
        for (UStrategyBase* Base : BaseMgr->GetBases(Faction))
        {
            if (!Base || !Base->HasOperationalCommandCenter())
            {
                continue;
            }

            ProcessBaseSites(Base, Faction, Range, CurrentGameHours, BaseMgr, IntelMgr, TerrainMgr);
            ProcessBaseVehicles(Base, Faction, Range, CurrentGameHours, MissionMgr);
        }
    }

    FlushDeferredReactiveInterceptions(MissionMgr);
    ExpireStaleContacts(CurrentGameHours);
}
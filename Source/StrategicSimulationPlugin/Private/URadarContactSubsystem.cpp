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

void URadarContactSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogTemp, Display, TEXT("URadarContactSubsystem initialized — base passive radar contacts ready"));
}

float URadarContactSubsystem::GetBaseRadarRange()
{
    return 512.0f;
}

namespace RadarContactHelpers
{
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
            Direction = (Vehicle->CurrentWaypoints[1] - Vehicle->CurrentPosition).GetSafeNormal();
            if (Direction.IsNearlyZero())
            {
                Direction = (Vehicle->CurrentWaypoints[1] - Vehicle->CurrentWaypoints[0]).GetSafeNormal();
            }
        }

        if (Direction.IsNearlyZero() && Vehicle->HomeBase)
        {
            Direction = (Vehicle->CurrentPosition - Vehicle->HomeBase->MapLocation).GetSafeNormal();
        }

        return Direction * Speed;
    }
}

FVector2D URadarContactSubsystem::GetContactInterceptPosition(const FRadarContact& Contact)
{
    if (Contact.bHasFirstDetectedPosition)
    {
        return Contact.FirstDetectedPosition;
    }

    return Contact.LastPosition;
}

TMap<FGuid, FRadarContact>& URadarContactSubsystem::GetContactMap(EFactionType Faction)
{
    return (Faction == EFactionType::Human) ? HumanContactsById : EnemyContactsById;
}

const TMap<FGuid, FRadarContact>& URadarContactSubsystem::GetContactMap(EFactionType Faction) const
{
    return (Faction == EFactionType::Human) ? HumanContactsById : EnemyContactsById;
}

TMap<TWeakObjectPtr<UStrategyVehicle>, FGuid>& URadarContactSubsystem::GetVehicleIdMap(EFactionType Faction)
{
    return (Faction == EFactionType::Human) ? HumanVehicleContactIds : EnemyVehicleContactIds;
}

void URadarContactSubsystem::ClearAllContacts()
{
    HumanContactsById.Empty();
    EnemyContactsById.Empty();
    HumanVehicleContactIds.Empty();
    EnemyVehicleContactIds.Empty();
    ContactsWithActiveInterception.Empty();
    AccumulatedPingHours = 0.0f;
}

TArray<FRadarContact> URadarContactSubsystem::GetContactsForFaction(EFactionType Faction) const
{
    TArray<FRadarContact> Result;
    for (const TPair<FGuid, FRadarContact>& Pair : GetContactMap(Faction))
    {
        Result.Add(Pair.Value);
    }
    return Result;
}

bool URadarContactSubsystem::GetContactById(EFactionType Faction, FGuid ContactId, FRadarContact& OutContact) const
{
    if (const FRadarContact* Found = GetContactMap(Faction).Find(ContactId))
    {
        OutContact = *Found;
        return true;
    }
    return false;
}

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

bool URadarContactSubsystem::IsContactAlreadyTargeted(FGuid ContactId) const
{
    return ContactsWithActiveInterception.Contains(ContactId);
}

void URadarContactSubsystem::MarkContactTargeted(FGuid ContactId)
{
    ContactsWithActiveInterception.Add(ContactId);
}

void URadarContactSubsystem::UnmarkContactTargeted(FGuid ContactId)
{
    ContactsWithActiveInterception.Remove(ContactId);
}

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

FRadarContact URadarContactSubsystem::UpsertVehicleContact(EFactionType DetectingFaction, UStrategyBase* DetectingBase,
    UStrategyVehicle* EnemyVehicle, float CurrentGameHours, bool bIsInboundThreat)
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
        Contact.FirstDetectedPosition = CurrentPosition;
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
            const FRadarContact Contact = UpsertVehicleContact(Faction, Base, EnemyVehicle, CurrentGameHours, bInbound);
            if (Contact.ContactId.IsValid() && Contact.bIsInboundThreat)
            {
                QueueReactiveInterception(Faction, Base, Contact.ContactId);
            }
        }
    }
}

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
#include "UStrategyVehicle.h"
#include "UStrategyBase.h"
#include "UAIControllerSubsystem.h"
#include "UStrategyFacility.h"
#include "UMissionGroup.h"
#include "UFacilityDefinition.h"
#include "UItemDefinition.h"
#include "UBaseManagerSubsystem.h"
#include "UMissionManagerSubsystem.h"
#include "StrategicSiteDefinition.h"
#include "UStrategyCampaignSubsystem.h"
#include "UResourceManagerSubsystem.h"
#include "USoldierManagerSubsystem.h"
#include "UStrategicSimulationDisplayHelpers.h"
#include "Engine/Engine.h"

static UMissionManagerSubsystem* GetMissionManagerForVehicle(UStrategyVehicle* Vehicle);

UStrategyVehicle::UStrategyVehicle()
{
    CurrentRangeLeft = 0.0f;
    CurrentHealth = 100;
    DamageState = EVehicleDamageState::Undamaged;
    CurrentPhase = EVehicleMissionPhase::Docked;
    CurrentBehavior = EVehicleBehavior::Idle;

    CurrentPosition = FVector2D::ZeroVector;
    CurrentWaypoints.Empty();
    LaunchGameTimeHours = 0.0f;
    TotalTravelTimeHours = 0.0f;
    LastPingGameTimeHours = 0.0f;
    CruiseSpeedPixelsPerHour = 180.0f;
    PingIntervalHours = 0.5f;
}

float UStrategyVehicle::GetCruiseSpeed() const
{
    if (CruiseSpeedPixelsPerHour > 0.0f)
    {
        return CruiseSpeedPixelsPerHour;
    }
    return 180.0f;
}

float UStrategyVehicle::GetMaxRange() const
{
    return VehicleDefinition ? VehicleDefinition->MaxRange : 800.0f;
}

bool UStrategyVehicle::HasEnoughRangeForMission(float RequiredDistance) const
{
    return CurrentRangeLeft >= RequiredDistance;
}

bool UStrategyVehicle::IsMissionFinished() const
{
    if (IsDestroyed())
    {
        return true;
    }

    if (!CurrentMission)
    {
        return false;
    }

    if (!CurrentMission->bMovementActivated)
    {
        return false;
    }

    return CurrentPhase == EVehicleMissionPhase::Docked;
}

void UStrategyVehicle::ApplyDamage(int32 DamageAmount)
{
    if (DamageAmount <= 0 || IsDestroyed()) return;

    CurrentHealth = FMath::Max(0, CurrentHealth - DamageAmount);
    UpdateDamageStateFromHealth();

    UE_LOG(LogTemp, Warning, TEXT("[VEHICLE] %s took %d damage → Health: %d/%d (%s)"),
        VehicleDefinition ? *VehicleDefinition->VehicleName.ToString() : TEXT("Unknown"),
        DamageAmount, CurrentHealth,
        VehicleDefinition ? VehicleDefinition->MaxHealth : 100,
        *UEnum::GetValueAsString(DamageState));
}

void UStrategyVehicle::UpdateDamageStateFromHealth()
{
    if (!VehicleDefinition || VehicleDefinition->MaxHealth <= 0)
    {
        DamageState = (CurrentHealth <= 0) ? EVehicleDamageState::Destroyed : EVehicleDamageState::Undamaged;
        return;
    }

    float HealthPercent = (float)CurrentHealth / VehicleDefinition->MaxHealth;

    if (HealthPercent <= 0.0f)
        DamageState = EVehicleDamageState::Destroyed;
    else if (HealthPercent <= 0.3f)
        DamageState = EVehicleDamageState::HeavilyDamaged;
    else if (HealthPercent <= 0.7f)
        DamageState = EVehicleDamageState::LightlyDamaged;
    else
        DamageState = EVehicleDamageState::Undamaged;
}

bool UStrategyVehicle::NeedsRepair() const
{
    int32 MaxH = VehicleDefinition ? VehicleDefinition->MaxHealth : 100;
    bool bNeeds = CurrentHealth < MaxH || DamageState != EVehicleDamageState::Undamaged;

    if (bNeeds && CurrentHealth >= MaxH)
    {
        return false;
    }
    return bNeeds;
}

int32 UStrategyVehicle::GetMaxWeaponSlots() const
{
    return VehicleDefinition ? VehicleDefinition->MaxWeaponSlots : 2;
}

int32 UStrategyVehicle::GetMaxDefenseSlots() const
{
    return VehicleDefinition ? VehicleDefinition->MaxDefenseSlots : 1;
}

bool UStrategyVehicle::CanEquipWeapon(UItemDefinition* Weapon) const
{
    if (!Weapon || !Weapon->IsVehicleWeapon()) return false;
    return EquippedWeapons.Num() < GetMaxWeaponSlots();
}

bool UStrategyVehicle::EquipWeapon(UItemDefinition* Weapon)
{
    if (!CanEquipWeapon(Weapon)) return false;

    EquippedWeapons.Add(Weapon);
    WeaponAmmoCounts.Add(Weapon->MaxAmmo);

    UE_LOG(LogTemp, Display, TEXT("[VEHICLE] %s equipped weapon '%s' (ammo: %d)"),
        VehicleDefinition ? *VehicleDefinition->VehicleName.ToString() : TEXT("Unknown"),
        *Weapon->ItemName.ToString(), Weapon->MaxAmmo);
    return true;
}

bool UStrategyVehicle::EquipDefenseSystem(UItemDefinition* DefenseItem)
{
    if (!DefenseItem || !DefenseItem->IsVehicleDefense() || EquippedDefenseSystems.Num() >= GetMaxDefenseSlots())
        return false;

    EquippedDefenseSystems.Add(DefenseItem);
    UE_LOG(LogTemp, Display, TEXT("[VEHICLE] %s equipped defense system '%s'"),
        VehicleDefinition ? *VehicleDefinition->VehicleName.ToString() : TEXT("Unknown"),
        *DefenseItem->ItemName.ToString());
    return true;
}

TArray<UItemDefinition*> UStrategyVehicle::GetEquippedWeapons() const
{
    TArray<UItemDefinition*> Result;
    for (const TSoftObjectPtr<UItemDefinition>& Ptr : EquippedWeapons)
    {
        if (UItemDefinition* Item = Ptr.Get()) Result.Add(Item);
    }
    return Result;
}

int32 UStrategyVehicle::GetVehicleOffensiveRating() const
{
    int32 Rating = VehicleDefinition ? VehicleDefinition->AttackPower : 0;

    for (int32 i = 0; i < EquippedWeapons.Num(); ++i)
    {
        if (UItemDefinition* Weapon = EquippedWeapons[i].Get())
        {
            Rating += Weapon->VehicleDamageBonus;
            if (WeaponAmmoCounts.IsValidIndex(i) && Weapon->MaxAmmo > 0)
                Rating += (WeaponAmmoCounts[i] * 5);
        }
    }
    return Rating;
}

int32 UStrategyVehicle::GetVehicleDefensiveRating() const
{
    int32 Rating = 0;
    for (const TSoftObjectPtr<UItemDefinition>& Ptr : EquippedDefenseSystems)
    {
        if (UItemDefinition* Def = Ptr.Get())
            Rating += Def->VehicleDefenseBonus;
    }
    return Rating;
}

void UStrategyVehicle::InitializeParkedAtBase()
{
    if (HomeBase)
    {
        CurrentPosition = HomeBase->MapLocation;
    }

    CurrentPhase = EVehicleMissionPhase::Docked;
    if (CurrentMission == nullptr)
    {
        CurrentBehavior = EVehicleBehavior::Idle;
    }

    CurrentWaypoints.Empty();
    ReturningWaypoints.Empty();
    ReturningDistanceTraveled = 0.0f;
    ReturningPathLength = 0.0f;
    PlannedRoundTripRange = 0.0f;
    RangeTraveledThisMission = 0.0f;
}

void UStrategyVehicle::DockAtHomeHangar()
{
    if (HomeBase)
    {
        CurrentPosition = HomeBase->MapLocation;
    }

    CurrentPhase = EVehicleMissionPhase::Docked;
    CurrentBehavior = EVehicleBehavior::Idle;
    CurrentTargetVehicle = nullptr;
    CombatBehaviorStartTime = -1.0f;

    CurrentWaypoints.Empty();
    ReturningWaypoints.Empty();
    ReturningDistanceTraveled = 0.0f;
    ReturningPathLength = 0.0f;
    TotalTravelTimeHours = 0.0f;
    OutboundTravelTime = 0.0f;
    ReturnTravelTime = 0.0f;
    SearchTimeAtTarget = 0.0f;
    PlannedRoundTripRange = 0.0f;
    RangeTraveledThisMission = 0.0f;

    CurrentRangeLeft = GetMaxRange();

    if (HomeHanger)
    {
        CurrentHanger = HomeHanger;
        HomeHanger->ParkedVehicles.AddUnique(this);
    }

    ActiveSalvageSite = nullptr;

    UE_LOG(LogTemp, Verbose, TEXT("[VEHICLE] %s docked at base '%s'"),
        VehicleDefinition ? *VehicleDefinition->VehicleName.ToString() : *GetNameSafe(this),
        HomeBase ? *HomeBase->BaseName.ToString() : TEXT("Unknown"));
}

void UStrategyVehicle::BeginMissionMovement(FVector2D TargetLocation, float CurrentGameHours, float SearchHoursAtTarget, EMissionType MissionType)
{
    if (!HomeBase) return;

    if (HomeBase->MapLocation.IsNearlyZero(10.f) || TargetLocation.IsNearlyZero(10.f))
    {
        UE_LOG(LogTemp, Warning, TEXT("[VEHICLE] %s refused mission movement — invalid origin (%.0f,%.0f) or target (%.0f,%.0f)"),
            VehicleDefinition ? *VehicleDefinition->VehicleName.ToString() : *GetNameSafe(this),
            HomeBase->MapLocation.X, HomeBase->MapLocation.Y, TargetLocation.X, TargetLocation.Y);
        return;
    }

    CurrentPosition = HomeBase->MapLocation;
    CurrentWaypoints.Empty();
    CurrentWaypoints.Add(HomeBase->MapLocation);
    CurrentWaypoints.Add(TargetLocation);
    CurrentWaypoints.Add(HomeBase->MapLocation);

    LaunchGameTimeHours = CurrentGameHours;
    LastPingGameTimeHours = CurrentGameHours;

    float DistOutbound = FVector2D::Distance(HomeBase->MapLocation, TargetLocation);
    PlannedRoundTripRange = DistOutbound * 2.0f;
    RangeTraveledThisMission = 0.0f;
    OutboundTravelTime = DistOutbound / GetCruiseSpeed();
    ReturnTravelTime = OutboundTravelTime;
    SearchTimeAtTarget = SearchHoursAtTarget;
    TotalTravelTimeHours = OutboundTravelTime + SearchTimeAtTarget + ReturnTravelTime;

    CurrentPhase = EVehicleMissionPhase::EnRoute;
    ReturningWaypoints.Empty();
    ReturningDistanceTraveled = 0.0f;
    ReturningPathLength = 0.0f;
    CombatBehaviorStartTime = -1.0f;
    CurrentTargetVehicle = nullptr;
    SalvageExtractedThisMission = FResourceStockpile();
    ActiveSalvageSite = nullptr;

    switch (MissionType)
    {
    case EMissionType::Recon:
        CurrentBehavior = EVehicleBehavior::Scouting;
        break;
    case EMissionType::Salvage:
        CurrentBehavior = EVehicleBehavior::Scouting;
        {
            UGameInstance* GI = GetTypedOuter<UGameInstance>();
            if (!GI)
            {
                if (UWorld* World = GetWorld())
                {
                    GI = World->GetGameInstance();
                }
            }
            if (GI)
            {
                if (UBaseManagerSubsystem* BaseMgr = GI->GetSubsystem<UBaseManagerSubsystem>())
                {
                    ActiveSalvageSite = BaseMgr->FindSiteAtLocation(TargetLocation);
                }
            }
        }
        break;
    case EMissionType::Interception:
    case EMissionType::Offensive:
        CurrentBehavior = EVehicleBehavior::Attacking;
        break;
    case EMissionType::Defensive:
        CurrentBehavior = EVehicleBehavior::Patrolling;
        break;
    default:
        CurrentBehavior = EVehicleBehavior::Scouting;
        break;
    }

    UE_LOG(LogTemp, Verbose, TEXT("[VEHICLE] %s began %s movement — out: %.1f hrs | search: %.1f hrs | return: %.1f hrs"),
        VehicleDefinition ? *VehicleDefinition->VehicleName.ToString() : *GetNameSafe(this),
        *UEnum::GetValueAsString(MissionType),
        OutboundTravelTime, SearchTimeAtTarget, ReturnTravelTime);
}

void UStrategyVehicle::LaunchScoutingMission(FVector2D TargetLocation, float CurrentGameHours, float SearchHoursAtTarget)
{
    BeginMissionMovement(TargetLocation, CurrentGameHours, SearchHoursAtTarget, EMissionType::Recon);
}

void UStrategyVehicle::UpdatePhaseFromPathProgress(float Progress)
{
    if (TotalTravelTimeHours <= 0.0f || CurrentWaypoints.Num() < 3)
    {
        return;
    }

    float MovingTime = Progress * TotalTravelTimeHours;

    if (MovingTime <= OutboundTravelTime)
    {
        CurrentPhase = EVehicleMissionPhase::EnRoute;
    }
    else if (MovingTime <= OutboundTravelTime + SearchTimeAtTarget)
    {
        CurrentPhase = EVehicleMissionPhase::OnStation;
    }
    else
    {
        CurrentPhase = EVehicleMissionPhase::EnRoute;
    }
}

void UStrategyVehicle::TickRadarPings(float CurrentGameHours)
{
    while (CurrentGameHours >= LastPingGameTimeHours + PingIntervalHours)
    {
        LastPingGameTimeHours += PingIntervalHours;
        PerformRadarPing();
    }
}

float UStrategyVehicle::GetReturningPathLength() const
{
    if (ReturningWaypoints.Num() < 2)
    {
        return 0.0f;
    }

    float TotalLength = 0.0f;
    for (int32 i = 0; i < ReturningWaypoints.Num() - 1; ++i)
    {
        TotalLength += FVector2D::Distance(ReturningWaypoints[i], ReturningWaypoints[i + 1]);
    }
    return TotalLength;
}

FVector2D UStrategyVehicle::GetPositionOnReturningPath(float DistanceAlongPath) const
{
    if (ReturningWaypoints.Num() < 2)
    {
        return CurrentPosition;
    }

    float Remaining = DistanceAlongPath;
    for (int32 i = 0; i < ReturningWaypoints.Num() - 1; ++i)
    {
        const float SegmentLength = FVector2D::Distance(ReturningWaypoints[i], ReturningWaypoints[i + 1]);
        if (SegmentLength <= 0.0f)
        {
            continue;
        }

        if (Remaining <= SegmentLength)
        {
            const float T = Remaining / SegmentLength;
            return FMath::Lerp(ReturningWaypoints[i], ReturningWaypoints[i + 1], T);
        }

        Remaining -= SegmentLength;
    }

    return ReturningWaypoints.Last();
}

void UStrategyVehicle::ConsumeMissionRange(float Distance)
{
    if (Distance <= 0.0f)
    {
        return;
    }

    RangeTraveledThisMission += Distance;
}

bool UStrategyVehicle::HasExceededMissionRangeBudget() const
{
    return PlannedRoundTripRange > 0.0f && RangeTraveledThisMission > PlannedRoundTripRange * 1.05f;
}

void UStrategyVehicle::AdvanceReturningMovement(float DeltaGameHours)
{
    if (!HomeBase)
    {
        CurrentPhase = EVehicleMissionPhase::Docked;
        CurrentBehavior = EVehicleBehavior::Idle;
        return;
    }

    if (ReturningWaypoints.Num() == 0)
    {
        GenerateReturnPath();
    }

    if (ReturningWaypoints.Num() < 2 || ReturningPathLength <= 0.0f)
    {
        const FVector2D Direction = (HomeBase->MapLocation - CurrentPosition).GetSafeNormal();
        const FVector2D PreviousPosition = CurrentPosition;
        CurrentPosition += Direction * GetCruiseSpeed() * DeltaGameHours;
        ConsumeMissionRange(FVector2D::Distance(PreviousPosition, CurrentPosition));

        if (FVector2D::Distance(CurrentPosition, HomeBase->MapLocation) <= GetCruiseSpeed() * DeltaGameHours + 1.0f)
        {
            DockAtHomeHangar();
        }
        return;
    }

    const float PreviousReturningDistance = ReturningDistanceTraveled;
    ReturningDistanceTraveled += GetCruiseSpeed() * DeltaGameHours;
    ConsumeMissionRange(ReturningDistanceTraveled - PreviousReturningDistance);

    if (ReturningDistanceTraveled >= ReturningPathLength)
    {
        DockAtHomeHangar();
        return;
    }

    CurrentPosition = GetPositionOnReturningPath(ReturningDistanceTraveled);
}

void UStrategyVehicle::UpdatePositionAndPings(float CurrentGameHours, float DeltaGameHours)
{
    if (CurrentMission == nullptr || CurrentPhase == EVehicleMissionPhase::Docked)
    {
        return;
    }

    if (DeltaGameHours <= 0.0f)
    {
        return;
    }

    // === COMBAT ===
    if (CurrentPhase == EVehicleMissionPhase::Combat && CurrentTargetVehicle.IsValid())
    {
        UStrategyVehicle* Target = CurrentTargetVehicle.Get();

        if (CombatBehaviorStartTime < 0.0f)
        {
            CombatBehaviorStartTime = CurrentGameHours;
        }

        ProcessCombatTick(DeltaGameHours);

        const FVector2D Direction = (CurrentBehavior == EVehicleBehavior::Attacking)
            ? (Target->CurrentPosition - CurrentPosition).GetSafeNormal()
            : (CurrentPosition - Target->CurrentPosition).GetSafeNormal();

        if (!Direction.IsNearlyZero())
        {
            const FVector2D PreviousPosition = CurrentPosition;
            CurrentPosition += Direction * GetCruiseSpeed() * DeltaGameHours;
            ConsumeMissionRange(FVector2D::Distance(PreviousPosition, CurrentPosition));
        }

        TickRadarPings(CurrentGameHours);

        if (IsDestroyed())
        {
            return;
        }

        bool bShouldReturn = false;

        const bool bMutualCombat = Target->CurrentBehavior == EVehicleBehavior::Attacking
            && Target->CurrentTargetVehicle.Get() == this;
        const float CombatDuration = CurrentGameHours - CombatBehaviorStartTime;
        const float CombatTimeoutHours = bMutualCombat ? 6.0f : 2.0f;
        if (CombatBehaviorStartTime >= 0.0f && CombatDuration >= CombatTimeoutHours)
        {
            bShouldReturn = true;
        }

        if (HasExceededMissionRangeBudget())
        {
            UE_LOG(LogTemp, Display, TEXT("[VEHICLE] %s returning — mission range budget exceeded (%.0f / %.0f)"),
                VehicleDefinition ? *VehicleDefinition->VehicleName.ToString() : *GetNameSafe(this),
                RangeTraveledThisMission, PlannedRoundTripRange);
            bShouldReturn = true;
        }

        if (HomeBase)
        {
            const float DistanceFromHome = FVector2D::Distance(CurrentPosition, HomeBase->MapLocation);
            const float MaxOutbound = PlannedRoundTripRange > 0.0f
                ? PlannedRoundTripRange * 0.5f
                : GetMaxRange() * 0.45f;
            if (DistanceFromHome > MaxOutbound)
            {
                bShouldReturn = true;
            }
        }

        if (bShouldReturn)
        {
            SetBehavior(EVehicleBehavior::Returning);
        }

        return;
    }

    // === RETURNING ===
    if (CurrentPhase == EVehicleMissionPhase::Returning)
    {
        AdvanceReturningMovement(DeltaGameHours);
        TickRadarPings(CurrentGameHours);
        return;
    }

    // === NORMAL MISSION PATHING ===
    const float Elapsed = CurrentGameHours - LaunchGameTimeHours;
    const float Progress = (TotalTravelTimeHours > 0.0f)
        ? FMath::Clamp(Elapsed / TotalTravelTimeHours, 0.0f, 1.0f)
        : 0.0f;

    const EVehicleMissionPhase PreviousPhase = CurrentPhase;
    UpdatePhaseFromPathProgress(Progress);

    if (PreviousPhase != EVehicleMissionPhase::OnStation
        && CurrentPhase == EVehicleMissionPhase::OnStation
        && CurrentMission
        && CurrentMission->MissionType == EMissionType::Offensive)
    {
        if (UMissionManagerSubsystem* MissionMgr = GetMissionManagerForVehicle(this))
        {
            MissionMgr->HandleBaseAttackArrival(this, CurrentMission);
        }
    }

    const FVector2D PreviousPosition = CurrentPosition;
    CurrentPosition = GetPositionOnPath(Progress);
    ConsumeMissionRange(FVector2D::Distance(PreviousPosition, CurrentPosition));

    if (HasExceededMissionRangeBudget())
    {
        UE_LOG(LogTemp, Display, TEXT("[VEHICLE] %s returning — exceeded planned round-trip range (%.0f / %.0f)"),
            VehicleDefinition ? *VehicleDefinition->VehicleName.ToString() : *GetNameSafe(this),
            RangeTraveledThisMission, PlannedRoundTripRange);
        SetBehavior(EVehicleBehavior::Returning);
        TickRadarPings(CurrentGameHours);
        return;
    }

    if (CurrentPhase == EVehicleMissionPhase::OnStation
        && CurrentMission
        && CurrentMission->MissionType == EMissionType::Salvage)
    {
        if (!ProcessSalvageExtractionTick(DeltaGameHours))
        {
            SetBehavior(EVehicleBehavior::Returning);
        }
    }

    TickRadarPings(CurrentGameHours);

    if (Progress >= 1.0f)
    {
        UE_LOG(LogTemp, Verbose, TEXT("[VEHICLE] %s mission path complete — docking"),
            VehicleDefinition ? *VehicleDefinition->VehicleName.ToString() : *GetNameSafe(this));
        DockAtHomeHangar();
    }
}

bool UStrategyVehicle::ProcessSalvageExtractionTick(float DeltaGameHours)
{
    if (DeltaGameHours <= 0.0f || !HomeBase || !CurrentMission
        || CurrentMission->MissionType != EMissionType::Salvage)
    {
        return true;
    }

    UGameInstance* GI = GetTypedOuter<UGameInstance>();
    if (!GI)
    {
        if (UWorld* World = GetWorld())
        {
            GI = World->GetGameInstance();
        }
    }
    if (!GI)
    {
        return true;
    }

    UBaseManagerSubsystem* BaseMgr = GI->GetSubsystem<UBaseManagerSubsystem>();
    UResourceManagerSubsystem* ResourceMgr = GI->GetSubsystem<UResourceManagerSubsystem>();
    if (!BaseMgr || !ResourceMgr)
    {
        return true;
    }

    UStrategySiteDefinition* Site = ActiveSalvageSite;
    if (!Site)
    {
        Site = BaseMgr->FindSiteAtLocation(CurrentPosition);
        ActiveSalvageSite = Site;
    }

    const EFactionType SalvagingFaction = HomeBase->OwningFaction;
    if (!Site || !BaseMgr->CanSalvageSite(SalvagingFaction, Site, this))
    {
        return false;
    }

    float EstimatedHours = 4.0f;
    float Efficiency = 1.0f;
    if (UStrategyCampaignSubsystem* Campaign = GI->GetSubsystem<UStrategyCampaignSubsystem>())
    {
        EstimatedHours = FMath::Max(0.5f, Campaign->SalvageOnStationHours);
        Efficiency = FMath::Max(0.1f, Campaign->SalvageEfficiencyMultiplier);
    }

    const float HourlyRate = (0.25f / EstimatedHours) * Efficiency;
    const float TransferScale = HourlyRate * DeltaGameHours;

    FResourceStockpile Extracted;
    auto TransferField = [&](int32& SiteValue, int32& ExtractedValue)
    {
        const int32 Amount = FMath::Clamp(
            FMath::RoundToInt(static_cast<float>(SiteValue) * TransferScale),
            0,
            SiteValue);
        SiteValue -= Amount;
        ExtractedValue = Amount;
    };

    TransferField(Site->CurrentResources.Money, Extracted.Money);
    TransferField(Site->CurrentResources.Metals, Extracted.Metals);
    TransferField(Site->CurrentResources.Chemicals, Extracted.Chemicals);
    TransferField(Site->CurrentResources.Biologicals, Extracted.Biologicals);
    TransferField(Site->CurrentResources.ExoticMaterial, Extracted.ExoticMaterial);

    if (Extracted.Money > 0 || Extracted.Metals > 0 || Extracted.Chemicals > 0
        || Extracted.Biologicals > 0 || Extracted.ExoticMaterial > 0)
    {
        ResourceMgr->AddResources(SalvagingFaction, Extracted);
        SalvageExtractedThisMission.Add(Extracted);

        UE_LOG(LogTemp, Verbose, TEXT("[SALVAGE] %s extracted M:%d Mt:%d Chem:%d from '%s' (wreck M:%d Mt:%d Chem:%d remaining)"),
            VehicleDefinition ? *VehicleDefinition->VehicleName.ToString() : *GetNameSafe(this),
            Extracted.Money, Extracted.Metals, Extracted.Chemicals,
            *Site->SiteName,
            Site->CurrentResources.Money, Site->CurrentResources.Metals, Site->CurrentResources.Chemicals);
    }

    if (Site->CurrentResources.IsEmpty())
    {
        Site->SalvageState = ESalvageSiteState::Depleted;
        Site->bHasBeenUsed = true;

        if (USoldierManagerSubsystem* SoldierMgr = GI->GetSubsystem<USoldierManagerSubsystem>())
        {
            if (Site->WreckOwnerFaction == SalvagingFaction)
            {
                SoldierMgr->RescueMIAsFromWreck(SalvagingFaction, Site, HomeBase);
            }
            else
            {
                SoldierMgr->ProcessMIAsOnOpposingSalvage(SalvagingFaction, Site);
            }
        }

        BaseMgr->RemoveSalvageSite(Site, false, SalvagingFaction);
        ActiveSalvageSite = nullptr;

        UE_LOG(LogTemp, Display, TEXT("[SALVAGE] Wreck '%s' depleted by %s — removed from map"),
            *Site->SiteName, *UEnum::GetValueAsString(SalvagingFaction));

        return false;
    }

    return true;
}

void UStrategyVehicle::PerformRadarPing()
{
    if (!HomeBase) return;

    EFactionType VehicleFaction = HomeBase->OwningFaction;

    UGameInstance* GI = GetTypedOuter<UGameInstance>();
    if (!GI)
    {
        if (UWorld* World = GetWorld())
            GI = World->GetGameInstance();
    }
    if (!GI) return;

    if (UBaseManagerSubsystem* BaseManager = GI->GetSubsystem<UBaseManagerSubsystem>())
    {
        for (UStrategySiteDefinition* Site : BaseManager->AllPotentialSites)
        {
            if (!Site || Site->bHasBeenUsed) continue;

            if (FVector2D::Distance(Site->Location, CurrentPosition) <= GetRadarRange())
            {
                const TArray<UStrategySiteDefinition*>& DiscoveredList =
                    (VehicleFaction == EFactionType::Human) ?
                    BaseManager->DiscoveredSitesHuman : BaseManager->DiscoveredSitesEnemy;

                if (!DiscoveredList.Contains(Site))
                {
                    BaseManager->AddDiscoveredSite(VehicleFaction, Site);
                    OnSiteDetected.Broadcast(VehicleFaction, Site);
                }
            }
        }

        TArray<UStrategyBase*> EnemyBases = BaseManager->GetBases(
            (VehicleFaction == EFactionType::Human) ? EFactionType::Enemy : EFactionType::Human);

        for (UStrategyBase* OtherBase : EnemyBases)
        {
            if (!OtherBase) continue;

            for (UStrategyFacility* Facility : OtherBase->Facilities)
            {
                if (!Facility || Facility->BuildProgressDays > 0) continue;

                if (Facility->FacilityDefinition &&
                    Facility->FacilityDefinition->FacilityType == EFacilityType::Hanger)
                {
                    for (UStrategyVehicle* OtherVehicle : Facility->ParkedVehicles)
                    {
                        TryDetectVehicle(OtherVehicle);
                    }
                }
            }
        }

        if (UMissionManagerSubsystem* MissionMgr = GI->GetSubsystem<UMissionManagerSubsystem>())
        {
            for (UMissionGroup* Mission : MissionMgr->ActiveMissions)
            {
                if (!Mission) continue;

                if (Mission->OriginBase && Mission->OriginBase->OwningFaction == VehicleFaction)
                    continue;

                for (UStrategyVehicle* OtherVehicle : Mission->VehiclesInFleet)
                {
                    TryDetectVehicle(OtherVehicle);
                }
            }
        }
    }
}

FVector2D UStrategyVehicle::GetPositionOnPath(float Progress) const
{
    if (CurrentWaypoints.Num() < 3) return CurrentPosition;

    float TravelPortion = OutboundTravelTime + ReturnTravelTime;
    if (TravelPortion <= 0.0f) return CurrentWaypoints[1];

    float MovingTime = Progress * TotalTravelTimeHours;

    if (MovingTime <= OutboundTravelTime)
    {
        float t = OutboundTravelTime > 0.0f ? MovingTime / OutboundTravelTime : 1.0f;
        return FMath::Lerp(CurrentWaypoints[0], CurrentWaypoints[1], t);
    }
    else if (MovingTime <= OutboundTravelTime + SearchTimeAtTarget)
    {
        return CurrentWaypoints[1];
    }
    else
    {
        float ReturnElapsed = MovingTime - (OutboundTravelTime + SearchTimeAtTarget);
        float t = ReturnTravelTime > 0.0f ? ReturnElapsed / ReturnTravelTime : 1.0f;
        return FMath::Lerp(CurrentWaypoints[1], CurrentWaypoints[2], t);
    }
}

bool UStrategyVehicle::IsMissionComplete(float CurrentGameHours) const
{
    return CurrentPhase == EVehicleMissionPhase::Docked && CurrentMission != nullptr;
}

float UStrategyVehicle::GetRadarRange() const
{
    if (VehicleDefinition)
    {
        return VehicleDefinition->RadarRangePixels;
    }
    return 64.0f;
}

void UStrategyVehicle::TryDetectVehicle(UStrategyVehicle* OtherVehicle)
{
    if (!OtherVehicle || OtherVehicle == this) return;

    float Distance = FVector2D::Distance(OtherVehicle->CurrentPosition, CurrentPosition);
    if (Distance > GetRadarRange()) return;

    UGameInstance* GI = GetTypedOuter<UGameInstance>();
    if (!GI)
    {
        if (UWorld* World = GetWorld())
            GI = World->GetGameInstance();
    }

    float CurrentGameHours = 0.0f;
    if (GI)
    {
        if (UMissionManagerSubsystem* MissionMgr = GI->GetSubsystem<UMissionManagerSubsystem>())
        {
            CurrentGameHours = MissionMgr->GetCurrentGameHours();
        }
    }

    const TWeakObjectPtr<UStrategyVehicle> OtherKey(OtherVehicle);

    if (const float* LastDetected = LastDetectedGameHour.Find(OtherKey))
    {
        if (CurrentGameHours - *LastDetected < VehicleDetectionCooldownHours)
        {
            return;
        }
    }

    LastDetectedGameHour.Add(OtherKey, CurrentGameHours);
    OnVehicleDetected.Broadcast(this, OtherVehicle);
    HandleVehicleDetected(OtherVehicle);
}

void UStrategyVehicle::SetBehavior(EVehicleBehavior NewBehavior, UStrategyVehicle* Target)
{
    if (CurrentBehavior == NewBehavior && CurrentTargetVehicle.Get() == Target)
        return;

    EVehicleBehavior PreviousBehavior = CurrentBehavior;
    CurrentBehavior = NewBehavior;
    CurrentTargetVehicle = Target;

    UE_LOG(LogTemp, Display, TEXT("[VEHICLE] %s changed behavior: %s → %s"),
        *GetNameSafe(this),
        *UEnum::GetValueAsString(PreviousBehavior),
        *UEnum::GetValueAsString(NewBehavior));

    if (NewBehavior == EVehicleBehavior::Attacking || NewBehavior == EVehicleBehavior::Evading)
    {
        CurrentPhase = EVehicleMissionPhase::Combat;
        CombatBehaviorStartTime = -1.0f;
        CurrentWaypoints.Empty();
        UE_LOG(LogTemp, Display, TEXT("[VEHICLE] %s entering combat phase"), *GetNameSafe(this));
    }
    else if (NewBehavior == EVehicleBehavior::Returning)
    {
        CurrentPhase = EVehicleMissionPhase::Returning;
        ReturningDistanceTraveled = 0.0f;
        GenerateReturnPath();
    }
    else if (NewBehavior == EVehicleBehavior::Idle)
    {
        if (CurrentMission == nullptr)
        {
            CurrentPhase = EVehicleMissionPhase::Docked;
        }
    }
}

void UStrategyVehicle::HandleVehicleDetected(UStrategyVehicle* DetectedVehicle)
{
    if (!DetectedVehicle || DetectedVehicle == this) return;

    UGameInstance* GI = GetTypedOuter<UGameInstance>();
    if (!GI)
    {
        if (UWorld* World = GetWorld())
            GI = World->GetGameInstance();
    }

    if (GI)
    {
        if (UAIControllerSubsystem* AI = GI->GetSubsystem<UAIControllerSubsystem>())
        {
            AI->HandleVehicleDetection(this, DetectedVehicle);
        }
    }
}

static UMissionManagerSubsystem* GetMissionManagerForVehicle(UStrategyVehicle* Vehicle)
{
    if (!Vehicle) return nullptr;

    UGameInstance* GI = Vehicle->GetTypedOuter<UGameInstance>();
    if (!GI)
    {
        if (UWorld* World = Vehicle->GetWorld())
        {
            GI = World->GetGameInstance();
        }
    }
    return GI ? GI->GetSubsystem<UMissionManagerSubsystem>() : nullptr;
}

void UStrategyVehicle::ProcessCombatTick(float DeltaGameHours)
{
    if (CurrentPhase != EVehicleMissionPhase::Combat || !CurrentTargetVehicle.IsValid() || IsDestroyed())
    {
        return;
    }

    UStrategyVehicle* Target = CurrentTargetVehicle.Get();
    if (!Target || Target->IsDestroyed())
    {
        SetBehavior(EVehicleBehavior::Returning);
        return;
    }

    const float CombatRange = GetRadarRange();
    const float Distance = FVector2D::Distance(CurrentPosition, Target->CurrentPosition);
    if (Distance > CombatRange * 1.5f)
    {
        return;
    }

    const float DamageScale = 0.35f;
    UMissionManagerSubsystem* MissionMgr = GetMissionManagerForVehicle(this);

    if (CurrentBehavior == EVehicleBehavior::Attacking)
    {
        const int32 DamageDealt = FMath::Max(1, FMath::RoundToInt(GetVehicleOffensiveRating() * DamageScale * DeltaGameHours));
        Target->ApplyDamage(DamageDealt);

        if (Target->IsDestroyed() && MissionMgr)
        {
            MissionMgr->HandleVehicleDestroyedInCombat(Target);
        }
    }

    if (Target->CurrentBehavior == EVehicleBehavior::Attacking && Target->CurrentTargetVehicle.Get() == this)
    {
        const int32 DamageReceived = FMath::Max(1, FMath::RoundToInt(Target->GetVehicleOffensiveRating() * DamageScale * DeltaGameHours));
        ApplyDamage(DamageReceived);

        if (IsDestroyed() && MissionMgr)
        {
            MissionMgr->HandleVehicleDestroyedInCombat(this);
        }
    }
}

void UStrategyVehicle::GenerateReturnPath()
{
    ReturningWaypoints.Empty();
    ReturningDistanceTraveled = 0.0f;

    if (!HomeBase) return;

    FVector2D Start = CurrentPosition;
    FVector2D End = HomeBase->MapLocation;

    const int32 NumWaypoints = 4;
    for (int32 i = 0; i <= NumWaypoints; i++)
    {
        float Alpha = (float)i / (float)NumWaypoints;
        ReturningWaypoints.Add(FMath::Lerp(Start, End, Alpha));
    }

    ReturningPathLength = GetReturningPathLength();

    UE_LOG(LogTemp, Display, TEXT("[VEHICLE] %s generated return path (%.0f px)"),
        *GetNameSafe(this), ReturningPathLength);
}
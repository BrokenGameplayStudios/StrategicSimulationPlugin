#include "UStrategyVehicle.h"
#include "UStrategyBase.h"
//#include "UStrategyCampaignSubsystem.h"
#include "UAIControllerSubsystem.h"
#include "UStrategyFacility.h"
#include "UMissionGroup.h"
#include "UFacilityDefinition.h"
#include "UItemDefinition.h"
#include "UBaseManagerSubsystem.h"
#include "UMissionManagerSubsystem.h"
#include "StrategicSiteDefinition.h"
#include "Engine/Engine.h"

UStrategyVehicle::UStrategyVehicle()
{
    CurrentRangeLeft = 0.0f;
    CurrentHealth = 100;
    DamageState = EVehicleDamageState::Undamaged;

    // Movement defaults
    CurrentPosition = FVector2D::ZeroVector;
    CurrentWaypoints.Empty();
    LaunchGameTimeHours = 0.0f;
    TotalTravelTimeHours = 0.0f;
    LastPingGameTimeHours = 0.0f;
    CruiseSpeedPixelsPerHour = 180.0f;
    PingIntervalHours = 0.5f;
}

float UStrategyVehicle::GetMaxRange() const
{
    return VehicleDefinition ? VehicleDefinition->MaxRange : 800.0f;
}

bool UStrategyVehicle::HasEnoughRangeForMission(float RequiredDistance) const
{
    return CurrentRangeLeft >= RequiredDistance;
}

void UStrategyVehicle::ApplyDamage(int32 DamageAmount)
{
    if (DamageAmount <= 0) return;

    CurrentHealth = FMath::Max(0, CurrentHealth - DamageAmount);
    UpdateDamageStateFromHealth();

    UE_LOG(LogTemp, Warning, TEXT("[VEHICLE] %s took %d damage → Health: %d/%d (%s)"),
        *VehicleDefinition->VehicleName.ToString(),
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
        UE_LOG(LogTemp, Warning, TEXT("[VEHICLE] %s NeedsRepair() returned true even at full health! Forcing false."), *VehicleDefinition->VehicleName.ToString());
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
        *VehicleDefinition->VehicleName.ToString(), *Weapon->ItemName.ToString(), Weapon->MaxAmmo);
    return true;
}

bool UStrategyVehicle::EquipDefenseSystem(UItemDefinition* DefenseItem)
{
    if (!DefenseItem || !DefenseItem->IsVehicleDefense() || EquippedDefenseSystems.Num() >= GetMaxDefenseSlots())
        return false;

    EquippedDefenseSystems.Add(DefenseItem);
    UE_LOG(LogTemp, Display, TEXT("[VEHICLE] %s equipped defense system '%s'"),
        *VehicleDefinition->VehicleName.ToString(), *DefenseItem->ItemName.ToString());
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

// ===========================================================================
// Updated LaunchScoutingMission — now uses realistic scale + 1 hour at target
// ===========================================================================
void UStrategyVehicle::LaunchScoutingMission(FVector2D TargetLocation, float CurrentGameHours, float SearchHoursAtTarget)
{
    if (!HomeBase) return;

    CurrentPosition = HomeBase->MapLocation;
    CurrentWaypoints.Empty();
    CurrentWaypoints.Add(HomeBase->MapLocation);      // 0 = Base
    CurrentWaypoints.Add(TargetLocation);             // 1 = Target
    CurrentWaypoints.Add(HomeBase->MapLocation);      // 2 = Base

    LaunchGameTimeHours = CurrentGameHours;
    LastPingGameTimeHours = CurrentGameHours;

    float DistOutbound = FVector2D::Distance(HomeBase->MapLocation, TargetLocation);
    OutboundTravelTime = DistOutbound / CruiseSpeedPixelsPerHour;
    ReturnTravelTime = OutboundTravelTime;           // assume same speed back
    SearchTimeAtTarget = SearchHoursAtTarget;

    TotalTravelTimeHours = OutboundTravelTime + SearchTimeAtTarget + ReturnTravelTime;

    UE_LOG(LogTemp, Display, TEXT("[VEHICLE] %s launched mission — travel out: %.1f hrs | search: %.1f hrs | return: %.1f hrs"),
        *VehicleDefinition->VehicleName.ToString(),
        OutboundTravelTime, SearchTimeAtTarget, ReturnTravelTime);
}

void UStrategyVehicle::UpdatePositionAndPings(float CurrentGameHours)
{
    if (CurrentMission == nullptr) return;

    float Elapsed = CurrentGameHours - LaunchGameTimeHours;
    float Progress = (TotalTravelTimeHours > 0.0f)
        ? FMath::Clamp(Elapsed / TotalTravelTimeHours, 0.0f, 1.0f)
        : 0.0f;

    FVector2D NewPosition;

    // =====================================================
    // ATTACKING / EVADING BEHAVIOR
    // =====================================================
    if ((CurrentBehavior == EVehicleBehavior::Attacking || CurrentBehavior == EVehicleBehavior::Evading)
        && CurrentTargetVehicle.IsValid())
    {
        UStrategyVehicle* Target = CurrentTargetVehicle.Get();

        if (CombatBehaviorStartTime < 0.0f)
            CombatBehaviorStartTime = CurrentGameHours;

        FVector2D Direction = (CurrentBehavior == EVehicleBehavior::Attacking)
            ? (Target->CurrentPosition - CurrentPosition)
            : (CurrentPosition - Target->CurrentPosition);

        if (!Direction.IsNearlyZero())
        {
            Direction.Normalize();
            NewPosition = CurrentPosition + Direction * 180.0f;
        }
        else
        {
            NewPosition = CurrentPosition;
        }

        CurrentPosition = NewPosition;

        while (CurrentGameHours >= LastPingGameTimeHours + PingIntervalHours)
        {
            LastPingGameTimeHours += PingIntervalHours;
            PerformRadarPing();
        }

        // Exit conditions
        bool bShouldReturn = false;

        if (CurrentGameHours - CombatBehaviorStartTime >= 1.0f)
        {
            bShouldReturn = true;
        }

        if (HomeBase)
        {
            float DistanceFromHome = FVector2D::Distance(CurrentPosition, HomeBase->MapLocation);
            float MaxDistance = VehicleDefinition ? VehicleDefinition->MaxRange * 0.9f : 700.0f;

            if (DistanceFromHome > MaxDistance)
            {
                bShouldReturn = true;
            }
        }

        if (bShouldReturn)
        {
            SetBehavior(EVehicleBehavior::Returning);
            CombatBehaviorStartTime = -1.0f;
        }

        return;
    }

    // =====================================================
    // RETURNING BEHAVIOR (Waypoint Pathfinding)
    // =====================================================
    else if (CurrentBehavior == EVehicleBehavior::Returning)
    {
        if (!HomeBase)
        {
            CurrentBehavior = EVehicleBehavior::Idle;
            return;
        }

        if (ReturningWaypoints.Num() == 0)
        {
            GenerateReturnPath();
        }

        if (ReturningWaypoints.Num() == 0)
        {
            // Fallback direct movement
            FVector2D Direction = HomeBase->MapLocation - CurrentPosition;
            if (!Direction.IsNearlyZero())
            {
                Direction.Normalize();
                CurrentPosition += Direction * 200.0f;
            }
        }
        else
        {
            ReturningProgress = FMath::Clamp(ReturningProgress + 0.012f, 0.0f, 1.0f);

            float TotalSegments = ReturningWaypoints.Num() - 1;
            float ScaledProgress = ReturningProgress * TotalSegments;
            int32 CurrentIndex = FMath::FloorToInt(ScaledProgress);
            float SegmentProgress = ScaledProgress - CurrentIndex;

            if (CurrentIndex >= ReturningWaypoints.Num() - 1)
            {
                CurrentPosition = HomeBase->MapLocation;

                UE_LOG(LogTemp, Display, TEXT("[VEHICLE] %s has RETURNED to base"), *GetNameSafe(this));

                CurrentBehavior = EVehicleBehavior::Idle;
                CurrentTargetVehicle = nullptr;
                CurrentMission = nullptr;           // Safe to clear here
                CurrentWaypoints.Empty();
                ReturningWaypoints.Empty();
                TotalTravelTimeHours = 0.0f;
                ReturningProgress = 0.0f;
                CombatBehaviorStartTime = -1.0f;

                if (HomeHanger)
                {
                    CurrentHanger = HomeHanger;
                    HomeHanger->ParkedVehicles.AddUnique(this);
                }
            }
            else
            {
                FVector2D Start = ReturningWaypoints[CurrentIndex];
                FVector2D End = ReturningWaypoints[CurrentIndex + 1];
                CurrentPosition = FMath::Lerp(Start, End, SegmentProgress);
            }
        }

        while (CurrentGameHours >= LastPingGameTimeHours + PingIntervalHours)
        {
            LastPingGameTimeHours += PingIntervalHours;
            PerformRadarPing();
        }

        return;
    }

    // =====================================================
    // NORMAL MISSION PATHING
    // =====================================================
    CurrentPosition = GetPositionOnPath(Progress);

    while (CurrentGameHours >= LastPingGameTimeHours + PingIntervalHours)
    {
        LastPingGameTimeHours += PingIntervalHours;
        PerformRadarPing();
    }

    // Only complete mission when NOT in any special behavior
    if (Progress >= 1.0f &&
        CurrentBehavior != EVehicleBehavior::Attacking &&
        CurrentBehavior != EVehicleBehavior::Evading &&
        CurrentBehavior != EVehicleBehavior::Returning)
    {
        // Only complete here for normal missions
        CurrentMission = nullptr;
        CurrentWaypoints.Empty();
        TotalTravelTimeHours = 0.0f;
        CurrentPosition = HomeBase ? HomeBase->MapLocation : CurrentPosition;
        CurrentBehavior = EVehicleBehavior::Idle;
        CurrentTargetVehicle = nullptr;
        ReturningWaypoints.Empty();
        CombatBehaviorStartTime = -1.0f;
    }
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
        // === SITE DETECTION ===
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
                    BaseManager->AddDiscoveredSite(VehicleFaction, Site->Location, Site->SiteType);
                    OnSiteDetected.Broadcast(VehicleFaction, Site);
                }
            }
        }

        // === VEHICLE DETECTION ===
        // 1. Check parked vehicles in enemy hangers
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

        // 2. Check vehicles currently on active missions
        if (UMissionManagerSubsystem* MissionMgr = GI->GetSubsystem<UMissionManagerSubsystem>())
        {
            for (UMissionGroup* Mission : MissionMgr->ActiveMissions)
            {
                if (!Mission) continue;

                // Only check enemy missions
                if (Mission->OriginBase && Mission->OriginBase->OwningFaction == VehicleFaction)
                    continue;

                for (UStrategyVehicle* OtherVehicle : Mission->VehiclesInFleet)
                {
                    TryDetectVehicle(OtherVehicle);
                }
            }
        }
    }

    // Simple cleanup
    if (RecentlyDetectedVehicles.Num() > 20)
    {
        RecentlyDetectedVehicles.Empty();
    }
}

FVector2D UStrategyVehicle::GetPositionOnPath(float Progress) const
{
    if (CurrentWaypoints.Num() < 3) return CurrentPosition;

    // Remap progress so we dwell at the target during search time
    float TravelPortion = OutboundTravelTime + ReturnTravelTime;
    if (TravelPortion <= 0.0f) return CurrentWaypoints[1]; // safety

    // Time spent moving vs searching
    float MovingTime = Progress * TotalTravelTimeHours;

    if (MovingTime <= OutboundTravelTime)
    {
        // Going to target (first segment)
        float t = MovingTime / OutboundTravelTime;
        return FMath::Lerp(CurrentWaypoints[0], CurrentWaypoints[1], t);
    }
    else if (MovingTime <= OutboundTravelTime + SearchTimeAtTarget)
    {
        // Waiting at target
        return CurrentWaypoints[1];
    }
    else
    {
        // Returning home (second segment)
        float ReturnElapsed = MovingTime - (OutboundTravelTime + SearchTimeAtTarget);
        float t = ReturnElapsed / ReturnTravelTime;
        return FMath::Lerp(CurrentWaypoints[1], CurrentWaypoints[2], t);
    }
}

bool UStrategyVehicle::IsMissionComplete(float CurrentGameHours) const
{
    if (TotalTravelTimeHours <= 0.0f) return true;
    float Elapsed = CurrentGameHours - LaunchGameTimeHours;
    return Elapsed >= TotalTravelTimeHours;
}

float UStrategyVehicle::GetRadarRange() const
{
    if (VehicleDefinition)
    {
        return VehicleDefinition->RadarRangePixels;
    }
    return 64.0f; // Safe fallback
}

void UStrategyVehicle::TryDetectVehicle(UStrategyVehicle* OtherVehicle)
{
    if (!OtherVehicle || OtherVehicle == this) return;

    float Distance = FVector2D::Distance(OtherVehicle->CurrentPosition, CurrentPosition);
    if (Distance > GetRadarRange()) return;

    // Check if we already detected this vehicle recently
    bool bAlreadyDetected = false;
    for (int32 i = RecentlyDetectedVehicles.Num() - 1; i >= 0; i--)
    {
        if (!RecentlyDetectedVehicles[i].IsValid())
        {
            RecentlyDetectedVehicles.RemoveAt(i);
            continue;
        }
        if (RecentlyDetectedVehicles[i].Get() == OtherVehicle)
        {
            bAlreadyDetected = true;
            break;
        }
    }

    if (!bAlreadyDetected)
    {
        RecentlyDetectedVehicles.Add(OtherVehicle);
        OnVehicleDetected.Broadcast(this, OtherVehicle);
        HandleVehicleDetected(OtherVehicle);
    }
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

    // Clear old returning path
    ReturningWaypoints.Empty();
    ReturningProgress = 0.0f;

    if (NewBehavior == EVehicleBehavior::Returning && HomeBase)
    {
        // Generate waypoints from current position back to home base
        GenerateReturnPath();
    }

    if (NewBehavior == EVehicleBehavior::Attacking || NewBehavior == EVehicleBehavior::Evading)
    {
        CurrentWaypoints.Empty(); // Interrupt normal mission
        UE_LOG(LogTemp, Display, TEXT("[VEHICLE] %s interrupting mission for combat behavior"),
            *GetNameSafe(this));
    }
}

void UStrategyVehicle::HandleVehicleDetected(UStrategyVehicle* DetectedVehicle)
{
    if (!DetectedVehicle || DetectedVehicle == this) return;

    OnVehicleDetected.Broadcast(this, DetectedVehicle);

    // Notify the AI Controller (this is where decision logic lives)
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

void UStrategyVehicle::GenerateReturnPath()
{
    ReturningWaypoints.Empty();

    if (!HomeBase) return;

    FVector2D Start = CurrentPosition;
    FVector2D End = HomeBase->MapLocation;

    // Create a simple path with a few waypoints (you can increase this for smoother paths)
    int32 NumWaypoints = 4;

    for (int32 i = 0; i <= NumWaypoints; i++)
    {
        float Alpha = (float)i / (float)NumWaypoints;
        FVector2D Waypoint = FMath::Lerp(Start, End, Alpha);
        ReturningWaypoints.Add(Waypoint);
    }

    ReturningProgress = 0.0f;

    UE_LOG(LogTemp, Display, TEXT("[VEHICLE] %s generated return path with %d waypoints"),
        *GetNameSafe(this), ReturningWaypoints.Num());
}
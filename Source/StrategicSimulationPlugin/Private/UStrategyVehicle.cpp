#include "UStrategyVehicle.h"
#include "UStrategyBase.h"
#include "UStrategyFacility.h"
#include "UMissionGroup.h"
#include "UFacilityDefinition.h"
#include "UItemDefinition.h"

UStrategyVehicle::UStrategyVehicle()
{
    CurrentRangeLeft = 0.0f;
    CurrentHealth = 100;
    DamageState = EVehicleDamageState::Undamaged;

    // NEW movement/radar defaults
    CurrentPosition = FVector2D::ZeroVector;
    CurrentWaypoints.Empty();
    LaunchGameTimeHours = 0.0f;
    TotalTravelTimeHours = 0.0f;
    LastPingGameTimeHours = 0.0f;
    CruiseSpeedPixelsPerHour = 250.0f;
    PingIntervalHours = 0.5f;
    PingRadiusPixels = 120.0f;
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
// NEW LIVE MOVEMENT + RADAR PING FUNCTIONS (full implementations)
// ===========================================================================

void UStrategyVehicle::LaunchScoutingMission(FVector2D TargetLocation, float CurrentGameHours, float SearchHoursAtTarget)
{
    if (!HomeBase)
    {
        UE_LOG(LogTemp, Error, TEXT("[VEHICLE] LaunchScoutingMission failed — no HomeBase!"));
        return;
    }

    CurrentPosition = HomeBase->MapLocation;
    CurrentWaypoints.Empty();
    CurrentWaypoints.Add(HomeBase->MapLocation);      // start at base
    CurrentWaypoints.Add(TargetLocation);              // fly to target
    CurrentWaypoints.Add(HomeBase->MapLocation);       // return to base

    LaunchGameTimeHours = CurrentGameHours;
    LastPingGameTimeHours = CurrentGameHours;

    float DistOutbound = FVector2D::Distance(HomeBase->MapLocation, TargetLocation);
    float TravelTimeHours = (DistOutbound * 2.0f) / CruiseSpeedPixelsPerHour;
    TotalTravelTimeHours = TravelTimeHours + SearchHoursAtTarget;

    UE_LOG(LogTemp, Display, TEXT("[VEHICLE] %s launched scouting mission to (%.0f,%.0f) — total travel time %.1f hours (search %.1f hrs)"),
        *VehicleDefinition->VehicleName.ToString(), TargetLocation.X, TargetLocation.Y, TotalTravelTimeHours, SearchHoursAtTarget);
}

void UStrategyVehicle::UpdatePositionAndPings(float CurrentGameHours)
{
    if (CurrentMission == nullptr || TotalTravelTimeHours <= 0.0f) return;

    float Elapsed = CurrentGameHours - LaunchGameTimeHours;
    float Progress = FMath::Clamp(Elapsed / TotalTravelTimeHours, 0.0f, 1.0f);

    CurrentPosition = GetPositionOnPath(Progress);

    // Periodic radar pings (overlapping OK, works at any time scale)
    while (CurrentGameHours >= LastPingGameTimeHours + PingIntervalHours)
    {
        LastPingGameTimeHours += PingIntervalHours;
        PerformRadarPing();
    }

    if (Progress >= 1.0f)
    {
        UE_LOG(LogTemp, Display, TEXT("[VEHICLE] %s mission COMPLETE — returned to base at (%.0f,%.0f)"),
            *VehicleDefinition->VehicleName.ToString(), CurrentPosition.X, CurrentPosition.Y);

        CurrentMission = nullptr;           // let MissionManager clean up the old daily system
        CurrentWaypoints.Empty();
        TotalTravelTimeHours = 0.0f;
        CurrentPosition = HomeBase ? HomeBase->MapLocation : CurrentPosition;
    }
}

void UStrategyVehicle::PerformRadarPing()
{
    // Simple discovery roll (tune chance later with zones/vehicle type)
    if (FMath::FRand() < 0.22f)  // ~22% per ping for visible testing
    {
        UE_LOG(LogTemp, Warning, TEXT("[RADAR PING SUCCESS] %s detected CONTACT at (%.0f, %.0f)!"),
            *VehicleDefinition->VehicleName.ToString(), CurrentPosition.X, CurrentPosition.Y);

        // TODO (next step): Fire your UStrategyEventDispatcher or OnStrategicEvent here
        // e.g. OnStrategicEvent.Broadcast(...);
    }
    // else silent ping (no log spam during fast sims)
}

FVector2D UStrategyVehicle::GetPositionOnPath(float Progress) const
{
    if (CurrentWaypoints.Num() < 2) return CurrentPosition;

    float SegmentProgress = Progress * (CurrentWaypoints.Num() - 1);
    int32 SegmentIdx = FMath::FloorToInt(SegmentProgress);
    float Frac = SegmentProgress - SegmentIdx;

    if (SegmentIdx >= CurrentWaypoints.Num() - 1) SegmentIdx = CurrentWaypoints.Num() - 2;

    FVector2D A = CurrentWaypoints[SegmentIdx];
    FVector2D B = CurrentWaypoints[SegmentIdx + 1];
    return FMath::Lerp(A, B, Frac);
}

bool UStrategyVehicle::IsMissionComplete(float CurrentGameHours) const
{
    if (TotalTravelTimeHours <= 0.0f) return true;
    float Elapsed = CurrentGameHours - LaunchGameTimeHours;
    return Elapsed >= TotalTravelTimeHours;
}
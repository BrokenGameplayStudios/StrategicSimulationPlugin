#include "UStrategyVehicle.h"
#include "UStrategyBase.h"
#include "UStrategyFacility.h"
#include "UMissionGroup.h"
#include "UFacilityDefinition.h"
#include "UItemDefinition.h"
#include "UBaseManagerSubsystem.h"
#include "StrategicSiteDefinition.h"

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
    CruiseSpeedPixelsPerHour = 180.0f;
    PingIntervalHours = 0.5f;

    // Radar range now comes from VehicleDefinition (fallback to 300)
    PingRadiusPixels = 300.0f;
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
    if (!HomeBase) return;

    EFactionType VehicleFaction = HomeBase->OwningFaction;

    // Get BaseManager safely without relying on GetWorld() on a UObject
    UGameInstance* GameInstance = nullptr;
    if (UWorld* World = GetWorld())
    {
        GameInstance = World->GetGameInstance();
    }
    if (!GameInstance)
    {
        GameInstance = Cast<UGameInstance>(GetOuter());
    }
    if (!GameInstance) return;

    if (UBaseManagerSubsystem* BaseManager = GameInstance->GetSubsystem<UBaseManagerSubsystem>())
    {
        for (UStrategySiteDefinition* Site : BaseManager->AllPotentialSites)
        {
            if (!Site || Site->bHasBeenUsed) continue;

            if (FVector2D::Distance(Site->Location, CurrentPosition) <= PingRadiusPixels)
            {
                BaseManager->AddDiscoveredSite(VehicleFaction, Site->Location, Site->SiteType);
            }
        }
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
    return PingRadiusPixels;
}
#include "AStrategyDebugHUD.h"
#include "Engine/Engine.h"
#include "UStrategyCampaignSubsystem.h"
#include "UTimeManagerSubsystem.h"
#include "UBaseManagerSubsystem.h"
#include "USoldierManagerSubsystem.h"

AStrategyDebugHUD::AStrategyDebugHUD()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AStrategyDebugHUD::BeginPlay()
{
    Super::BeginPlay();
    ToggleDebugHUD();
}

void AStrategyDebugHUD::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (!bDebugVisible || !GEngine) return;

    UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();
    if (!Campaign) return;

    FString DebugText = FString::Printf(TEXT(
        "=== STRATEGIC SIMULATION DEBUG ===\n"
        "DAY: %d\n"
        "Human Net Power: %d\n"
        "Enemy Net Power: %d\n"
        "Human Soldiers: %d | Enemy Soldiers: %d\n\n"),
        Campaign->GetTimeManager()->GetCurrentDay(),
        Campaign->GetBaseManager()->GetNetPower(EFactionType::Human),
        Campaign->GetBaseManager()->GetNetPower(EFactionType::Enemy),
        Campaign->GetSoldierManager()->GetRoster(EFactionType::Human).Num(),
        Campaign->GetSoldierManager()->GetRoster(EFactionType::Enemy).Num());

    // === NEW: Vehicle Weapon System Debug ===
    UBaseManagerSubsystem* BaseMgr = Campaign->GetBaseManager();
    if (BaseMgr)
    {
        DebugText += FString::Printf(TEXT("=== VEHICLE HARDPOINTS & LOADOUT ===\n"));

        // Human bases
        // Use the generic GetBases(...) method (takes EFactionType) instead of GetHumanBases/GetEnemyBases
        for (UStrategyBase* Base : BaseMgr->GetBases(EFactionType::Human))
        {
            if (!Base) continue;
            DebugText += FString::Printf(TEXT("HUMAN BASE '%s':\n"), *Base->BaseName.ToString());
            for (UStrategyFacility* Fac : Base->Facilities)
            {
                if (Fac && Fac->FacilityDefinition && Fac->FacilityDefinition->FacilityType == EFacilityType::Hanger)
                {
                    for (UStrategyVehicle* Vehicle : Fac->ParkedVehicles)
                    {
                        if (!Vehicle) continue;
                        DebugText += FString::Printf(TEXT("  %s [%d/%d weapons] Offensive:%d Defense:%d\n"),
                            *Vehicle->VehicleDefinition->VehicleName.ToString(),
                            Vehicle->GetEquippedWeapons().Num(),
                            Vehicle->GetMaxWeaponSlots(),
                            Vehicle->GetVehicleOffensiveRating(),
                            Vehicle->GetVehicleDefensiveRating());

                        for (int32 i = 0; i < Vehicle->GetEquippedWeapons().Num(); ++i)
                        {
                            if (UItemDefinition* Weapon = Vehicle->EquippedWeapons[i].Get())
                            {
                                int32 Ammo = Vehicle->WeaponAmmoCounts.IsValidIndex(i) ? Vehicle->WeaponAmmoCounts[i] : 0;
                                DebugText += FString::Printf(TEXT("    → %s (Ammo: %d/%d)\n"),
                                    *Weapon->ItemName.ToString(), Ammo, Weapon->MaxAmmo);
                            }
                        }
                    }
                }
            }
        }

        // Enemy bases (same info)
        for (UStrategyBase* Base : BaseMgr->GetBases(EFactionType::Enemy))
        {
            if (!Base) continue;
            DebugText += FString::Printf(TEXT("ENEMY BASE '%s':\n"), *Base->BaseName.ToString());
            for (UStrategyFacility* Fac : Base->Facilities)
            {
                if (Fac && Fac->FacilityDefinition && Fac->FacilityDefinition->FacilityType == EFacilityType::Hanger)
                {
                    for (UStrategyVehicle* Vehicle : Fac->ParkedVehicles)
                    {
                        if (!Vehicle) continue;
                        DebugText += FString::Printf(TEXT("  %s [%d/%d weapons] Offensive:%d Defense:%d\n"),
                            *Vehicle->VehicleDefinition->VehicleName.ToString(),
                            Vehicle->GetEquippedWeapons().Num(),
                            Vehicle->GetMaxWeaponSlots(),
                            Vehicle->GetVehicleOffensiveRating(),
                            Vehicle->GetVehicleDefensiveRating());

                        for (int32 i = 0; i < Vehicle->GetEquippedWeapons().Num(); ++i)
                        {
                            if (UItemDefinition* Weapon = Vehicle->EquippedWeapons[i].Get())
                            {
                                int32 Ammo = Vehicle->WeaponAmmoCounts.IsValidIndex(i) ? Vehicle->WeaponAmmoCounts[i] : 0;
                                DebugText += FString::Printf(TEXT("    → %s (Ammo: %d/%d)\n"),
                                    *Weapon->ItemName.ToString(), Ammo, Weapon->MaxAmmo);
                            }
                        }
                    }
                }
            }
        }
    }

    GEngine->AddOnScreenDebugMessage(999, 0.0f, FColor::Cyan, DebugText);
}

void AStrategyDebugHUD::ToggleDebugHUD()
{
    bDebugVisible = !bDebugVisible;
    UE_LOG(LogTemp, Display, TEXT("Debug HUD %s"), bDebugVisible ? TEXT("ENABLED") : TEXT("DISABLED"));
}
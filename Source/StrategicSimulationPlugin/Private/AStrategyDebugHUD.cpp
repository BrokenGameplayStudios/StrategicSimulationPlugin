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

    // === Vehicle Weapon System Debug + POWs ===
    UBaseManagerSubsystem* BaseMgr = Campaign->GetBaseManager();
    if (BaseMgr)
    {
        DebugText += FString::Printf(TEXT("=== VEHICLE HARDPOINTS & LOADOUT + PRISONERS ===\n"));

        // Human bases
        for (UStrategyBase* Base : BaseMgr->GetBases(EFactionType::Human))
        {
            if (!Base) continue;
            DebugText += FString::Printf(TEXT("HUMAN BASE '%s':\n"), *Base->BaseName.ToString());

            // Vehicles
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
                        
            // === POW / KIA DEBUG (updated for per-base system) ===
            if (Base->GetPOWCount() > 0 || Base->GetKIABodyCount() > 0)
            {
                DebugText += FString::Printf(TEXT("  [POW/KIA] %d POWs | %d KIA Bodies in this base\n"),
                    Base->GetPOWCount(), Base->GetKIABodyCount());
            }
        }

        // Enemy bases (same info)
        for (UStrategyBase* Base : BaseMgr->GetBases(EFactionType::Enemy))
        {
            if (!Base) continue;
            DebugText += FString::Printf(TEXT("ENEMY BASE '%s':\n"), *Base->BaseName.ToString());

            // Vehicles
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

            // === POW / Prisoners Debug ===
            if (Base->GetPOWCount() > 0)
            {
                DebugText += FString::Printf(TEXT("  [PRISONERS] %d captured soldiers held here!\n"), Base->GetPOWCount());
                for (UStrategySoldier* Prisoner : Base->ContainedPOWs   )
                {
                    if (Prisoner)
                        DebugText += FString::Printf(TEXT("    → %s (%s)\n"), *Prisoner->SoldierName, *Prisoner->ClassDefinition->ClassName.ToString());
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
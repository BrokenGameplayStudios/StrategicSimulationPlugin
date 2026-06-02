#include "UAIControllerSubsystem.h"
#include "UResourceManagerSubsystem.h"
#include "USoldierManagerSubsystem.h"
#include "USoldierClassDatabase.h"
#include "UEngineeringManagerSubsystem.h"
#include "UStrategyEventDispatcher.h"
#include "UItemDatabase.h"
#include "UStrategyCampaignSubsystem.h"
#include "UFacilityDatabase.h"
#include "UVehicleDatabase.h"
#include "UStrategyBase.h"
#include "UStrategyVehicle.h"
#include "UMissionManagerSubsystem.h"
#include "UBaseManagerSubsystem.h"
#include "Engine/Engine.h"

void UAIControllerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    if (UTimeManagerSubsystem* TimeMgr = GetGameInstance()->GetSubsystem<UTimeManagerSubsystem>())
    {
        TimeMgr->OnDayPassed.AddDynamic(this, &UAIControllerSubsystem::OnDayPassed);
        UE_LOG(LogTemp, Display, TEXT("✅ UAIControllerSubsystem — OnDayPassed bound successfully"));
    }

    UE_LOG(LogTemp, Display, TEXT("✅ UAIControllerSubsystem initialized — AI vs AI mode ACTIVE (Human + Enemy)"));
    UE_LOG(LogTemp, Display, TEXT("   Human AI: %s | Enemy AI: %s"),
        bSimulateHumanAI ? TEXT("ENABLED") : TEXT("DISABLED"),
        bSimulateEnemyAI ? TEXT("ENABLED") : TEXT("DISABLED"));
}

void UAIControllerSubsystem::OnDayPassed(int32 NewDay)
{
    UE_LOG(LogTemp, Display, TEXT("🔥 [AI] === AI TICK START — Day %d (BOTH FACTIONS) ==="), NewDay);

    if (UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>())
    {
        BaseMgr->AdvanceFacilityConstruction(EFactionType::Enemy);
        BaseMgr->AdvanceFacilityConstruction(EFactionType::Human);
        BaseMgr->AdvanceAllConstruction();
    }

    // === NEW: Respect per-faction simulation flags ===
    if (bSimulateHumanAI)
        RunAIForFaction(EFactionType::Human, NewDay);

    if (bSimulateEnemyAI)
        RunAIForFaction(EFactionType::Enemy, NewDay);
}

void UAIControllerSubsystem::Debug_RunAI()
{
    UE_LOG(LogTemp, Display, TEXT("[AI DEBUG] Manual AI run requested for BOTH factions (if enabled)"));
    if (bSimulateHumanAI) RunAIForFaction(EFactionType::Human, 999);
    if (bSimulateEnemyAI) RunAIForFaction(EFactionType::Enemy, 999);
}

void UAIControllerSubsystem::RunAIForFaction(EFactionType Faction, int32 CurrentDay)
{
    if (!bAIEnabled) return;   // global master switch still respected

    if (CurrentDay == LastProcessedAIDay && Faction == EFactionType::Enemy)
    {
        UE_LOG(LogTemp, Verbose, TEXT("[AI GUARD] Skipping duplicate AI run for %s on day %d"),
            *UEnum::GetValueAsString(Faction), CurrentDay);
        return;
    }
    
    LastProcessedAIDay = CurrentDay;

    UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    UResourceManagerSubsystem* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>();
    USoldierManagerSubsystem* SoldierMgr = GetGameInstance()->GetSubsystem<USoldierManagerSubsystem>();
    UResearchManagerSubsystem* ResearchMgr = GetGameInstance()->GetSubsystem<UResearchManagerSubsystem>();
    UEngineeringManagerSubsystem* EngineeringMgr = GetGameInstance()->GetSubsystem<UEngineeringManagerSubsystem>();

    if (!BaseMgr || !ResourceMgr) return;

    UE_LOG(LogTemp, Display, TEXT("[AI] %s AI — Day %d decision (full build order) - Bases: %d"),
        *UEnum::GetValueAsString(Faction), CurrentDay, BaseMgr->GetBases(Faction).Num());

    // === Initial base creation (different locations for Human vs Enemy) ===
    if (BaseMgr->GetBases(Faction).Num() == 0)
    {
        FVector2D NewLocation = (Faction == EFactionType::Human)
            ? FVector2D(300.0f, 540.0f)      // Human starts on left side
            : FVector2D(1620.0f, 540.0f);    // Enemy starts on right side

        FText BaseName = FText::FromString("Command Center");

        UE_LOG(LogTemp, Display, TEXT("[AI] %s has no bases — creating initial Command Center"), *UEnum::GetValueAsString(Faction));

        if (UStrategyBase* NewBase = BaseMgr->BuildNewBase(Faction, BaseName, NewLocation))
        {
            UE_LOG(LogTemp, Display, TEXT("[AI] ✅ Initial 'Command Center' base created for %s"), *UEnum::GetValueAsString(Faction));
            return;
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("[AI] FAILED to create initial base for %s!"), *UEnum::GetValueAsString(Faction));
            return;
        }
    }

    BaseMgr->AdvanceFacilityConstruction(Faction);
    ResourceMgr->ApplyFacilityIncome(Faction);

    // === The rest of your original logic stays 100% unchanged from here down ===
    if (BaseMgr->CanBuildNewBase(Faction))
    {
        int32 OperationalHangers = BaseMgr->GetNumberOfOperationalHangers(Faction);
        if (OperationalHangers >= 1)
        {
            FVector2D NewLocation = FVector2D(FMath::RandRange(100.0f, 1820.0f), FMath::RandRange(100.0f, 980.0f));
            FText BaseName = FText::FromString(FString::Printf(TEXT("Forward Base %d"), BaseMgr->GetBases(Faction).Num() + 1));

            if (UStrategyBase* NewBase = BaseMgr->BuildNewBase(Faction, BaseName, NewLocation))
            {
                UE_LOG(LogTemp, Display, TEXT("[AI] ✅ %s expanded to new base '%s'"), *UEnum::GetValueAsString(Faction), *BaseName.ToString());
                return;
            }
        }
    }

    for (UStrategyBase* Base : BaseMgr->GetBases(Faction))
    {
        if (!Base) continue;
        if (!Base->HasOperationalCommandCenter()) continue;

        UE_LOG(LogTemp, Display, TEXT("[AI] Developing base '%s' (Net Power: %d)"), *Base->BaseName.ToString(), Base->GetNetPower());

        if (Base->GetNetPower() < 0 || !Base->HasOperationalFacilityOfType(EFacilityType::PowerPlant))
        {
            TryBuildFacility(Faction, EFacilityType::PowerPlant, Base);
        }

        int32 CurrentCapacity = Base->GetTotalCapacityForType(EFacilityType::LivingQuarters);
        int32 CurrentSoldiers = SoldierMgr ? SoldierMgr->GetNumSoldiersStationedAt(Base, Faction) : 0;

        if (CurrentCapacity < 36 || CurrentSoldiers >= CurrentCapacity - 4)
        {
            UE_LOG(LogTemp, Display, TEXT("[AI] → BARRACKS NEAR FULL (%d/%d) — Trying extra LivingQuarters in '%s'"),
                CurrentSoldiers, CurrentCapacity, *Base->BaseName.ToString());
            TryBuildFacility(Faction, EFacilityType::LivingQuarters, Base);
        }

        if (!Base->HasFacilityOfType(EFacilityType::Storage)) TryBuildFacility(Faction, EFacilityType::Storage, Base);
        if (!Base->HasFacilityOfType(EFacilityType::Workshop)) TryBuildFacility(Faction, EFacilityType::Workshop, Base);
        if (!Base->HasFacilityOfType(EFacilityType::Laboratory)) TryBuildFacility(Faction, EFacilityType::Laboratory, Base);
        if (!Base->HasFacilityOfType(EFacilityType::Medical)) TryBuildFacility(Faction, EFacilityType::Medical, Base);

        if (!Base->HasOperationalFacilityOfType(EFacilityType::Hanger)) TryBuildFacility(Faction, EFacilityType::Hanger, Base);

        int32 OperationalHangers = 0;
        for (UStrategyFacility* Fac : Base->Facilities)
        {
            if (Fac && Fac->FacilityDefinition && Fac->FacilityDefinition->FacilityType == EFacilityType::Hanger && Fac->bIsOperational)
                OperationalHangers++;
        }

        int32 CurrentRepairBays = 0;
        int32 RepairUnderConstruction = 0;
        for (UStrategyFacility* Fac : Base->Facilities)
        {
            if (Fac && Fac->FacilityDefinition && Fac->FacilityDefinition->FacilityType == EFacilityType::VehicleRepair)
            {
                if (Fac->bIsOperational)
                    CurrentRepairBays++;
                else
                    RepairUnderConstruction++;
            }
        }

        if (CurrentRepairBays + RepairUnderConstruction < OperationalHangers)
        {
            UE_LOG(LogTemp, Display, TEXT("[AI] → Building VehicleRepair bay for hanger in '%s'"), *Base->BaseName.ToString());
            TryBuildFacility(Faction, EFacilityType::VehicleRepair, Base);
        }

        // === STAGGERED VEHICLE PRODUCTION ===
        if (Base->HasOperationalFacilityOfType(EFacilityType::Hanger))
        {
            UStrategyBase* TargetBase = GetBaseWithFewestVehicles(Faction);
            if (TargetBase)
            {
                if (TryBuildVehicle(Faction, TargetBase))
                {
                    UE_LOG(LogTemp, Display, TEXT("[AI] %s queued vehicle in base with fewest vehicles ('%s')"),
                        *UEnum::GetValueAsString(Faction), *TargetBase->BaseName.ToString());
                }
            }
        }

        if (Base->HasOperationalFacilityOfType(EFacilityType::Hanger))
        {
            bool bHasParkedVehicles = false;
            for (UStrategyFacility* Fac : Base->Facilities)
            {
                if (Fac && Fac->FacilityDefinition && Fac->FacilityDefinition->FacilityType == EFacilityType::Hanger)
                {
                    if (Fac->ParkedVehicles.Num() > 0)
                    {
                        bHasParkedVehicles = true;
                        break;
                    }
                }
            }

            if (bHasParkedVehicles)
            {
                // === Vehicle weapon + ammo equipping (your existing code) ===
                UEngineeringManagerSubsystem* EngMgr = GetGameInstance()->GetSubsystem<UEngineeringManagerSubsystem>();
                UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();
                if (EngMgr && Campaign && ResourceMgr)
                {
                    UItemDatabase* VehicleItemDB = Campaign->GetVehicleItemDatabase();
                    if (VehicleItemDB)
                    {
                        UItemDefinition* AvailableWeapon = nullptr;
                        for (const TSoftObjectPtr<UItemDefinition>& SoftItem : VehicleItemDB->BuyableItems)
                        {
                            if (UItemDefinition* Item = SoftItem.Get())
                            {
                                if (Item->Category == EItemCategory::VehicleWeapon)
                                {
                                    AvailableWeapon = Item;
                                    break;
                                }
                            }
                        }

                        for (UStrategyFacility* Fac : Base->Facilities)
                        {
                            if (Fac && Fac->FacilityDefinition && Fac->FacilityDefinition->FacilityType == EFacilityType::Hanger)
                            {
                                for (UStrategyVehicle* Vehicle : Fac->ParkedVehicles)
                                {
                                    if (!Vehicle) continue;

                                    if (Vehicle->GetEquippedWeapons().Num() < Vehicle->GetMaxWeaponSlots() && AvailableWeapon)
                                    {
                                        if (EngMgr->PurchaseAndEquipVehicleWeapon(Faction, Vehicle, AvailableWeapon))
                                        {
                                            UE_LOG(LogTemp, Display, TEXT("[AI] %s equipped vehicle weapon '%s' on %s"),
                                                *UEnum::GetValueAsString(Faction), *AvailableWeapon->ItemName.ToString(),
                                                *Vehicle->VehicleDefinition->VehicleName.ToString());
                                        }
                                    }

                                    for (int32 i = 0; i < Vehicle->WeaponAmmoCounts.Num(); ++i)
                                    {
                                        if (Vehicle->WeaponAmmoCounts.IsValidIndex(i) &&
                                            Vehicle->EquippedWeapons.IsValidIndex(i) &&
                                            Vehicle->WeaponAmmoCounts[i] < Vehicle->EquippedWeapons[i].Get()->MaxAmmo)
                                        {
                                            EngMgr->PurchaseAmmoForVehicle(Faction, Vehicle, i);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // === MISSION LAUNCH (now faction-aware) ===
                if (UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
                {
                    EMissionType ChosenType = static_cast<EMissionType>(FMath::RandRange(0, 2));

                    // Use StartMission directly so we pass the correct AttackingFaction
                    TArray<UStrategyVehicle*> AvailableVehicles;
                    for (UStrategyFacility* Fac : Base->Facilities)
                    {
                        if (Fac && Fac->FacilityDefinition && Fac->FacilityDefinition->FacilityType == EFacilityType::Hanger)
                            AvailableVehicles.Append(Fac->ParkedVehicles);
                    }

                    if (AvailableVehicles.Num() > 0)
                    {
                        if (UMissionGroup* Mission = MissionMgr->StartMission(Base, AvailableVehicles, 15, TArray<UStrategySoldier*>(), ChosenType, Faction))
                        {
                            UE_LOG(LogTemp, Display, TEXT("[AI] %s AI launched %s mission from base '%s' with %d vehicles"),
                                *UEnum::GetValueAsString(Faction), *UEnum::GetValueAsString(ChosenType), *Base->BaseName.ToString(), AvailableVehicles.Num());
                        }
                    }
                }
            }
            else
            {
                UE_LOG(LogTemp, Verbose, TEXT("[AI] Hanger exists in '%s' but no parked vehicles yet — waiting"), *Base->BaseName.ToString());
            }
        }

        UE_LOG(LogTemp, Display, TEXT("[AI] → Trying extra Storage/Hanger/Power in '%s'"), *Base->BaseName.ToString());
        TryBuildFacility(Faction, EFacilityType::Storage, Base);
        TryBuildFacility(Faction, EFacilityType::Hanger, Base);
        if (Base->GetNetPower() < 100) TryBuildFacility(Faction, EFacilityType::PowerPlant, Base);
    }

    bool bRecruited = (SoldierMgr && TryRecruit(Faction));
    if (bRecruited) UE_LOG(LogTemp, Display, TEXT("[AI] %s recruited soldier"), *UEnum::GetValueAsString(Faction));

    if (ResearchMgr && TryResearch(Faction)) UE_LOG(LogTemp, Display, TEXT("[AI] %s started research"), *UEnum::GetValueAsString(Faction));
    if (TryBuyAndEquip(Faction)) UE_LOG(LogTemp, Display, TEXT("[AI] %s purchase/equip action taken"), *UEnum::GetValueAsString(Faction));
    if (EngineeringMgr && EngineeringMgr->TryProduce(Faction)) UE_LOG(LogTemp, Display, TEXT("[AI] %s production action taken"), *UEnum::GetValueAsString(Faction));

    UE_LOG(LogTemp, Display, TEXT("[AI] %s AI — End of day %d (actions completed)"), *UEnum::GetValueAsString(Faction), CurrentDay);
}

// === UPDATED: Queues soldier training in Barracks using Production Slots ===
bool UAIControllerSubsystem::TryRecruit(EFactionType Faction)
{
    auto* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>();
    auto* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    auto* SoldierMgr = GetGameInstance()->GetSubsystem<USoldierManagerSubsystem>();
    auto* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();

    if (!ResourceMgr || !BaseMgr || !SoldierMgr)
    {
        UE_LOG(LogTemp, Warning, TEXT("[RECRUIT] Missing required subsystems!"));
        return false;
    }

    FResourceStockpile Res = ResourceMgr->GetResources(Faction);
    if (Res.Money < 500)
    {
        UE_LOG(LogTemp, Verbose, TEXT("[RECRUIT] EFactionType::%s cannot afford recruit (needs 500 Money)"), *UEnum::GetValueAsString(Faction));
        return false;
    }

    const TArray<UStrategyBase*>& Bases = BaseMgr->GetBases(Faction);
    for (UStrategyBase* Base : Bases)
    {
        if (!Base) continue;

        for (UStrategyFacility* Barracks : Base->Facilities)
        {
            if (Barracks && Barracks->FacilityDefinition && Barracks->FacilityDefinition->FacilityType == EFacilityType::LivingQuarters)
            {
                if (Barracks->HasFreeProductionSlot())
                {
                    USoldierClassDefinition* ClassDef = nullptr;
                    if (Campaign && Campaign->SoldierClassDatabaseAsset.IsValid())
                    {
                        if (USoldierClassDatabase* DB = Campaign->SoldierClassDatabaseAsset.Get())
                            if (DB->AvailableSoldierClasses.Num() > 0)
                                ClassDef = DB->AvailableSoldierClasses[0].Get();
                    }

                    if (Barracks->StartProduction(EProductionType::Soldier, ClassDef, 4)) // 4 day training
                    {
                        Res.Money -= 500;
                        ResourceMgr->SetResources(Faction, Res);

                        UE_LOG(LogTemp, Display, TEXT("[AI] ✅ EFactionType::%s queued soldier training in %s (4 days)"),
                            *UEnum::GetValueAsString(Faction), *Barracks->FacilityDefinition->FacilityName.ToString());
                        return true;
                    }
                }
            }
        }
    }

    UE_LOG(LogTemp, Verbose, TEXT("[RECRUIT] No base with free barracks production slots"));
    return false;
}

// === UPDATED: Queues vehicle construction in Hanger using Production Slots ===
bool UAIControllerSubsystem::TryBuildVehicle(EFactionType Faction, UStrategyBase* TargetBase)
{
    UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();
    if (!Campaign) return false;

    UVehicleDatabase* VehicleDB = Campaign->VehicleDatabaseAsset.Get();
    if (!VehicleDB || VehicleDB->AvailableVehicles.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AI] No vehicles available in database for %s"), *UEnum::GetValueAsString(Faction));
        return false;
    }

    UVehicleDefinition* VehDef = VehicleDB->AvailableVehicles[0].Get();
    if (!VehDef) return false;

    UResourceManagerSubsystem* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>();
    if (!ResourceMgr) return false;

    FResourceStockpile Res = ResourceMgr->GetResources(Faction);

    if (!ResourceMgr->CanAfford(Faction, VehDef->BuildCost))
    {
        UE_LOG(LogTemp, Verbose, TEXT("[AI] %s cannot afford vehicle '%s' (needs %d Money, %d Metals, %d Biologicals, %d Chemicals)"),
            *UEnum::GetValueAsString(Faction), *VehDef->VehicleName.ToString(),
            VehDef->BuildCost.Money, VehDef->BuildCost.Metals, VehDef->BuildCost.Biologicals, VehDef->BuildCost.Chemicals);
        return false;
    }

    for (UStrategyFacility* Hanger : TargetBase->Facilities)
    {
        if (Hanger && Hanger->FacilityDefinition && Hanger->FacilityDefinition->FacilityType == EFacilityType::Hanger)
        {
            if (Hanger->HasFreeProductionSlot())
            {
                if (Hanger->StartProduction(EProductionType::Vehicle, VehDef, VehDef->ProductionDays))
                {
                    ResourceMgr->AddResources(Faction, { -VehDef->BuildCost.Money, -VehDef->BuildCost.Metals, -VehDef->BuildCost.Biologicals, -VehDef->BuildCost.Chemicals, 0, 0 });

                    UE_LOG(LogTemp, Display, TEXT("[AI] %s queued vehicle '%s' in hanger (%d days)"),
                        *UEnum::GetValueAsString(Faction), *VehDef->VehicleName.ToString(), VehDef->ProductionDays);
                    return true;
                }
            }
        }
    }

    UE_LOG(LogTemp, Display, TEXT("[AI] %s has operational hanger but no free production slot for vehicle"), *UEnum::GetValueAsString(Faction));
    return false;
}

bool UAIControllerSubsystem::TryBuyAndEquip(EFactionType Faction)
{
    UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();
    USoldierManagerSubsystem* SoldierMgr = GetGameInstance()->GetSubsystem<USoldierManagerSubsystem>();
    UEngineeringManagerSubsystem* EngineeringMgr = GetGameInstance()->GetSubsystem<UEngineeringManagerSubsystem>();
    UResourceManagerSubsystem* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>();
    UItemDatabase* ItemDB = Campaign ? Campaign->ItemDatabaseAsset.Get() : nullptr;

    if (!SoldierMgr || !EngineeringMgr || !ResourceMgr || !ItemDB) return false;

    const TArray<UStrategySoldier*>& Roster = SoldierMgr->GetRoster(Faction);
    if (Roster.Num() == 0) return false;

    UE_LOG(LogTemp, Display, TEXT("[PURCHASE] === %s starting buy round (smart priority) ==="), *UEnum::GetValueAsString(Faction));

    bool bBoughtAnything = false;
    int32 PurchasesThisDay = 0;
    const int32 MaxPurchasesPerDay = 4;

    while (PurchasesThisDay < MaxPurchasesPerDay)
    {
        UStrategySoldier* TargetSoldier = nullptr;
        int32 MinItems = INT_MAX;
        for (UStrategySoldier* Soldier : Roster)
        {
            if (Soldier && Soldier->CurrentLoadout.Num() < MinItems)
            {
                MinItems = Soldier->CurrentLoadout.Num();
                TargetSoldier = Soldier;
            }
        }
        if (!TargetSoldier) break;

        if (MinItems >= 10) break;

        FResourceStockpile Res = ResourceMgr->GetResources(Faction);

        bool bPurchasedThisLoop = false;

        for (const TSoftObjectPtr<UItemDefinition>& SoftItem : ItemDB->BuyableItems)
        {
            UItemDefinition* ItemDef = SoftItem.Get();
            if (!ItemDef) continue;

            // === CRITICAL FIX: Strict research gating ===
            if (!Campaign->IsItemUnlocked(Faction, ItemDef))
            {
                UE_LOG(LogTemp, Verbose, TEXT("[PURCHASE] %s is NOT unlocked yet — skipping (research not completed)"), *ItemDef->ItemName.ToString());
                continue;
            }

            if (TargetSoldier->CurrentLoadout.Contains(ItemDef)) continue;

            if (Res.Money >= ItemDef->PurchaseCost.Money)
            {
                if (EngineeringMgr->PurchaseItem(Faction, ItemDef, TargetSoldier))
                {
                    UE_LOG(LogTemp, Display, TEXT("[AI] Bought %s on soldier (now has %d items)"),
                        *ItemDef->ItemName.ToString(), TargetSoldier->CurrentLoadout.Num());

                    if (UStrategyEventDispatcher* EventDisp = GetGameInstance()->GetSubsystem<UStrategyEventDispatcher>())
                    {
                        EventDisp->OnSoldierLoadoutChanged.Broadcast(Faction, TargetSoldier);
                    }

                    bPurchasedThisLoop = true;
                    bBoughtAnything = true;
                    PurchasesThisDay++;
                    break;
                }
            }
        }

        if (!bPurchasedThisLoop) break;
    }

    return bBoughtAnything;
}

bool UAIControllerSubsystem::TryBuildFacility(EFactionType Faction, EFacilityType FacilityTypeToBuild, UStrategyBase* TargetBase)
{
    UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();
    UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    UResourceManagerSubsystem* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>();

    if (!Campaign || !BaseMgr || !ResourceMgr)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AI] Missing subsystems for facility build"));
        return false;
    }

    UFacilityDatabase* DB = Campaign->FacilityDatabaseAsset.Get();
    if (!DB)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AI] FacilityDatabase is null!"));
        return false;
    }

    UFacilityDefinition* FacilityDef = nullptr;
    for (const TSoftObjectPtr<UFacilityDefinition>& SoftDef : DB->AvailableFacilities)
    {
        if (UFacilityDefinition* Def = SoftDef.Get())
        {
            if (Def->FacilityType == FacilityTypeToBuild)
            {
                FacilityDef = Def;
                break;
            }
        }
    }

    if (!FacilityDef)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AI] No facility of type %s in FacilityDatabase!"), *UEnum::GetValueAsString(FacilityTypeToBuild));
        return false;
    }

    if (TargetBase)
    {
        int32 CurrentCountInBase = 0;
        int32 UnderConstruction = 0;
        for (UStrategyFacility* Fac : TargetBase->Facilities)
        {
            if (Fac && Fac->FacilityDefinition && Fac->FacilityDefinition->FacilityType == FacilityTypeToBuild)
            {
                CurrentCountInBase++;
                if (!Fac->bIsOperational)
                    UnderConstruction++;
            }
        }

        if (CurrentCountInBase + UnderConstruction >= FacilityDef->MaxBuilt)
        {
            UE_LOG(LogTemp, Verbose, TEXT("[AI] MaxBuilt or already queued for %s in base '%s' (%d built + %d building) — skipping"),
                *UEnum::GetValueAsString(FacilityTypeToBuild), *TargetBase->BaseName.ToString(), CurrentCountInBase, UnderConstruction);
            return false;
        }
    }

    if (!ResourceMgr->CanAfford(Faction, FacilityDef->BuildCost))
    {
        UE_LOG(LogTemp, Verbose, TEXT("[AI] Not enough resources for %s (needs %d Money, %d Metals, %d Biologicals, %d Chemicals)"),
            *FacilityDef->FacilityName.ToString(), FacilityDef->BuildCost.Money,
            FacilityDef->BuildCost.Metals, FacilityDef->BuildCost.Biologicals, FacilityDef->BuildCost.Chemicals);
        return false;
    }

    if (BaseMgr->BuildFacility(Faction, FacilityDef, TargetBase))
    {
        UE_LOG(LogTemp, Display, TEXT("[AI] %s started construction of %s in base '%s' (%d days)"),
            *UEnum::GetValueAsString(Faction), *FacilityDef->FacilityName.ToString(),
            TargetBase ? *TargetBase->BaseName.ToString() : TEXT("default base"), FacilityDef->BuildTimeDays);
        return true;
    }

    UE_LOG(LogTemp, Warning, TEXT("[AI] BuildFacility failed for %s in base '%s'"),
        *FacilityDef->FacilityName.ToString(), TargetBase ? *TargetBase->BaseName.ToString() : TEXT("unknown"));
    return false;
}

bool UAIControllerSubsystem::TryResearch(EFactionType Faction)
{
    UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();
    UResearchManagerSubsystem* ResearchMgr = GetGameInstance()->GetSubsystem<UResearchManagerSubsystem>();
    UResourceManagerSubsystem* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>();

    if (!Campaign || !ResearchMgr || !ResourceMgr)
    {
        UE_LOG(LogTemp, Warning, TEXT("[RESEARCH] Missing required subsystems!"));
        return false;
    }

    TArray<UActiveResearchProject*> Active = ResearchMgr->GetActiveResearch(Faction);
    if (Active.Num() > 0)
    {
        UE_LOG(LogTemp, Verbose, TEXT("[AI] %s already has %d active research job(s) — skipping new research this day"),
            *UEnum::GetValueAsString(Faction), Active.Num());
        return false;
    }

    // === FIX: Early-out when no Laboratory exists yet (eliminates all the spam) ===
    UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    if (BaseMgr && !BaseMgr->HasFacilityOfType(Faction, EFacilityType::Laboratory))
    {
        UE_LOG(LogTemp, Verbose, TEXT("[AI] %s has no Laboratory yet — skipping research attempts this day"),
            *UEnum::GetValueAsString(Faction));
        return false;
    }

    UResearchDatabase* ResearchDB = Campaign->ResearchDatabaseAsset.Get();
    if (!ResearchDB)
    {
        UE_LOG(LogTemp, Warning, TEXT("[RESEARCH] ResearchDatabaseAsset not loaded!"));
        return false;
    }
    if (ResearchDB->AvailableTechs.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[RESEARCH] No AvailableTechs in ResearchDatabase!"));
        return false;
    }

    UE_LOG(LogTemp, Display, TEXT("[RESEARCH] %s checking %d available techs..."),
        *UEnum::GetValueAsString(Faction), ResearchDB->AvailableTechs.Num());

    FResourceStockpile Res = ResourceMgr->GetResources(Faction);

    for (const TSoftObjectPtr<UResearchTechDefinition>& SoftResearch : ResearchDB->AvailableTechs)
    {
        UResearchTechDefinition* ResearchDef = SoftResearch.Get();
        if (!ResearchDef) continue;

        if (ResearchMgr->IsResearchInProgress(Faction, ResearchDef) || ResearchMgr->HasCompletedResearch(Faction, ResearchDef))
        {
            UE_LOG(LogTemp, Verbose, TEXT("[RESEARCH] Skipping %s — already in progress or completed"), *ResearchDef->ProjectName.ToString());
            continue;
        }

        if (Res.Money < ResearchDef->ResearchCost.Money)
        {
            UE_LOG(LogTemp, Verbose, TEXT("[RESEARCH] Skipping %s — not enough money (%d needed)"),
                *ResearchDef->ProjectName.ToString(), ResearchDef->ResearchCost.Money);
            continue;
        }

        if (ResearchMgr->StartResearch(Faction, ResearchDef))
        {
            UE_LOG(LogTemp, Display, TEXT("[AI] %s started research: %s (%d days)"),
                *UEnum::GetValueAsString(Faction), *ResearchDef->ProjectName.ToString(), ResearchDef->ResearchDays);
            return true;
        }
    }

    UE_LOG(LogTemp, Verbose, TEXT("[RESEARCH] No suitable tech found this day"));
    return false;
}

void UAIControllerSubsystem::SetAIEnabled(bool bEnable)
{
    bAIEnabled = bEnable;
    UE_LOG(LogTemp, Display, TEXT("AI Controller %s for Enemy faction"), bAIEnabled ? TEXT("ENABLED") : TEXT("DISABLED"));
}

bool UAIControllerSubsystem::IsAIEnabled() const
{
    return bAIEnabled;
}

void UAIControllerSubsystem::SetSimulateHumanAI(bool bEnable)
{
    bSimulateHumanAI = bEnable;
    UE_LOG(LogTemp, Display, TEXT("Human AI simulation %s"), bEnable ? TEXT("ENABLED") : TEXT("DISABLED"));
}

void UAIControllerSubsystem::SetSimulateEnemyAI(bool bEnable)
{
    bSimulateEnemyAI = bEnable;
    UE_LOG(LogTemp, Display, TEXT("Enemy AI simulation %s"), bEnable ? TEXT("ENABLED") : TEXT("DISABLED"));
}

bool UAIControllerSubsystem::IsSimulatingHumanAI() const
{
    return bSimulateHumanAI;
}

bool UAIControllerSubsystem::IsSimulatingEnemyAI() const
{
    return bSimulateEnemyAI;
}

UStrategyBase* UAIControllerSubsystem::GetBaseWithFewestVehicles(EFactionType Faction) const
{
    UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    if (!BaseMgr) return nullptr;

    const TArray<UStrategyBase*>& Bases = BaseMgr->GetBases(Faction);
    if (Bases.Num() == 0) return nullptr;

    UStrategyBase* BestBase = nullptr;
    int32 MinVehicles = INT_MAX;

    for (UStrategyBase* Base : Bases)
    {
        int32 ParkedCount = 0;
        for (UStrategyFacility* Fac : Base->Facilities)
        {
            if (Fac && Fac->FacilityDefinition && Fac->FacilityDefinition->FacilityType == EFacilityType::Hanger)
            {
                ParkedCount += Fac->ParkedVehicles.Num();
            }
        }

        if (ParkedCount < MinVehicles)
        {
            MinVehicles = ParkedCount;
            BestBase = Base;
        }
    }

    return BestBase;
}
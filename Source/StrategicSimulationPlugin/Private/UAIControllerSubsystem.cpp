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
    UE_LOG(LogTemp, Display, TEXT("🔥 [AI TICK] === DAY %d START — Checking simulation flags ==="), NewDay);
    UE_LOG(LogTemp, Display, TEXT("   → Human AI enabled: %s"), bSimulateHumanAI ? TEXT("YES") : TEXT("NO"));
    UE_LOG(LogTemp, Display, TEXT("   → Enemy AI enabled: %s"), bSimulateEnemyAI ? TEXT("YES") : TEXT("NO"));

    if (UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>())
    {
        BaseMgr->AdvanceFacilityConstruction(EFactionType::Enemy);
        BaseMgr->AdvanceFacilityConstruction(EFactionType::Human);
        BaseMgr->AdvanceAllConstruction();
    }

    if (bSimulateHumanAI)
    {
        UE_LOG(LogTemp, Display, TEXT("[AI] Human AI — Calling RunAIForFaction..."));
        RunAIForFaction(EFactionType::Human, NewDay);
    }
    else
    {
        UE_LOG(LogTemp, Display, TEXT("[AI] Human AI — SKIPPED (toggle is OFF)"));
    }

    if (bSimulateEnemyAI)
    {
        UE_LOG(LogTemp, Display, TEXT("[AI] Enemy AI — Calling RunAIForFaction..."));
        RunAIForFaction(EFactionType::Enemy, NewDay);
    }
    else
    {
        UE_LOG(LogTemp, Display, TEXT("[AI] Enemy AI — SKIPPED (toggle is OFF)"));
    }

    UE_LOG(LogTemp, Display, TEXT("[AI TICK] === DAY %d COMPLETE ==="), NewDay);
}

void UAIControllerSubsystem::PerformDailyBuildOrder(EFactionType Faction)
{
    UE_LOG(LogTemp, Display, TEXT("[PLAYER-CALLABLE] Performing daily build order for %s"), *UEnum::GetValueAsString(Faction));
    RunAIForFaction(Faction, GetGameInstance()->GetSubsystem<UTimeManagerSubsystem>()->GetCurrentDay());
}

// RUN AI FOR FACTION
// Does routine for AI
void UAIControllerSubsystem::RunAIForFaction(EFactionType Faction, int32 CurrentDay)
{
    UE_LOG(LogTemp, Display, TEXT("[AI] >>> ENTERING RunAIForFaction for %s (Day %d)"),
        *UEnum::GetValueAsString(Faction), CurrentDay);

    if (!bAIEnabled) return;

    int32& LastDay = LastProcessedDayPerFaction.FindOrAdd(Faction, -1);
    if (CurrentDay == LastDay) return;
    LastDay = CurrentDay;

    UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    UResourceManagerSubsystem* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>();
    USoldierManagerSubsystem* SoldierMgr = GetGameInstance()->GetSubsystem<USoldierManagerSubsystem>();
    UResearchManagerSubsystem* ResearchMgr = GetGameInstance()->GetSubsystem<UResearchManagerSubsystem>();
    UEngineeringManagerSubsystem* EngineeringMgr = GetGameInstance()->GetSubsystem<UEngineeringManagerSubsystem>();

    if (!BaseMgr || !ResourceMgr) return;

    FResourceStockpile Res = ResourceMgr->GetResources(Faction);
    UE_LOG(LogTemp, Display, TEXT("[AI] %s AI — Day %d decision - Bases: %d | 💰%d | 🛠️%d | 🧬%d | ⚗️%d | 🌌%d | 📚%d"),
        *UEnum::GetValueAsString(Faction), CurrentDay,
        BaseMgr->GetBases(Faction).Num(),
        Res.Money, Res.Metals, Res.Biologicals, Res.Chemicals, Res.ExoticMaterial, Res.ResearchPoints);

    // === INITIAL BASE CREATION ===
    if (BaseMgr->GetBases(Faction).Num() == 0)
    {
        FVector2D NewLocation = (Faction == EFactionType::Human) ? FVector2D(300.0f, 540.0f) : FVector2D(1620.0f, 540.0f);
        if (UStrategyBase* NewBase = BaseMgr->BuildNewBase(Faction, FText::FromString("Command Center"), NewLocation))
        {
            UE_LOG(LogTemp, Display, TEXT("[AI] ✅ Initial Command Center created for %s"), *UEnum::GetValueAsString(Faction));
        }
    }

    // === PER-BASE DEVELOPMENT — ONLY OLDEST BASE EACH DAY ===
    BaseMgr->AdvanceFacilityConstruction(Faction);
    ResourceMgr->ApplyFacilityIncome(Faction);

    const TArray<UStrategyBase*>& AllBases = BaseMgr->GetBases(Faction);
    if (AllBases.Num() == 0) return;

    UStrategyBase* OldestBase = AllBases[0];  // ← Wave rule: only develop oldest base
    UE_LOG(LogTemp, Display, TEXT("[AI] Developing oldest base '%s' (Net Power: %d)"), *OldestBase->BaseName.ToString(), OldestBase->GetNetPower());

    TArray<EFacilityType> DesiredOrder = {
        EFacilityType::LivingQuarters,
        EFacilityType::Laboratory,
        EFacilityType::Workshop,
        EFacilityType::Hanger,
        EFacilityType::Medical,
        EFacilityType::VehicleRepair
    };

    for (EFacilityType FacType : DesiredOrder)
    {
        UE_LOG(LogTemp, Display, TEXT("[AI DEBUG] Testing %s for build"), *UEnum::GetValueAsString(FacType));

        if (!OldestBase->CanBuildFacilityType(FacType))
        {
            UE_LOG(LogTemp, Verbose, TEXT("[FACILITY] %s skipped — prerequisites not met"), *UEnum::GetValueAsString(FacType));
            continue;
        }

        bool bShouldBuild = false;
        if (FacType == EFacilityType::LivingQuarters)
        {
            bool bCoreLayerDone = OldestBase->HasOperationalFacilityOfType(EFacilityType::Laboratory);
            int32 CurrentBarracks = OldestBase->GetTotalBuiltOfType(EFacilityType::LivingQuarters);
            bShouldBuild = (CurrentBarracks < 6) && (bCoreLayerDone || CurrentBarracks == 0);
        }
        else
        {
            bShouldBuild = !OldestBase->HasOperationalFacilityOfType(FacType);
        }

        if (bShouldBuild)
        {
            UE_LOG(LogTemp, Display, TEXT("[AI] Attempting to build %s in oldest base '%s'"), *UEnum::GetValueAsString(FacType), *OldestBase->BaseName.ToString());

            if (TryBuildFacility(Faction, FacType, OldestBase))
            {
                UE_LOG(LogTemp, Display, TEXT("[AI] %s built %s in oldest base '%s'"), *UEnum::GetValueAsString(Faction), *UEnum::GetValueAsString(FacType), *OldestBase->BaseName.ToString());
                break; // one facility per day
            }
        }
    }

    // === FULL VEHICLE / MISSION / AMMO LOGIC (unchanged) ===
    if (OldestBase->HasOperationalFacilityOfType(EFacilityType::Hanger))
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

    if (OldestBase->HasOperationalFacilityOfType(EFacilityType::Hanger))
    {
        bool bHasParkedVehicles = false;
        for (UStrategyFacility* Fac : OldestBase->Facilities)
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

                    for (UStrategyFacility* Fac : OldestBase->Facilities)
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

            if (UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
            {
                EMissionType ChosenType = static_cast<EMissionType>(FMath::RandRange(0, 2));

                TArray<UStrategyVehicle*> AvailableVehicles;
                for (UStrategyFacility* Fac : OldestBase->Facilities)
                {
                    if (Fac && Fac->FacilityDefinition && Fac->FacilityDefinition->FacilityType == EFacilityType::Hanger)
                        AvailableVehicles.Append(Fac->ParkedVehicles);
                }

                if (AvailableVehicles.Num() > 0)
                {
                    if (UMissionGroup* Mission = MissionMgr->StartMission(OldestBase, AvailableVehicles, 15, TArray<UStrategySoldier*>(), ChosenType, Faction))
                    {
                        UE_LOG(LogTemp, Display, TEXT("[AI] %s AI launched %s mission from base '%s' with %d vehicles"),
                            *UEnum::GetValueAsString(Faction), *UEnum::GetValueAsString(ChosenType), *OldestBase->BaseName.ToString(), AvailableVehicles.Num());
                    }
                }
            }
        }
        else
        {
            UE_LOG(LogTemp, Verbose, TEXT("[AI] Hanger exists in '%s' but no parked vehicles yet — waiting"), *OldestBase->BaseName.ToString());
        }
    }

    // === DAILY ROUTINES ===
    bool bRecruited = (SoldierMgr && TryRecruit(Faction));
    if (bRecruited) UE_LOG(LogTemp, Display, TEXT("[AI] %s recruited soldier"), *UEnum::GetValueAsString(Faction));

    if (ResearchMgr && TryResearch(Faction)) UE_LOG(LogTemp, Display, TEXT("[AI] %s started research"), *UEnum::GetValueAsString(Faction));
    if (TryBuyAndEquip(Faction)) UE_LOG(LogTemp, Display, TEXT("[AI] %s purchase/equip action taken"), *UEnum::GetValueAsString(Faction));
    if (EngineeringMgr && EngineeringMgr->TryProduce(Faction)) UE_LOG(LogTemp, Display, TEXT("[AI] %s production action taken"), *UEnum::GetValueAsString(Faction));

    if (BaseMgr)
    {
        BaseMgr->DebugPrintFullBaseState(Faction);
    }

    // === EXPANSION — only after oldest base owns a vehicle (parked OR on mission) ===
    const TArray<UStrategyBase*>& AllBases = BaseMgr->GetBases(Faction);
    if (AllBases.Num() < MaxBases)   // ← NOW USES THE CAMPAIGN SETTING
    {
        UStrategyBase* OldestBaseForExpansionCheck = AllBases[0];

        bool bReadyToExpand = false;
        UE_LOG(LogTemp, Display, TEXT("[EXPANSION DEBUG] %s — Checking expansion: Bases = %d (max %d)"),
            *UEnum::GetValueAsString(Faction), AllBases.Num(), MaxBases);

        // Does oldest base have a Hanger?
        if (OldestBaseForExpansionCheck->HasOperationalFacilityOfType(EFacilityType::Hanger))
        {
            // Check parked vehicles
            bool bHasParked = false;
            for (UStrategyFacility* Fac : OldestBaseForExpansionCheck->Facilities)
            {
                if (Fac && Fac->FacilityDefinition && Fac->FacilityDefinition->FacilityType == EFacilityType::Hanger)
                {
                    if (Fac->ParkedVehicles.Num() > 0)
                    {
                        bHasParked = true;
                        break;
                    }
                }
            }

            // Or has active missions (vehicles are flying but still owned by this base)
            UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>();
            bool bHasFlyingVehicles = (MissionMgr && MissionMgr->ActiveMissions.Num() > 0);

            if (bHasParked || bHasFlyingVehicles)
            {
                bReadyToExpand = true;
                UE_LOG(LogTemp, Display, TEXT("[EXPANSION DEBUG]   Oldest base '%s' owns vehicle (parked:%s | flying:%s)"),
                    *OldestBaseForExpansionCheck->BaseName.ToString(),
                    bHasParked ? TEXT("YES") : TEXT("no"),
                    bHasFlyingVehicles ? TEXT("YES") : TEXT("no"));
            }
        }

        if (bReadyToExpand && ResourceMgr->GetResources(Faction).Money > 9000)
        {
            UE_LOG(LogTemp, Display, TEXT("[AI] %s — EXPANSION TRIGGERED! Oldest base owns vehicle → Building NEW base #%d"),
                *UEnum::GetValueAsString(Faction), AllBases.Num() + 1);

            if (UStrategyBase* NewBase = BaseMgr->BuildNewBase(Faction, FText::FromString(FString::Printf(TEXT("Forward Base %02d"), AllBases.Num() + 1)), FVector2D::ZeroVector))
            {
                UE_LOG(LogTemp, Display, TEXT("[AI] ✅ New base created: %s"), *NewBase->BaseName.ToString());
            }
        }
        else if (bReadyToExpand)
        {
            UE_LOG(LogTemp, Display, TEXT("[EXPANSION DEBUG] %s — Ready to expand but not enough money"), *UEnum::GetValueAsString(Faction));
        }
    }
    else
    {
        UE_LOG(LogTemp, Verbose, TEXT("[EXPANSION DEBUG] %s — At maximum bases (%d)"), *UEnum::GetValueAsString(Faction), MaxBases);
    }

    UE_LOG(LogTemp, Display, TEXT("[AI] %s AI — End of day %d (actions completed)"), *UEnum::GetValueAsString(Faction), CurrentDay);
}

// === UPDATED: Uses data-driven soldier class cost and training time ===
bool UAIControllerSubsystem::TryRecruit(EFactionType Faction)
{
    UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    UResourceManagerSubsystem* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>();
    USoldierManagerSubsystem* SoldierMgr = GetGameInstance()->GetSubsystem<USoldierManagerSubsystem>();
    UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();

    if (!BaseMgr || !ResourceMgr || !SoldierMgr || !Campaign) return false;

    int32 TotalCapacity = BaseMgr->GetTotalBarracksCapacity(Faction);
    int32 CurrentSoldiers = SoldierMgr->GetRoster(Faction).Num();

    // === COMMANDER EXCEPTION ===
    // The initial "Sgt. Rookie" / "Overlord Rookie" is the owner/flag — he stays in Command Center
    // and does NOT count against barracks capacity.
    int32 CommanderCount = 0;
    for (UStrategySoldier* Soldier : SoldierMgr->GetRoster(Faction))
    {
        if (Soldier && Soldier->SoldierName.Contains(TEXT("Rookie")))
            CommanderCount++;
    }
    int32 EffectiveSoldiers = CurrentSoldiers - CommanderCount;

    if (EffectiveSoldiers >= TotalCapacity)
    {
        UE_LOG(LogTemp, Verbose, TEXT("[RECRUIT] %s at max capacity (%d regular soldiers + %d commander / %d slots) — skipping"),
            *UEnum::GetValueAsString(Faction), EffectiveSoldiers, CommanderCount, TotalCapacity);
        return false;
    }

    if (ResourceMgr->GetResources(Faction).Money < 1500)
    {
        UE_LOG(LogTemp, Verbose, TEXT("[RECRUIT] Skipping — saving for next facility"));
        return false;
    }

    // Get soldier class (first one in DB)
    USoldierClassDefinition* ClassDef = nullptr;
    if (Campaign->SoldierClassDatabaseAsset.IsValid())
    {
        if (USoldierClassDatabase* DB = Campaign->SoldierClassDatabaseAsset.Get())
        {
            if (!DB->AvailableSoldierClasses.IsEmpty())
                ClassDef = DB->AvailableSoldierClasses[0].Get();
        }
    }
    if (!ClassDef) return false;

    const FResourceStockpile& Cost = ClassDef->TrainingCost;
    if (!ResourceMgr->CanAfford(Faction, Cost)) return false;

    // Try to queue in any barracks with free production slot
    const TArray<UStrategyBase*>& Bases = BaseMgr->GetBases(Faction);
    for (UStrategyBase* Base : Bases)
    {
        if (!Base) continue;
        for (UStrategyFacility* Barracks : Base->Facilities)
        {
            if (Barracks && Barracks->FacilityDefinition &&
                Barracks->FacilityDefinition->FacilityType == EFacilityType::LivingQuarters &&
                Barracks->HasFreeProductionSlot())
            {
                if (Barracks->StartProduction(EProductionType::Soldier, ClassDef, ClassDef->TrainingDays))
                {
                    ResourceMgr->AddResources(Faction, FResourceStockpile{
                        -Cost.Money, -Cost.Metals, -Cost.Biologicals,
                        -Cost.Chemicals, -Cost.ExoticMaterial, -Cost.ResearchPoints });

                    UE_LOG(LogTemp, Display, TEXT("[AI] %s queued %s training in %s (%d days)"),
                        *UEnum::GetValueAsString(Faction), *ClassDef->ClassName.ToString(),
                        *Barracks->FacilityDefinition->FacilityName.ToString(), ClassDef->TrainingDays);

                    return true;
                }
            }
        }
    }

    UE_LOG(LogTemp, Verbose, TEXT("[RECRUIT] No free production slots in any barracks"));
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

// === FIXED PURCHASE — 100% deterministic, same for both factions ===
bool UAIControllerSubsystem::TryBuyAndEquip(EFactionType Faction)
{
    UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();
    USoldierManagerSubsystem* SoldierMgr = GetGameInstance()->GetSubsystem<USoldierManagerSubsystem>();
    UEngineeringManagerSubsystem* EngineeringMgr = GetGameInstance()->GetSubsystem<UEngineeringManagerSubsystem>();
    UResourceManagerSubsystem* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>();
    UItemDatabase* ItemDB = Campaign ? Campaign->ItemDatabaseAsset.Get() : nullptr;

    if (!SoldierMgr || !EngineeringMgr || !ResourceMgr || !ItemDB) return false;

    TArray<UStrategySoldier*> Roster = SoldierMgr->GetRoster(Faction);
    if (Roster.Num() == 0) return false;

    // === FORCE SAME ROSTER ORDER EVERY TIME (alphabetical by name) ===
    Roster.Sort([](const UStrategySoldier& A, const UStrategySoldier& B) {
        return A.SoldierName < B.SoldierName;   // FString comparison, no .ToString()
    });

    UE_LOG(LogTemp, Display, TEXT("[PURCHASE] === %s starting buy round (smart priority) ==="), *UEnum::GetValueAsString(Faction));

    TArray<FString> ItemPriority = { "Knife", "Pistol", "Basic Armor", "Healthpack", "M-16 Rifle", "Grenade", "Proximity Bomb" };

    bool bBoughtAnything = false;
    int32 PurchasesThisDay = 0;
    const int32 MaxPurchasesPerDay = 1;

    for (const FString& DesiredName : ItemPriority)
    {
        if (PurchasesThisDay >= MaxPurchasesPerDay) break;

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
        if (!TargetSoldier || MinItems >= 10) break;

        for (const TSoftObjectPtr<UItemDefinition>& SoftItem : ItemDB->BuyableItems)
        {
            UItemDefinition* ItemDef = SoftItem.Get();
            if (!ItemDef) continue;
            if (ItemDef->ItemName.ToString() != DesiredName) continue;

            if (!Campaign->IsItemUnlocked(Faction, ItemDef)) continue;
            if (TargetSoldier->CurrentLoadout.Contains(ItemDef)) continue;

            FResourceStockpile Res = ResourceMgr->GetResources(Faction);
            if (Res.Money >= ItemDef->PurchaseCost.Money)
            {
                if (EngineeringMgr->PurchaseItem(Faction, ItemDef, TargetSoldier))
                {
                    UE_LOG(LogTemp, Display, TEXT("[AI] Bought %s on soldier %s (now has %d items)"),
                        *ItemDef->ItemName.ToString(), *TargetSoldier->SoldierName, TargetSoldier->CurrentLoadout.Num());

                    if (UStrategyEventDispatcher* EventDisp = GetGameInstance()->GetSubsystem<UStrategyEventDispatcher>())
                    {
                        EventDisp->OnSoldierLoadoutChanged.Broadcast(Faction, TargetSoldier);
                    }

                    bBoughtAnything = true;
                    PurchasesThisDay++;
                    break;
                }
            }
        }
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
        UE_LOG(LogTemp, Warning, TEXT("[AI] TryBuildFacility FAILED — Missing subsystems"));
        return false;
    }

    UFacilityDatabase* DB = Campaign->FacilityDatabaseAsset.Get();
    if (!DB)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AI] TryBuildFacility FAILED — FacilityDatabase is null!"));
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
        UE_LOG(LogTemp, Warning, TEXT("[AI] TryBuildFacility FAILED — No facility definition found for %s in database!"), *UEnum::GetValueAsString(FacilityTypeToBuild));
        return false;
    }

    UE_LOG(LogTemp, Display, TEXT("[AI] TryBuildFacility — Found definition for %s (MaxBuilt=%d, BuildTime=%d days)"),
        *FacilityDef->FacilityName.ToString(), FacilityDef->MaxBuilt, FacilityDef->BuildTimeDays);

    // === MaxBuilt check ===
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
            UE_LOG(LogTemp, Warning, TEXT("[AI] TryBuildFacility FAILED — MaxBuilt reached for %s (%d built + %d under construction)"),
                *UEnum::GetValueAsString(FacilityTypeToBuild), CurrentCountInBase, UnderConstruction);
            return false;
        }
    }

    UE_LOG(LogTemp, Display, TEXT("[AI] TryBuildFacility — MaxBuilt check PASSED"));

    // === Resource check ===
    if (!ResourceMgr->CanAfford(Faction, FacilityDef->BuildCost))
    {
        UE_LOG(LogTemp, Warning, TEXT("[AI] TryBuildFacility FAILED — Not enough resources for %s (needs %d Money, %d Metals, %d Biologicals, %d Chemicals)"),
            *FacilityDef->FacilityName.ToString(), FacilityDef->BuildCost.Money,
            FacilityDef->BuildCost.Metals, FacilityDef->BuildCost.Biologicals, FacilityDef->BuildCost.Chemicals);
        return false;
    }

    UE_LOG(LogTemp, Display, TEXT("[AI] TryBuildFacility — Resources check PASSED"));

    // === Actually build it ===
    if (BaseMgr->BuildFacility(Faction, FacilityDef, TargetBase))
    {
        UE_LOG(LogTemp, Display, TEXT("[AI] %s started construction of %s in base '%s' (%d days)"),
            *UEnum::GetValueAsString(Faction), *FacilityDef->FacilityName.ToString(),
            TargetBase ? *TargetBase->BaseName.ToString() : TEXT("default base"), FacilityDef->BuildTimeDays);
        return true;
    }

    UE_LOG(LogTemp, Warning, TEXT("[AI] TryBuildFacility FAILED — BuildFacility returned false for %s"),
        *FacilityDef->FacilityName.ToString());
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

    TArray<FString> TechPriority = {
        "Basic Melee Weapons Research",
        "Basic Ballistics Research",
        "Basic Armor Research",
        "Explosives T0 Research",
        "Basic Medical Supplies Research",
        "Rifle Mk1 Research"
    };

    for (const FString& DesiredName : TechPriority)
    {
        for (const TSoftObjectPtr<UResearchTechDefinition>& SoftResearch : ResearchDB->AvailableTechs)
        {
            UResearchTechDefinition* ResearchDef = SoftResearch.Get();
            if (!ResearchDef) continue;
            if (ResearchDef->ProjectName.ToString() != DesiredName) continue;

            if (ResearchMgr->IsResearchInProgress(Faction, ResearchDef) || ResearchMgr->HasCompletedResearch(Faction, ResearchDef))
                continue;

            if (Res.Money < ResearchDef->ResearchCost.Money)
                continue;

            if (ResearchMgr->StartResearch(Faction, ResearchDef))
            {
                UE_LOG(LogTemp, Display, TEXT("[AI] %s started research: %s (%d days)"),
                    *UEnum::GetValueAsString(Faction), *ResearchDef->ProjectName.ToString(), ResearchDef->ResearchDays);
                return true;
            }
        }
    }

    UE_LOG(LogTemp, Verbose, TEXT("[RESEARCH] No suitable tech found this day"));
    return false;
}

void UAIControllerSubsystem::SetAIEnabled(bool bEnable)
{
    bAIEnabled = bEnable;
    UE_LOG(LogTemp, Display, TEXT("AI Controller %s for both factions"), bAIEnabled ? TEXT("ENABLED") : TEXT("DISABLED"));
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

void UAIControllerSubsystem::Debug_RunAI()
{
    UE_LOG(LogTemp, Display, TEXT("[AI DEBUG] Manual AI run requested for BOTH factions (if enabled)"));

    if (bSimulateHumanAI)
    {
        UE_LOG(LogTemp, Display, TEXT("[AI DEBUG] → Running Human AI"));
        RunAIForFaction(EFactionType::Human, 999);
    }

    if (bSimulateEnemyAI)
    {
        UE_LOG(LogTemp, Display, TEXT("[AI DEBUG] → Running Enemy AI"));
        RunAIForFaction(EFactionType::Enemy, 999);
    }
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
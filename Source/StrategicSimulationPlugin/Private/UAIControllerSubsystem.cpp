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
#include "StrategicSiteDefinition.h"
#include "StrategicSimulationTypes.h"
#include "Engine/Engine.h"

void UAIControllerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Collection.InitializeDependency<UTimeManagerSubsystem>();
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

void UAIControllerSubsystem::ResetDailyProcessingState()
{
    LastProcessedDayPerFaction.Empty();
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
    if (UTimeManagerSubsystem* TimeMgr = GetGameInstance()->GetSubsystem<UTimeManagerSubsystem>())
    {
        RunAIForFaction(Faction, TimeMgr->GetSimulationDayNumber());
    }
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

    UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>();
    UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();
    if (MissionMgr && Campaign && Campaign->bSalvageSitesEnabled && Campaign->bSalvageMissionsEnabled)
    {
        MissionMgr->LogSalvageOpportunitiesForFaction(Faction, CurrentDay);
    }

    // === PER-BASE DEVELOPMENT — FOCUS BASE ===
    BaseMgr->AdvanceFacilityConstruction(Faction);
    ResourceMgr->ApplyFacilityIncome(Faction);

    const TArray<UStrategyBase*>& AllBases = BaseMgr->GetBases(Faction);
    if (AllBases.Num() == 0) return;

    UStrategyBase* FocusBase = nullptr;
    for (UStrategyBase* B : AllBases)
    {
        if (!B->HasOperationalFacilityOfType(EFacilityType::Command))
        {
            FocusBase = B;
            break;
        }
    }
    if (!FocusBase) FocusBase = AllBases[0];

    UE_LOG(LogTemp, Display, TEXT("[AI] Developing focus base '%s' (Net Power: %d) — [PARALLEL wave build now active]"),
        *FocusBase->BaseName.ToString(), FocusBase->GetNetPower());

    // === FACILITY BUILD ORDER (unchanged) ===
    TArray<EFacilityType> DesiredOrder = {
        EFacilityType::Command,
        EFacilityType::LivingQuarters,
        EFacilityType::Laboratory,
        EFacilityType::Workshop,
        EFacilityType::Hanger,
        EFacilityType::Medical,
        EFacilityType::VehicleRepair,
        EFacilityType::Containment,
        EFacilityType::Autopsy
    };

    for (int32 i = AllBases.Num() - 1; i >= 0; --i)
    {
        UStrategyBase* B = AllBases[i];

        UE_LOG(LogTemp, Display, TEXT("[AI DEBUG] Checking build order for base '%s'"), *B->BaseName.ToString());

        for (EFacilityType FacType : DesiredOrder)
        {
            bool bShouldBuild = false;
            if (FacType == EFacilityType::Command)
            {
                bShouldBuild = !B->HasAnyFacilityOfType(EFacilityType::Command);
            }
            else if (FacType == EFacilityType::LivingQuarters)
            {
                bool bCoreLayerDone = B->HasOperationalFacilityOfType(EFacilityType::Laboratory);
                int32 CurrentBarracks = B->GetTotalBuiltOfType(EFacilityType::LivingQuarters);
                bShouldBuild = (CurrentBarracks < 6) && (bCoreLayerDone || CurrentBarracks == 0);
            }
            else if (FacType == EFacilityType::Containment || FacType == EFacilityType::Autopsy)
            {
                int32 TotalSoldiers = SoldierMgr ? SoldierMgr->GetRoster(Faction).Num() : 0;
                int32 CurrentBarracksCapacity = B->GetTotalBuiltOfType(EFacilityType::LivingQuarters) * 6;
                bool bNearMaxCapacity = (CurrentBarracksCapacity > 0) && (TotalSoldiers >= (CurrentBarracksCapacity * 0.8f));

                bool bHasPOWOrKIA = false;
                if (SoldierMgr)
                {
                    if (FacType == EFacilityType::Containment)
                        bHasPOWOrKIA = SoldierMgr->GetPOWRoster(Faction).Num() > 0;
                }

                bShouldBuild = bNearMaxCapacity || bHasPOWOrKIA;
            }
            else
            {
                bShouldBuild = !B->HasOperationalFacilityOfType(FacType);
            }

            if (bShouldBuild)
            {
                UE_LOG(LogTemp, Display, TEXT("[AI] Attempting to build %s in base '%s'"),
                    *UEnum::GetValueAsString(FacType), *B->BaseName.ToString());

                if (TryBuildFacility(Faction, FacType, B))
                {
                    UE_LOG(LogTemp, Display, TEXT("[AI] %s built %s in base '%s'"),
                        *UEnum::GetValueAsString(Faction), *UEnum::GetValueAsString(FacType), *B->BaseName.ToString());
                    break;
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("[AI] TryBuildFacility FAILED for %s in '%s' — check resources/prereqs"),
                        *UEnum::GetValueAsString(FacType), *B->BaseName.ToString());
                }
            }
        }
    }

    // === VEHICLE BUILD (unchanged) ===
    if (FocusBase->HasOperationalFacilityOfType(EFacilityType::Hanger))
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

    // === DAILY MISSION SCHEDULING — one slot per idle vehicle per base, spread across 24h ===
    UEngineeringManagerSubsystem* EngMgr = GetGameInstance()->GetSubsystem<UEngineeringManagerSubsystem>();
    if (!MissionMgr)
    {
        MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>();
    }
    if (!Campaign)
    {
        Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();
    }

    int32 TotalScheduled = 0;

    for (UStrategyBase* Base : AllBases)
    {
        if (!Base || !Base->HasOperationalFacilityOfType(EFacilityType::Hanger) || !MissionMgr)
        {
            continue;
        }

        const TArray<UStrategyVehicle*> IdleVehicles = MissionMgr->GatherIdleVehiclesAtBase(Base);
        if (IdleVehicles.Num() == 0)
        {
            continue;
        }

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

                for (UStrategyVehicle* Vehicle : IdleVehicles)
                {
                    const bool bWantsWeapons = Vehicle->VehicleDefinition
                        && IsCombatVehicleType(Vehicle->VehicleDefinition->VehicleType);

                    if (bWantsWeapons
                        && Vehicle->GetEquippedWeapons().Num() < Vehicle->GetMaxWeaponSlots()
                        && AvailableWeapon)
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

        TArray<EMissionType> MissionTypes;
        MissionTypes.Reserve(IdleVehicles.Num());
        for (UStrategyVehicle* Vehicle : IdleVehicles)
        {
            MissionTypes.Add(PickAIMissionTypeForVehicle(Vehicle, CurrentDay));
        }

        const int32 ScheduledAtBase = MissionMgr->ScheduleVehicleMissionsForBase(Base, Faction, MissionTypes);
        TotalScheduled += ScheduledAtBase;

        if (ScheduledAtBase > 0)
        {
            UE_LOG(LogTemp, Display, TEXT("[AI] %s scheduled %d vehicle mission(s) from base '%s' across the day"),
                *UEnum::GetValueAsString(Faction), ScheduledAtBase, *Base->BaseName.ToString());
        }
    }

    if (TotalScheduled == 0)
    {
        UE_LOG(LogTemp, Verbose, TEXT("[AI] %s — No idle vehicles to schedule today"), *UEnum::GetValueAsString(Faction));
    }

    // === DAILY ROUTINES (unchanged) ===
    bool bRecruited = (SoldierMgr && TryRecruit(Faction));
    if (bRecruited) UE_LOG(LogTemp, Display, TEXT("[AI] %s recruited soldier"), *UEnum::GetValueAsString(Faction));

    if (ResearchMgr && TryResearch(Faction)) UE_LOG(LogTemp, Display, TEXT("[AI] %s started research"), *UEnum::GetValueAsString(Faction));
    if (TryBuyAndEquip(Faction)) UE_LOG(LogTemp, Display, TEXT("[AI] %s purchase/equip action taken"), *UEnum::GetValueAsString(Faction));
    if (EngineeringMgr && EngineeringMgr->TryProduce(Faction)) UE_LOG(LogTemp, Display, TEXT("[AI] %s production action taken"), *UEnum::GetValueAsString(Faction));

    if (BaseMgr && Campaign && Campaign->bVerboseLogging)
    {
        BaseMgr->DebugPrintFullBaseState(Faction);
    }

    // === EXPANSION (unchanged) ===
    if (AllBases.Num() < MaxBases)
    {
        bool bAllBasesHaveVehicle = true;
        UE_LOG(LogTemp, Display, TEXT("[EXPANSION DEBUG] %s — Checking expansion: Bases = %d (max %d)"),
            *UEnum::GetValueAsString(Faction), AllBases.Num(), MaxBases);

        for (UStrategyBase* B : AllBases)
        {
            bool bThisBaseHasVehicle = false;

            if (B->HasOperationalFacilityOfType(EFacilityType::Hanger))
            {
                for (UStrategyFacility* Fac : B->Facilities)
                {
                    if (Fac && Fac->FacilityDefinition && Fac->FacilityDefinition->FacilityType == EFacilityType::Hanger)
                    {
                        if (Fac->ParkedVehicles.Num() > 0)
                        {
                            bThisBaseHasVehicle = true;
                            break;
                        }
                    }
                }
            }

            if (!bThisBaseHasVehicle && MissionMgr)
            {
                for (UMissionGroup* Mission : MissionMgr->ActiveMissions)
                {
                    if (Mission && Mission->OriginBase == B && Mission->VehiclesInFleet.Num() > 0)
                    {
                        bThisBaseHasVehicle = true;
                        break;
                    }
                }
            }

            if (!bThisBaseHasVehicle)
            {
                bAllBasesHaveVehicle = false;
                UE_LOG(LogTemp, Display, TEXT("[EXPANSION DEBUG]   Base '%s' does NOT own a vehicle yet"), *B->BaseName.ToString());
                break;
            }
        }

        if (bAllBasesHaveVehicle && ResourceMgr->GetResources(Faction).Money > 9500)
        {
            UE_LOG(LogTemp, Display, TEXT("[AI] %s — EXPANSION TRIGGERED! ALL bases own vehicles → Building NEW base #%d"),
                *UEnum::GetValueAsString(Faction), AllBases.Num() + 1);

            FString NewName = FString::Printf(TEXT("Forward Base %02d"), AllBases.Num() + 1);

            // Try to expand onto a discovered site
            if (UStrategySiteDefinition* TargetSite = FindExpansionSiteForAI(Faction))
            {
                if (BaseMgr->TryBuildBaseOnSite(Faction, TargetSite, FText::FromString(NewName)))
                {
                    UE_LOG(LogTemp, Display, TEXT("[AI] %s successfully expanded onto discovered site: %s"),
                        *UEnum::GetValueAsString(Faction), *TargetSite->SiteName);
                }
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("[AI] %s wants to expand but has no valid discovered sites available."),
                    *UEnum::GetValueAsString(Faction));
            }
        }
    }
    else
    {
        UE_LOG(LogTemp, Verbose, TEXT("[EXPANSION DEBUG] %s — At maximum bases (%d)"), *UEnum::GetValueAsString(Faction), MaxBases);
    }

    UE_LOG(LogTemp, Display, TEXT("[AI] %s AI — End of day %d (actions completed)"), *UEnum::GetValueAsString(Faction), CurrentDay);
}

// === FULL FUNCTION: UAIControllerSubsystem::TryRecruit (CAPACITY FIXED - FINAL) ===
// Now checks capacity AFTER every soldier is queued. 
// Never exceeds barracks capacity, even during large waves.
bool UAIControllerSubsystem::TryRecruit(EFactionType Faction)
{
    UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    UResourceManagerSubsystem* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>();
    USoldierManagerSubsystem* SoldierMgr = GetGameInstance()->GetSubsystem<USoldierManagerSubsystem>();
    UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();

    if (!BaseMgr || !ResourceMgr || !SoldierMgr || !Campaign)
    {
        UE_LOG(LogTemp, Warning, TEXT("[RECRUIT] Missing required subsystems!"));
        return false;
    }

    // === CLASS PICK — SKIP INDEX 0 (Commander is reserved) ===
    USoldierClassDefinition* ClassDef = nullptr;
    if (Campaign->SoldierClassDatabaseAsset.IsValid())
    {
        if (USoldierClassDatabase* DB = Campaign->SoldierClassDatabaseAsset.Get())
        {
            if (DB->AvailableSoldierClasses.Num() > 1)
            {
                int32 RandomIndex = FMath::RandRange(1, DB->AvailableSoldierClasses.Num() - 1);
                ClassDef = DB->AvailableSoldierClasses[RandomIndex].Get();
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("[RECRUIT] Only Commander class exists — add more classes to database"));
                return false;
            }
        }
    }

    if (!ClassDef)
    {
        UE_LOG(LogTemp, Warning, TEXT("[RECRUIT] No soldier class definition found!"));
        return false;
    }

    const FResourceStockpile& Cost = ClassDef->TrainingCost;

    int32 RecruitedThisDay = 0;
    const int32 MaxPerDay = 999;

    while (RecruitedThisDay < MaxPerDay)
    {
        // Re-check capacity every iteration (this fixes the over-recruitment)
        int32 CurrentSoldiers = SoldierMgr->GetRoster(Faction).Num();
        if (CurrentSoldiers >= BaseMgr->GetTotalBarracksCapacity(Faction))
        {
            UE_LOG(LogTemp, Display, TEXT("[RECRUIT] %s reached max capacity mid-wave — queued %d soldiers today"),
                *UEnum::GetValueAsString(Faction), RecruitedThisDay);
            break;
        }

        const TArray<UStrategyBase*>& Bases = BaseMgr->GetBases(Faction);
        UStrategyBase* BestBase = nullptr;
        int32 LowestStationed = INT_MAX;

        for (int32 i = Bases.Num() - 1; i >= 0; --i)
        {
            UStrategyBase* Base = Bases[i];
            if (!Base) continue;

            int32 Stationed = 0;
            for (UStrategySoldier* Soldier : SoldierMgr->GetRoster(Faction))
            {
                if (Soldier && Soldier->StationedBase == Base)
                    Stationed++;
            }

            bool bHasFreeSlot = false;
            for (UStrategyFacility* Barracks : Base->Facilities)
            {
                if (Barracks && Barracks->FacilityDefinition &&
                    Barracks->FacilityDefinition->FacilityType == EFacilityType::LivingQuarters &&
                    Barracks->HasFreeProductionSlot())
                {
                    bHasFreeSlot = true;
                    break;
                }
            }

            if (bHasFreeSlot && Stationed < LowestStationed)
            {
                LowestStationed = Stationed;
                BestBase = Base;
            }
        }

        if (!BestBase)
        {
            UE_LOG(LogTemp, Verbose, TEXT("[RECRUIT] No base with free barracks slot found for %s"), *UEnum::GetValueAsString(Faction));
            break;
        }

        if (!ResourceMgr->CanAfford(Faction, Cost))
        {
            UE_LOG(LogTemp, Verbose, TEXT("[RECRUIT] %s ran out of resources mid-wave — queued %d soldiers today"),
                *UEnum::GetValueAsString(Faction), RecruitedThisDay);
            break;
        }

        bool bQueued = false;
        for (UStrategyFacility* Barracks : BestBase->Facilities)
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

                    UE_LOG(LogTemp, Display, TEXT("[AI] %s WAVE RECRUIT — queued %s in %s at base '%s' (slots left: %d)"),
                        *UEnum::GetValueAsString(Faction), *ClassDef->ClassName.ToString(),
                        *Barracks->FacilityDefinition->FacilityName.ToString(),
                        *BestBase->BaseName.ToString(),
                        Barracks->GetAvailableProductionSlots());

                    RecruitedThisDay++;
                    bQueued = true;
                    break;
                }
            }
        }

        if (!bQueued) break;
    }

    UE_LOG(LogTemp, Display, TEXT("[RECRUIT] %s wave complete — recruited %d soldiers today"),
        *UEnum::GetValueAsString(Faction), RecruitedThisDay);

    return RecruitedThisDay > 0;
}

// === FULL FUNCTION: UAIControllerSubsystem::TryBuyAndEquip (FINAL - CRASH PROOF) ===
// Works with your new Commander class (MaxLoadoutSize is respected).
// Re-sorts after every purchase + daily cap = no more runaway loops or crashes.
bool UAIControllerSubsystem::TryBuyAndEquip(EFactionType Faction)
{
    UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();
    USoldierManagerSubsystem* SoldierMgr = GetGameInstance()->GetSubsystem<USoldierManagerSubsystem>();
    UEngineeringManagerSubsystem* EngineeringMgr = GetGameInstance()->GetSubsystem<UEngineeringManagerSubsystem>();
    UResourceManagerSubsystem* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>();
    UItemDatabase* ItemDB = Campaign ? Campaign->ItemDatabaseAsset.Get() : nullptr;

    if (!SoldierMgr || !EngineeringMgr || !ResourceMgr || !ItemDB)
    {
        UE_LOG(LogTemp, Warning, TEXT("[PURCHASE] Missing subsystems!"));
        return false;
    }

    TArray<UStrategySoldier*> Roster = SoldierMgr->GetRoster(Faction);
    if (Roster.Num() == 0) return false;

    UE_LOG(LogTemp, Display, TEXT("[PURCHASE] === %s starting buy round (smart priority) ==="), *UEnum::GetValueAsString(Faction));

    int32 ItemsBoughtThisDay = 0;
    const int32 MaxItemsPerDay = 12;   // safe for UI performance

    TArray<FString> ItemPriority = {
        "Basic Armor", "M-16 Rifle", "Pistol", "Healthpack",
        "Grenade", "Proximity Bomb", "Knife"
    };

    bool bBoughtAnything = false;

    while (ItemsBoughtThisDay < MaxItemsPerDay)
    {
        // Re-sort every purchase so gear spreads evenly across all soldiers
        Roster.Sort([](const UStrategySoldier& A, const UStrategySoldier& B) {
            return A.CurrentLoadout.Num() < B.CurrentLoadout.Num();
        });

        bool bFoundPurchase = false;

        for (UStrategySoldier* Soldier : Roster)
        {
            if (!Soldier || !Soldier->ClassDefinition) continue;

            if (Soldier->CurrentLoadout.Num() >= Soldier->ClassDefinition->MaxLoadoutSize) continue;

            for (const FString& DesiredName : ItemPriority)
            {
                for (const TSoftObjectPtr<UItemDefinition>& SoftItem : ItemDB->BuyableItems)
                {
                    UItemDefinition* ItemDef = SoftItem.Get();
                    if (!ItemDef) continue;
                    if (ItemDef->ItemName.ToString() != DesiredName) continue;

                    if (!Soldier->ClassDefinition->AllowedItems.Contains(SoftItem)) continue;

                    // FIXED: Compare against the SoftObjectPtr that is actually stored in CurrentLoadout
                    if (Soldier->CurrentLoadout.Contains(SoftItem)) continue;

                    if (!Campaign->IsItemUnlocked(Faction, ItemDef)) continue;
                    if (!ResourceMgr->CanAfford(Faction, ItemDef->PurchaseCost)) continue;

                    if (EngineeringMgr->PurchaseItem(Faction, ItemDef, Soldier))
                    {
                        UE_LOG(LogTemp, Display, TEXT("[AI] Bought %s on %s (%s) (now has %d/%d items)"),
                            *ItemDef->ItemName.ToString(), *Soldier->SoldierName,
                            *Soldier->ClassDefinition->ClassName.ToString(),
                            Soldier->CurrentLoadout.Num(), Soldier->ClassDefinition->MaxLoadoutSize);

                        if (UStrategyEventDispatcher* EventDisp = GetGameInstance()->GetSubsystem<UStrategyEventDispatcher>())
                            EventDisp->OnSoldierLoadoutChanged.Broadcast(Faction, Soldier);

                        bBoughtAnything = true;
                        bFoundPurchase = true;
                        ItemsBoughtThisDay++;
                        break;
                    }
                }
                if (bFoundPurchase) break;
            }
            if (bFoundPurchase) break;
        }

        if (!bFoundPurchase)
        {
            UE_LOG(LogTemp, Display, TEXT("[PURCHASE] %s outfitting wave complete — bought %d items today (capped at %d)"),
                *UEnum::GetValueAsString(Faction), ItemsBoughtThisDay, MaxItemsPerDay);
            break;
        }
    }

    return bBoughtAnything;
}

// === FULL FUNCTION: UAIControllerSubsystem::TryBuildVehicle (WAVE VERSION) ===
// Same wave logic as TryRecruit. Hangars now act as their own controllers —
// multiple vehicles can be started in one day if multiple slots exist.
bool UAIControllerSubsystem::TryBuildVehicle(EFactionType Faction, UStrategyBase* TargetBase)
{
    if (!TargetBase)
        return false;

    UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();
    if (!Campaign) return false;

    UVehicleDatabase* VehicleDB = Campaign->VehicleDatabaseAsset.Get();
    if (!VehicleDB || VehicleDB->AvailableVehicles.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AI] No vehicles available in database for %s"), *UEnum::GetValueAsString(Faction));
        return false;
    }

    UVehicleDefinition* VehDef = SelectVehicleDefinitionToBuild(Faction);
    if (!VehDef) return false;

    UResourceManagerSubsystem* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>();
    if (!ResourceMgr) return false;

    // === WAVE LOOP START ===
    int32 BuiltThisDay = 0;
    const int32 MaxPerDay = 999;

    while (BuiltThisDay < MaxPerDay)
    {
        if (!ResourceMgr->CanAfford(Faction, VehDef->BuildCost))
        {
            UE_LOG(LogTemp, Verbose, TEXT("[AI] %s ran out of resources mid-vehicle wave — built %d vehicles today"),
                *UEnum::GetValueAsString(Faction), BuiltThisDay);
            break;
        }

        bool bQueued = false;
        for (UStrategyFacility* Hanger : TargetBase->Facilities)
        {
            if (Hanger && Hanger->FacilityDefinition &&
                Hanger->FacilityDefinition->FacilityType == EFacilityType::Hanger &&
                Hanger->HasFreeProductionSlot())
            {
                if (Hanger->StartProduction(EProductionType::Vehicle, VehDef, VehDef->ProductionDays))
                {
                    ResourceMgr->AddResources(Faction, {
                        -VehDef->BuildCost.Money,
                        -VehDef->BuildCost.Metals,
                        -VehDef->BuildCost.Biologicals,
                        -VehDef->BuildCost.Chemicals,
                        0, 0 });

                    UE_LOG(LogTemp, Display, TEXT("[AI] %s WAVE VEHICLE — queued '%s' in hanger at base '%s' (slots left: %d)"),
                        *UEnum::GetValueAsString(Faction), *VehDef->VehicleName.ToString(),
                        *TargetBase->BaseName.ToString(),
                        Hanger->GetAvailableProductionSlots());

                    BuiltThisDay++;
                    bQueued = true;
                    break;
                }
            }
        }

        if (!bQueued)
            break;  // no more free hangar slots in this base
    }

    UE_LOG(LogTemp, Display, TEXT("[AI] %s vehicle wave complete — built %d vehicles today"),
        *UEnum::GetValueAsString(Faction), BuiltThisDay);

    return BuiltThisDay > 0;
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

    // === PER-BASE MaxBuilt check (this is the critical fix) ===
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
            UE_LOG(LogTemp, Warning, TEXT("[AI] TryBuildFacility FAILED — MaxBuilt reached for %s in base '%s' (%d built + %d under construction)"),
                *UEnum::GetValueAsString(FacilityTypeToBuild), *TargetBase->BaseName.ToString(), CurrentCountInBase, UnderConstruction);
            return false;
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[AI] TryBuildFacility FAILED — No TargetBase provided!"));
        return false;
    }

    UE_LOG(LogTemp, Display, TEXT("[AI] TryBuildFacility — MaxBuilt check PASSED (per-base)"));

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

UStrategySiteDefinition* UAIControllerSubsystem::FindExpansionSiteForAI(EFactionType Faction) const
{
    UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    if (!BaseMgr) return nullptr;

    const TArray<UStrategySiteDefinition*>& DiscoveredSites = (Faction == EFactionType::Human)
        ? BaseMgr->DiscoveredSitesHuman
        : BaseMgr->DiscoveredSitesEnemy;

    for (UStrategySiteDefinition* Site : DiscoveredSites)
    {
        if (Site && !Site->bHasBeenUsed && BaseMgr->CanBuildBaseOnSite(Faction, Site))
        {
            return Site;
        }
    }

    return nullptr;
}

bool UAIControllerSubsystem::IsReconVehicleType(EVehicleType Type)
{
    return Type == EVehicleType::Scout || Type == EVehicleType::Transport || Type == EVehicleType::Support;
}

bool UAIControllerSubsystem::IsCombatVehicleType(EVehicleType Type)
{
    return Type == EVehicleType::Gunship || Type == EVehicleType::Heavy;
}

int32 UAIControllerSubsystem::CountFactionVehiclesOfTypes(EFactionType Faction, const TArray<EVehicleType>& Types) const
{
    UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>();
    if (!BaseMgr)
    {
        return 0;
    }

    int32 Count = 0;
    TSet<UStrategyVehicle*> Counted;

    auto CountIfMatching = [&](UStrategyVehicle* Vehicle)
    {
        if (!Vehicle || Vehicle->IsDestroyed() || Counted.Contains(Vehicle))
        {
            return;
        }

        if (Vehicle->VehicleDefinition && Types.Contains(Vehicle->VehicleDefinition->VehicleType))
        {
            Counted.Add(Vehicle);
            Count++;
        }
    };

    for (UStrategyBase* Base : BaseMgr->GetBases(Faction))
    {
        if (!Base)
        {
            continue;
        }

        for (UStrategyFacility* Facility : Base->Facilities)
        {
            if (!Facility || !Facility->FacilityDefinition || Facility->FacilityDefinition->FacilityType != EFacilityType::Hanger)
            {
                continue;
            }

            for (UStrategyVehicle* Vehicle : Facility->ParkedVehicles)
            {
                CountIfMatching(Vehicle);
            }
        }
    }

    if (MissionMgr)
    {
        for (UMissionGroup* Mission : MissionMgr->ActiveMissions)
        {
            if (!Mission || !Mission->OriginBase || Mission->OriginBase->OwningFaction != Faction)
            {
                continue;
            }

            for (UStrategyVehicle* Vehicle : Mission->VehiclesInFleet)
            {
                CountIfMatching(Vehicle);
            }
        }
    }

    return Count;
}

UVehicleDefinition* UAIControllerSubsystem::SelectVehicleDefinitionToBuild(EFactionType Faction) const
{
    UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();
    UResourceManagerSubsystem* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>();
    if (!Campaign || !ResourceMgr)
    {
        return nullptr;
    }

    UVehicleDatabase* VehicleDB = Campaign->VehicleDatabaseAsset.Get();
    if (!VehicleDB || VehicleDB->AvailableVehicles.Num() == 0)
    {
        return nullptr;
    }

    TArray<UVehicleDefinition*> ScoutDefs;
    TArray<UVehicleDefinition*> CombatDefs;
    TArray<UVehicleDefinition*> OtherDefs;

    for (const TSoftObjectPtr<UVehicleDefinition>& SoftDef : VehicleDB->AvailableVehicles)
    {
        UVehicleDefinition* Def = SoftDef.Get();
        if (!Def)
        {
            continue;
        }

        if (IsReconVehicleType(Def->VehicleType))
        {
            ScoutDefs.Add(Def);
        }
        else if (IsCombatVehicleType(Def->VehicleType))
        {
            CombatDefs.Add(Def);
        }
        else
        {
            OtherDefs.Add(Def);
        }
    }

    const int32 ScoutCount = CountFactionVehiclesOfTypes(Faction, { EVehicleType::Scout, EVehicleType::Transport, EVehicleType::Support });
    const int32 CombatCount = CountFactionVehiclesOfTypes(Faction, { EVehicleType::Gunship, EVehicleType::Heavy });

    auto FindFirstAffordable = [&](const TArray<UVehicleDefinition*>& Candidates) -> UVehicleDefinition*
    {
        for (UVehicleDefinition* Def : Candidates)
        {
            if (Def && ResourceMgr->CanAfford(Faction, Def->BuildCost))
            {
                return Def;
            }
        }
        return nullptr;
    };

    UVehicleDefinition* Chosen = nullptr;
    if (ScoutCount < 1)
    {
        Chosen = FindFirstAffordable(ScoutDefs);
    }
    else if (CombatCount < ScoutCount)
    {
        Chosen = FindFirstAffordable(CombatDefs);
    }

    if (!Chosen)
    {
        Chosen = FindFirstAffordable(CombatDefs);
    }
    if (!Chosen)
    {
        Chosen = FindFirstAffordable(ScoutDefs);
    }
    if (!Chosen)
    {
        Chosen = FindFirstAffordable(OtherDefs);
    }

    if (Chosen)
    {
        UE_LOG(LogTemp, Display, TEXT("[AI] %s selected vehicle '%s' (%s) — scouts: %d, combat: %d"),
            *UEnum::GetValueAsString(Faction), *Chosen->VehicleName.ToString(),
            *UEnum::GetValueAsString(Chosen->VehicleType), ScoutCount, CombatCount);
    }

    return Chosen;
}

EMissionType UAIControllerSubsystem::PickAIMissionTypeForVehicle(UStrategyVehicle* Vehicle, int32 CurrentDay) const
{
    if (!Vehicle || !Vehicle->VehicleDefinition)
    {
        return EMissionType::Recon;
    }

    UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();
    UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>();
    const int32 OffensiveStartDay = Campaign ? Campaign->OffensiveMissionsStartDay : 5;

    if (IsReconVehicleType(Vehicle->VehicleDefinition->VehicleType))
    {
        if (Campaign && Campaign->bSalvageMissionsEnabled && Campaign->bSalvageSitesEnabled && MissionMgr)
        {
            UStrategySiteDefinition* SalvageSite = nullptr;
            float SalvageScore = 0.0f;
            if (MissionMgr->EvaluateAISalvageScheduling(Vehicle, SalvageSite, SalvageScore) && SalvageSite)
            {
                const EFactionType Faction = Vehicle->HomeBase ? Vehicle->HomeBase->OwningFaction : EFactionType::Neutral;
                UE_LOG(LogTemp, Display,
                    TEXT("[SALVAGE AI] %s scheduling salvage to site %s score=%.1f wreck='%s'"),
                    *UEnum::GetValueAsString(Faction),
                    *SalvageSite->SiteId.ToString(EGuidFormats::Short),
                    SalvageScore,
                    *SalvageSite->SiteName);
                return EMissionType::Salvage;
            }
        }

        return EMissionType::Recon;
    }

    if (IsCombatVehicleType(Vehicle->VehicleDefinition->VehicleType) && MissionMgr && MissionMgr->HasOffensiveTargetInRange(Vehicle))
    {
        return EMissionType::Offensive;
    }

    return EMissionType::Recon;
}

bool UAIControllerSubsystem::ShouldEngageVehicle(UStrategyVehicle* DetectingVehicle, UStrategyVehicle* DetectedVehicle) const
{
    if (!DetectingVehicle || !DetectedVehicle || DetectedVehicle->IsDestroyed())
    {
        return false;
    }

    if (!DetectingVehicle->HomeBase || !DetectedVehicle->HomeBase)
    {
        return false;
    }

    if (DetectingVehicle->HomeBase->OwningFaction == DetectedVehicle->HomeBase->OwningFaction)
    {
        return false;
    }

    UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();
    const int32 MinOffense = Campaign ? Campaign->MinOffenseToEngage : 10;
    if (DetectingVehicle->GetVehicleOffensiveRating() < MinOffense)
    {
        return false;
    }

    if (DetectingVehicle->VehicleDefinition && IsCombatVehicleType(DetectingVehicle->VehicleDefinition->VehicleType))
    {
        return true;
    }

    if (DetectingVehicle->CurrentMission)
    {
        const EMissionType MissionType = DetectingVehicle->CurrentMission->MissionType;
        if (MissionType == EMissionType::Offensive
            || MissionType == EMissionType::Interception
            || MissionType == EMissionType::Defensive)
        {
            return true;
        }
    }

    return false;
}

bool UAIControllerSubsystem::ShouldPrioritizeEnRouteIntercept(UStrategyVehicle* DetectingVehicle,
    UStrategyVehicle* DetectedVehicle) const
{
    if (!DetectingVehicle || !DetectedVehicle)
    {
        return false;
    }

    auto IsInboundStrike = [](const UStrategyVehicle* Vehicle) -> bool
    {
        if (!Vehicle || !Vehicle->CurrentMission || !Vehicle->CurrentMission->bMovementActivated)
        {
            return false;
        }

        return Vehicle->CurrentMission->MissionType == EMissionType::Offensive
            || Vehicle->CurrentMission->MissionType == EMissionType::Interception;
    };

    return IsInboundStrike(DetectingVehicle) || IsInboundStrike(DetectedVehicle);
}

void UAIControllerSubsystem::HandleVehicleDetection(UStrategyVehicle* DetectingVehicle, UStrategyVehicle* DetectedVehicle)
{
    if (!DetectingVehicle || !DetectedVehicle) return;

    const bool bCanEngage = ShouldEngageVehicle(DetectingVehicle, DetectedVehicle);
    const bool bInterceptPriority = ShouldPrioritizeEnRouteIntercept(DetectingVehicle, DetectedVehicle);

    UE_LOG(LogTemp, Display, TEXT("[DETECT] %s detected enemy vehicle %s (offense: %d, engage: %s%s)"),
        DetectingVehicle->VehicleDefinition ? *DetectingVehicle->VehicleDefinition->VehicleName.ToString() : *GetNameSafe(DetectingVehicle),
        DetectedVehicle->VehicleDefinition ? *DetectedVehicle->VehicleDefinition->VehicleName.ToString() : *GetNameSafe(DetectedVehicle),
        DetectingVehicle->GetVehicleOffensiveRating(),
        bCanEngage ? TEXT("yes") : TEXT("no"),
        bInterceptPriority ? TEXT(", en-route intercept") : TEXT(""));

    if (bCanEngage)
    {
        UE_LOG(LogTemp, Display, TEXT("%s %s attacking %s"),
            bInterceptPriority ? TEXT("[COMBAT] En-route intercept:") : TEXT("[COMBAT] Engagement started:"),
            DetectingVehicle->VehicleDefinition ? *DetectingVehicle->VehicleDefinition->VehicleName.ToString() : *GetNameSafe(DetectingVehicle),
            DetectedVehicle->VehicleDefinition ? *DetectedVehicle->VehicleDefinition->VehicleName.ToString() : *GetNameSafe(DetectedVehicle));
        DetectingVehicle->SetBehavior(EVehicleBehavior::Attacking, DetectedVehicle);
    }

    if (ShouldEngageVehicle(DetectedVehicle, DetectingVehicle))
    {
        UE_LOG(LogTemp, Display, TEXT("[COMBAT] Counter-engagement: %s attacking %s"),
            DetectedVehicle->VehicleDefinition ? *DetectedVehicle->VehicleDefinition->VehicleName.ToString() : *GetNameSafe(DetectedVehicle),
            DetectingVehicle->VehicleDefinition ? *DetectingVehicle->VehicleDefinition->VehicleName.ToString() : *GetNameSafe(DetectingVehicle));
        DetectedVehicle->SetBehavior(EVehicleBehavior::Attacking, DetectingVehicle);
    }
}
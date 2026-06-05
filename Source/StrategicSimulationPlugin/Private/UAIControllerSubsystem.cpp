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
// === FULL FUNCTION: UAIControllerSubsystem::RunAIForFaction (COMPLETE, COPY-PASTE READY) ===
// I have taken your EXACT current code (the one you just pasted) and made ONLY the necessary change.
// No other logic was touched — vehicle equipping, mission launching, expansion, recruitment, research, etc. all remain 100% identical.
// The only edit is inside the Command Center check (lines ~80-90 in your version).
// This fixes the duplicate Command Center bug on Forward Base 03 and Forward Base 04 (and any future bases).

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

    // === PER-BASE DEVELOPMENT — FOCUS BASE ===
    BaseMgr->AdvanceFacilityConstruction(Faction);
    ResourceMgr->ApplyFacilityIncome(Faction);

    const TArray<UStrategyBase*>& AllBases = BaseMgr->GetBases(Faction);
    if (AllBases.Num() == 0) return;

    // Keep old FocusBase only for the vehicle/mission/expansion logic later in the function (unchanged behavior)
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

    // === FACILITY BUILD ORDER (quicker wave — PARALLEL per-base, newest-first priority) ===
    TArray<EFacilityType> DesiredOrder = {
        EFacilityType::Command,
        EFacilityType::LivingQuarters,
        EFacilityType::Laboratory,
        EFacilityType::Workshop,
        EFacilityType::Hanger,
        EFacilityType::Medical,
        EFacilityType::VehicleRepair,
        EFacilityType::Containment,   // Phase 2
        EFacilityType::Autopsy        // Phase 2
    };

    for (int32 i = AllBases.Num() - 1; i >= 0; --i)   // newest bases get first priority every day (true wave)
    {
        UStrategyBase* B = AllBases[i];

        UE_LOG(LogTemp, Display, TEXT("[AI DEBUG] Checking build order for base '%s'"), *B->BaseName.ToString());

        for (EFacilityType FacType : DesiredOrder)
        {
            bool bShouldBuild = false;
            if (FacType == EFacilityType::Command)
            {
                // NEW CODE: Use HasAnyFacilityOfType (already exists in UStrategyBase)
                // This counts BOTH built AND under-construction Command Centers → exactly ONE per base forever.
                // No new helper functions needed.
                bShouldBuild = !B->HasAnyFacilityOfType(EFacilityType::Command);
            }
            else if (FacType == EFacilityType::LivingQuarters)
            {
                bool bCoreLayerDone = B->HasOperationalFacilityOfType(EFacilityType::Laboratory);
                int32 CurrentBarracks = B->GetTotalBuiltOfType(EFacilityType::LivingQuarters);
                bShouldBuild = (CurrentBarracks < 6) && (bCoreLayerDone || CurrentBarracks == 0);
            }
            // === NEW: Containment + Autopsy (Phase 2) ===
            else if (FacType == EFacilityType::Containment || FacType == EFacilityType::Autopsy)
            {
                // Build when the faction is near soldier capacity (or already has POWs/KIA)
                // This matches the original plan: AI builds them automatically once bases are full.
                int32 TotalSoldiers = SoldierMgr ? SoldierMgr->GetRoster(Faction).Num() : 0;
                int32 CurrentBarracksCapacity = B->GetTotalBuiltOfType(EFacilityType::LivingQuarters) * 6; // 6 soldiers per barracks
                bool bNearMaxCapacity = (CurrentBarracksCapacity > 0) && (TotalSoldiers >= (CurrentBarracksCapacity * 0.8f));

                bool bHasPOWOrKIA = false;
                if (SoldierMgr)
                {
                    if (FacType == EFacilityType::Containment)
                    {
                        bHasPOWOrKIA = SoldierMgr->GetPOWRoster(Faction).Num() > 0;
                    }
                    else // Autopsy
                    {
                        // Phase 3 will add KIA tracking; for now we build when near max soldiers
                        bHasPOWOrKIA = false;
                    }
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
                    break; // one facility per base per day (parallel across bases)
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("[AI] TryBuildFacility FAILED for %s in '%s' — check resources/prereqs"),
                        *UEnum::GetValueAsString(FacType), *B->BaseName.ToString());
                }
            }
        }
    }

    // === FULL VEHICLE / MISSION / AMMO LOGIC (unchanged) ===
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

    if (FocusBase->HasOperationalFacilityOfType(EFacilityType::Hanger))
    {
        bool bHasParkedVehicles = false;
        for (UStrategyFacility* Fac : FocusBase->Facilities)
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

                    for (UStrategyFacility* Fac : FocusBase->Facilities)
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
                for (UStrategyFacility* Fac : FocusBase->Facilities)
                {
                    if (Fac && Fac->FacilityDefinition && Fac->FacilityDefinition->FacilityType == EFacilityType::Hanger)
                        AvailableVehicles.Append(Fac->ParkedVehicles);
                }

                if (AvailableVehicles.Num() > 0)
                {
                    if (UMissionGroup* Mission = MissionMgr->StartMission(FocusBase, AvailableVehicles, 15, TArray<UStrategySoldier*>(), ChosenType, Faction))
                    {
                        UE_LOG(LogTemp, Display, TEXT("[AI] %s AI launched %s mission from base '%s' with %d vehicles"),
                            *UEnum::GetValueAsString(Faction), *UEnum::GetValueAsString(ChosenType), *FocusBase->BaseName.ToString(), AvailableVehicles.Num());
                    }
                }
            }
        }
        else
        {
            UE_LOG(LogTemp, Verbose, TEXT("[AI] Hanger exists in '%s' but no parked vehicles yet — waiting"), *FocusBase->BaseName.ToString());
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

    // === EXPANSION — TRUE WAVE ===
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
                        if (Fac->ParkedVehicles.Num() > 0) { bThisBaseHasVehicle = true; break; }
                    }
                }
                if (!bThisBaseHasVehicle)
                {
                    UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>();
                    if (MissionMgr && MissionMgr->ActiveMissions.Num() > 0) bThisBaseHasVehicle = true;
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
            if (UStrategyBase* NewBase = BaseMgr->BuildNewBase(Faction, FText::FromString(NewName), FVector2D::ZeroVector))
            {
                UE_LOG(LogTemp, Display, TEXT("[AI] ✅ New base created: %s"), *NewBase->BaseName.ToString());
            }
        }
        else if (bAllBasesHaveVehicle)
        {
            UE_LOG(LogTemp, Display, TEXT("[EXPANSION DEBUG] %s — All bases ready but not enough money"), *UEnum::GetValueAsString(Faction));
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

    UVehicleDefinition* VehDef = VehicleDB->AvailableVehicles[0].Get();
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
#include "UStrategyCampaignSubsystem.h"
#include "UStrategySaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "UMissionManagerSubsystem.h"
#include "UFactionIntelSubsystem.h"
#include "URadarTerrainSubsystem.h"
#include "URadarContactSubsystem.h"
#include "UExplorationSubsystem.h"
#include "UBaseManagerSubsystem.h"
#include "UStrategyBase.h"
#include "UStrategyFacility.h"
#include "UStrategyEventDispatcher.h"
#include "StrategicSiteDefinition.h"

// Wires subsystem dependencies and binds OnDayPassed to campaign, mission, and AI subsystems.
void UStrategyCampaignSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    Collection.InitializeDependency<UTimeManagerSubsystem>();
    Collection.InitializeDependency<UResourceManagerSubsystem>();
    Collection.InitializeDependency<USoldierManagerSubsystem>();
    Collection.InitializeDependency<UEngineeringManagerSubsystem>();
    Collection.InitializeDependency<UBaseManagerSubsystem>();
    Collection.InitializeDependency<UResearchManagerSubsystem>();
    Collection.InitializeDependency<UAIControllerSubsystem>();
    Collection.InitializeDependency<UMissionManagerSubsystem>();

    UE_LOG(LogTemp, Display, TEXT("✅ UStrategyCampaignSubsystem: All required subsystem dependencies declared"));

    if (UTimeManagerSubsystem* TimeMgr = GetTimeManager())
    {
        TimeMgr->OnDayPassed.AddDynamic(this, &UStrategyCampaignSubsystem::OnDayPassed);
        UE_LOG(LogTemp, Display, TEXT("Campaign — OnDayPassed bound to AI"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Campaign could NOT get TimeManager for AI binding!"));
    }

    if (UTimeManagerSubsystem* TimeMgr = GetTimeManager())
    {
        if (UMissionManagerSubsystem* MissionMgr = GetMissionManager())
        {
            TimeMgr->OnDayPassed.RemoveDynamic(MissionMgr, &UMissionManagerSubsystem::OnDayPassed);
            TimeMgr->OnDayPassed.AddDynamic(MissionMgr, &UMissionManagerSubsystem::OnDayPassed);
            UE_LOG(LogTemp, Display, TEXT("Campaign bound MissionManager to OnDayPassed"));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Campaign could NOT get MissionManager!"));
        }

        if (UAIControllerSubsystem* AI = GetAIController())
        {
            TimeMgr->OnDayPassed.RemoveDynamic(AI, &UAIControllerSubsystem::OnDayPassed);
            TimeMgr->OnDayPassed.AddDynamic(AI, &UAIControllerSubsystem::OnDayPassed);
            UE_LOG(LogTemp, Display, TEXT("Campaign bound AIController to OnDayPassed"));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Campaign could NOT get AIController!"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Campaign could NOT get TimeManager for MissionManager binding!"));
    }

    UE_LOG(LogTemp, Display, TEXT("UStrategyCampaignSubsystem initialized — All managers + AI forced active"));
}

// Runs daily repairs for both factions and logs facility capacity summary.
void UStrategyCampaignSubsystem::OnDayPassed(int32 NewDay)
{
    // === Real in-game date header ===
    UTimeManagerSubsystem* TimeMgr = GetTimeManager();
    FString DateHeader = TimeMgr ? TimeMgr->GetFormattedDateString() : FString::Printf(TEXT("DAY %d"), NewDay);

    // === BOLD DAY SEPARATOR ===
    UE_LOG(LogTemp, Display, TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("################################################################################"));
    UE_LOG(LogTemp, Display, TEXT("##############################   %s STARTED   ##############################"), *DateHeader);
    UE_LOG(LogTemp, Display, TEXT("################################################################################"));
    UE_LOG(LogTemp, Display, TEXT("[CAMPAIGN] %s passed — daily repairs and mission housekeeping"), *DateHeader);

    // === DAILY SIMULATION (repairs + healing) ===
    if (UBaseManagerSubsystem* BaseMgr = GetBaseManager())
    {
        BaseMgr->SimulateDailyRepairs(EFactionType::Human);
        BaseMgr->SimulateDailyRepairs(EFactionType::Enemy);
    }

    // === Clean daily summary ===
    int32 TotalMedical = 0;
    int32 TotalRepair = 0;

    if (UBaseManagerSubsystem* BaseMgr = GetBaseManager())
    {
        for (UStrategyBase* Base : BaseMgr->GetBases(EFactionType::Human))
        {
            if (!Base) continue;
            for (UStrategyFacility* Fac : Base->Facilities)
            {
                if (Fac && Fac->bIsOperational && Fac->FacilityDefinition)
                {
                    if (Fac->FacilityDefinition->FacilityType == EFacilityType::Medical)
                        TotalMedical += Fac->FacilityDefinition->Capacity;
                    else if (Fac->FacilityDefinition->FacilityType == EFacilityType::VehicleRepair)
                        TotalRepair += Fac->FacilityDefinition->Capacity;
                }
            }
        }
        for (UStrategyBase* Base : BaseMgr->GetBases(EFactionType::Enemy))
        {
            if (!Base) continue;
            for (UStrategyFacility* Fac : Base->Facilities)
            {
                if (Fac && Fac->bIsOperational && Fac->FacilityDefinition)
                {
                    if (Fac->FacilityDefinition->FacilityType == EFacilityType::Medical)
                        TotalMedical += Fac->FacilityDefinition->Capacity;
                    else if (Fac->FacilityDefinition->FacilityType == EFacilityType::VehicleRepair)
                        TotalRepair += Fac->FacilityDefinition->Capacity;
                }
            }
        }
    }

    UE_LOG(LogTemp, Display, TEXT("[DAILY SIM] Both factions — Medical Bays can heal %d soldiers | Vehicle Repair Shops can repair %d vehicles (+25 HP)"),
        TotalMedical, TotalRepair);

    // AI daily orders are handled by UAIControllerSubsystem::OnDayPassed (single binding).
}

// Clears resources, bases, research, intel, radar contacts, and exploration for New Game.
void UStrategyCampaignSubsystem::ResetSimulation()
{
    UE_LOG(LogTemp, Display, TEXT("[RESET] Resetting entire simulation..."));

    if (auto* ResourceMgr = GetResourceManager())
    {
        ResourceMgr->ResetResources(EFactionType::Human);
        ResourceMgr->ResetResources(EFactionType::Enemy);
    }

    if (auto* SoldierMgr = GetSoldierManager())
    {
        // TODO: Add ClearAllSoldiers() to SoldierManager later if needed
    }

    if (auto* BaseMgr = GetBaseManager())
    {
        BaseMgr->ResetAllBases();
    }

    if (auto* ResearchMgr = GetResearchManager())
    {
        ResearchMgr->ResetResearch();
    }

    if (auto* EngineeringMgr = GetEngineeringManager())
    {
        EngineeringMgr->ResetProduction();
    }

    if (UFactionIntelSubsystem* IntelMgr = GetFactionIntelManager())
    {
        IntelMgr->ClearAllIntel();
    }

    if (URadarContactSubsystem* ContactMgr = GetRadarContactManager())
    {
        ContactMgr->ClearAllContacts();
    }

    if (UExplorationSubsystem* Exploration = GetGameInstance()->GetSubsystem<UExplorationSubsystem>())
    {
        Exploration->ClearAllExplorationState();
    }

    UE_LOG(LogTemp, Display, TEXT("[RESET] Simulation has been fully cleared."));
}

// Generates strategic sites, places starting bases, triggers day-1 tick, and dumps database debug info.
void UStrategyCampaignSubsystem::StartSimulation()
{
    GetTimeManager()->SetTimeScale(1.0f);
    UE_LOG(LogTemp, Display, TEXT("SIMULATION STARTED")); 

    // === SET AI EXPANSION LIMIT AS A GAME SETTING ===
    if (UAIControllerSubsystem* AIController = GetAIController())
    {
        AIController->MaxBases = MaxAIBases;
        UE_LOG(LogTemp, Display, TEXT("[CAMPAIGN] AI MaxBases set to %d (campaign setting)"), AIController->MaxBases);
    }

    // === Generate sites and place initial Command Centers using campaign / initializer settings ===
    if (UBaseManagerSubsystem* BaseMgr = GetBaseManager())
    {
        BaseMgr->GenerateInitialSites(
            NumberOfStrategicSites,
            MinimumDistanceBetweenSites,
            LogicalMapWidth,
            LogicalMapHeight,
            MapBorderPadding);

        BaseMgr->InitializeStartingBases(FMath::RoundToInt(MinDistanceBetweenFactions));

        UE_LOG(LogTemp, Display, TEXT("[MAP] StartSimulation generated %d sites on %.0fx%.0f map (max %d bases per faction)"),
            BaseMgr->AllPotentialSites.Num(), LogicalMapWidth, LogicalMapHeight, MaxAIBases);
    }

    if (UTimeManagerSubsystem* TimeMgr = GetTimeManager())
    {
        if (UAIControllerSubsystem* AI = GetAIController())
        {
            AI->ResetDailyProcessingState();
        }

        const int32 DayNumber = TimeMgr->GetSimulationDayNumber();
        UE_LOG(LogTemp, Display, TEXT("[CAMPAIGN] Bases ready — triggering Day %d daily tick"), DayNumber);
        TimeMgr->OnDayPassed.Broadcast(DayNumber);
    }

    UE_LOG(LogTemp, Display, TEXT("=== DATA ASSET INITIALIZATION DEBUG START ==="));

    if (UFacilityDatabase* FacilityDB = FacilityDatabaseAsset.Get())
    {
        UE_LOG(LogTemp, Display, TEXT("[FACILITY DATABASE] Loaded %d facilities:"), FacilityDB->AvailableFacilities.Num());
        for (const TSoftObjectPtr<UFacilityDefinition>& SoftDef : FacilityDB->AvailableFacilities)
        {
            // Inside the for loop for facilities
            if (UFacilityDefinition* Def = SoftDef.Get())
            {
                FString RepairInfo = (Def->RepairHealthPerDay > 0) ?
                    FString::Printf(TEXT(" | Repair: +%d HP/day"), Def->RepairHealthPerDay) : TEXT("");

                // === NEW: Production & Extraction ===
                FString Production = FString::Printf(TEXT("Prod: M:%d Mt:%d Bio:%d Chem:%d Exo:%d"),
                    Def->ProductionPerDay.Money,
                    Def->ProductionPerDay.Metals,
                    Def->ProductionPerDay.Biologicals,
                    Def->ProductionPerDay.Chemicals,
                    Def->ProductionPerDay.ExoticMaterial);

                FString Extraction = FString::Printf(TEXT("Extract: M:%d Mt:%d Bio:%d Chem:%d Exo:%d"),
                    Def->ExtractionPerDay.Money,
                    Def->ExtractionPerDay.Metals,
                    Def->ExtractionPerDay.Biologicals,
                    Def->ExtractionPerDay.Chemicals,
                    Def->ExtractionPerDay.ExoticMaterial);

                UE_LOG(LogTemp, Display, TEXT("  • %s | Type: %s | Capacity: %d | Production Slots: %d | Speed: %.1f | MaxBuilt: %d | Power: +%d / -%d | Build Cost: %d Money, %d Metal, %d Biologicals, %d Chemicals | Build Time: %d days (Prerequisites: %s)%s | %s | %s"),
                    *Def->FacilityName.ToString(),
                    *UEnum::GetValueAsString(Def->FacilityType),
                    Def->Capacity,
                    Def->ProductionSlots,
                    Def->ProductionSpeedMultiplier,
                    Def->MaxBuilt,
                    Def->PowerProvided,
                    Def->PowerDraw,
                    Def->BuildCost.Money,
                    Def->BuildCost.Metals,
                    Def->BuildCost.Biologicals,
                    Def->BuildCost.Chemicals,
                    Def->BuildTimeDays,
                    *FString::JoinBy(Def->PrerequisiteFacilities, TEXT(", "), [](EFacilityType T) { return UEnum::GetValueAsString(T); }),
                    *RepairInfo,
                    *Production,
                    *Extraction);
            }
        }
    }

    if (USoldierClassDatabase* SoldierDB = SoldierClassDatabaseAsset.Get())
    {
        UE_LOG(LogTemp, Display, TEXT("[SOLDIER DATABASE] Loaded %d soldier classes:"), SoldierDB->AvailableSoldierClasses.Num());
        for (const TSoftObjectPtr<USoldierClassDefinition>& SoftClass : SoldierDB->AvailableSoldierClasses)
        {
            if (USoldierClassDefinition* ClassDef = SoftClass.Get())
            {
                const FResourceStockpile& Cost = ClassDef->TrainingCost;

                UE_LOG(LogTemp, Display, TEXT("  • %s | Starting XP: %d | Training Cost: 💰%d 🛠️%d 🧬%d ⚗️%d 🌌%d 📚%d | Training Days: %d"),
                    *ClassDef->ClassName.ToString(),
                    ClassDef->StartingXP,
                    Cost.Money, Cost.Metals, Cost.Biologicals, Cost.Chemicals,
                    Cost.ExoticMaterial, Cost.ResearchPoints,
                    ClassDef->TrainingDays);
            }
        }
    }

    if (UResearchDatabase* ResearchDB = ResearchDatabaseAsset.Get())
    {
        UE_LOG(LogTemp, Display, TEXT("[RESEARCH DATABASE] Loaded %d research techs:"), ResearchDB->AvailableTechs.Num());
        for (const TSoftObjectPtr<UResearchTechDefinition>& SoftTech : ResearchDB->AvailableTechs)
        {
            if (UResearchTechDefinition* Research = SoftTech.Get())
            {
                FString UnlockedList = " (Unlocks: ";
                bool First = true;

                for (const TSoftObjectPtr<UStrategyTechDefinition>& SoftStrategyTech : Research->UnlocksTech)
                {
                    if (UStrategyTechDefinition* TechDef = SoftStrategyTech.Get())
                    {
                        for (const TSoftObjectPtr<UItemDefinition>& SoftItem : TechDef->UnlocksItems)
                        {
                            if (UItemDefinition* Item = SoftItem.Get())
                            {
                                if (!First) UnlockedList += ", ";
                                UnlockedList += Item->ItemName.ToString();
                                First = false;
                            }
                        }
                    }
                }

                if (First) UnlockedList += "None";
                UnlockedList += ")";

                UE_LOG(LogTemp, Display, TEXT("  • %s | Research Days: %d%s"),
                    *Research->ProjectName.ToString(), Research->ResearchDays, *UnlockedList);
            }
        }
    }

    if (UItemDatabase* ItemDB = ItemDatabaseAsset.Get())
    {
        UE_LOG(LogTemp, Display, TEXT("[ITEM DATABASE] Loaded %d buyable items:"), ItemDB->BuyableItems.Num());
        for (const TSoftObjectPtr<UItemDefinition>& SoftItem : ItemDB->BuyableItems)
        {
            if (UItemDefinition* Item = SoftItem.Get())
            {
                UE_LOG(LogTemp, Display, TEXT("  • %s | Cost: %d Money, %d Metal, %d Biologicals, %d Chemicals | Production Days: %d"),
                    *Item->ItemName.ToString(),
                    Item->PurchaseCost.Money,
                    Item->PurchaseCost.Metals,
                    Item->PurchaseCost.Biologicals,
                    Item->PurchaseCost.Chemicals,
                    Item->ProductionDays);
            }
        }
    }

    if (UVehicleDatabase* VehicleDB = VehicleDatabaseAsset.Get())
    {
        UE_LOG(LogTemp, Display, TEXT("[VEHICLE DATABASE] Loaded %d vehicles:"), VehicleDB->AvailableVehicles.Num());
        for (const TSoftObjectPtr<UVehicleDefinition>& SoftVeh : VehicleDB->AvailableVehicles)
        {
            if (UVehicleDefinition* Veh = SoftVeh.Get())
            {
                UE_LOG(LogTemp, Display, TEXT("  • %s | Type: %s | Capacity: %d soldiers | Max Range: %.0f | Attack: %d | Build Cost: %d Money, %d Metal, %d Biologicals, %d Chemicals | Production: %d days"),
                    *Veh->VehicleName.ToString(),
                    *UEnum::GetValueAsString(Veh->VehicleType),
                    Veh->SoldierCapacity,
                    Veh->MaxRange,
                    Veh->AttackPower,
                    Veh->BuildCost.Money,
					Veh->BuildCost.Metals,
					Veh->BuildCost.Biologicals,
					Veh->BuildCost.Chemicals,
                    Veh->ProductionDays);
            }
        }
    }

    if (UResourceManagerSubsystem* ResourceMgr = GetResourceManager())
    {
        FResourceStockpile HumanRes = ResourceMgr->GetResources(EFactionType::Human);
        FResourceStockpile EnemyRes = ResourceMgr->GetResources(EFactionType::Enemy);
        UE_LOG(LogTemp, Display, TEXT("[RESOURCES] Human start: %d💰 %d🛠️ %d🧬 %d⚗️"), HumanRes.Money, HumanRes.Metals, HumanRes.Biologicals, HumanRes.Chemicals);
        UE_LOG(LogTemp, Display, TEXT("[RESOURCES] Enemy start: %d💰 %d🛠️ %d🧬 %d⚗️"), EnemyRes.Money, EnemyRes.Metals, EnemyRes.Biologicals, EnemyRes.Chemicals);
    }

    UE_LOG(LogTemp, Display, TEXT("=== DATA ASSET INITIALIZATION DEBUG COMPLETE ==="));
}

// Sets time scale to zero via the time manager.
void UStrategyCampaignSubsystem::StopSimulation()
{
    GetTimeManager()->SetTimeScale(0.0f);
    UE_LOG(LogTemp, Display, TEXT("SIMULATION STOPPED"));
}

// Returns a simple "Day N" label from the time manager calendar day.
FString UStrategyCampaignSubsystem::GetFormattedDate() const
{
    int32 Day = GetTimeManager()->GetCurrentDay();
    return FString::Printf(TEXT("Day %d"), Day);
}

// Forwards to AI subsystem debug planner.
void UStrategyCampaignSubsystem::Debug_RunAI()
{
    if (UAIControllerSubsystem* AI = GetAIController())
    {
        AI->Debug_RunAI();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Debug_RunAI: AI subsystem not found"));
    }
}

// Placeholder research completion check (always true when Tech is valid).
bool UStrategyCampaignSubsystem::HasCompletedResearch(EFactionType Faction, UResearchTechDefinition* Tech) const
{
    if (!Tech) return false;
    return true; // placeholder
}

// Walks facility research chains to determine whether an item is unlocked for a faction.
bool UStrategyCampaignSubsystem::IsItemUnlocked(EFactionType Faction, UItemDefinition* ItemDef) const
{
    if (!ItemDef) return false;

    UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    if (!BaseMgr) return false;

    const TArray<UStrategyFacility*>& Facilities = BaseMgr->GetFacilities(Faction);

    for (UStrategyFacility* Fac : Facilities)
    {
        if (Fac && Fac->bIsOperational && Fac->FacilityDefinition)
        {
            for (const auto& ResearchSoft : Fac->FacilityDefinition->UnlocksResearch)
            {
                UResearchTechDefinition* Research = ResearchSoft.Get();
                if (!Research) continue;

                if (HasCompletedResearch(Faction, Research))
                {
                    for (const auto& TechSoft : Research->UnlocksTech)
                    {
                        UStrategyTechDefinition* Tech = TechSoft.Get();
                        if (!Tech) continue;

                        for (const auto& UnlockedItem : Tech->UnlocksItems)
                        {
                            if (UnlockedItem.Get() == ItemDef)
                            {
                                if (bShowUnlockMessages)
                                {
                                    FString Key = ItemDef->ItemName.ToString();

                                    if (!const_cast<UStrategyCampaignSubsystem*>(this)->AnnouncedUnlocks.Contains(Key))
                                    {
                                        const_cast<UStrategyCampaignSubsystem*>(this)->AnnouncedUnlocks.Add(Key);
                                        UE_LOG(LogTemp, Display, TEXT("[UNLOCK] ✅ %s unlocked via Tech %s"),
                                            *ItemDef->ItemName.ToString(), *Tech->TechName.ToString());
                                    }
                                }
                                return true;
                            }
                        }
                    }
                }
            }
        }
    }

    UE_LOG(LogTemp, Verbose, TEXT("[UNLOCK] ❌ %s is NOT unlocked yet"), *ItemDef->ItemName.ToString());
    return false;
}

// Returns the game-instance resource manager subsystem.
UResourceManagerSubsystem* UStrategyCampaignSubsystem::GetResourceManager() const { return GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>(); }
// Returns the game-instance soldier manager subsystem.
USoldierManagerSubsystem* UStrategyCampaignSubsystem::GetSoldierManager() const { return GetGameInstance()->GetSubsystem<USoldierManagerSubsystem>(); }
// Returns the game-instance research manager subsystem.
UResearchManagerSubsystem* UStrategyCampaignSubsystem::GetResearchManager() const { return GetGameInstance()->GetSubsystem<UResearchManagerSubsystem>(); }
// Returns the game-instance engineering manager subsystem.
UEngineeringManagerSubsystem* UStrategyCampaignSubsystem::GetEngineeringManager() const { return GetGameInstance()->GetSubsystem<UEngineeringManagerSubsystem>(); }
// Returns the game-instance base manager subsystem.
UBaseManagerSubsystem* UStrategyCampaignSubsystem::GetBaseManager() const { return GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>(); }
// Returns the game-instance time manager subsystem.
UTimeManagerSubsystem* UStrategyCampaignSubsystem::GetTimeManager() const { return GetGameInstance()->GetSubsystem<UTimeManagerSubsystem>(); }
// Returns the game-instance AI controller subsystem.
UAIControllerSubsystem* UStrategyCampaignSubsystem::GetAIController() const { return GetGameInstance()->GetSubsystem<UAIControllerSubsystem>(); }
// Returns the game-instance mission manager subsystem.
UMissionManagerSubsystem* UStrategyCampaignSubsystem::GetMissionManager() const { return GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>(); }

// Returns the faction intel subsystem when game instance is valid.
UFactionIntelSubsystem* UStrategyCampaignSubsystem::GetFactionIntelManager() const
{
    return GetGameInstance() ? GetGameInstance()->GetSubsystem<UFactionIntelSubsystem>() : nullptr;
}

// Returns the radar terrain subsystem when game instance is valid.
URadarTerrainSubsystem* UStrategyCampaignSubsystem::GetRadarTerrainManager() const
{
    return GetGameInstance() ? GetGameInstance()->GetSubsystem<URadarTerrainSubsystem>() : nullptr;
}

// Returns the radar contact subsystem when game instance is valid.
URadarContactSubsystem* UStrategyCampaignSubsystem::GetRadarContactManager() const
{
    return GetGameInstance() ? GetGameInstance()->GetSubsystem<URadarContactSubsystem>() : nullptr;
}

// Serializes campaign day, resources, sites, and intel into a numbered save slot.
void UStrategyCampaignSubsystem::SaveCampaign(int32 SlotIndex)
{
    if (SlotIndex < 1) SlotIndex = 1;
    FString SlotName = FString::Printf(TEXT("SaveSlot%02d"), SlotIndex);

    UStrategySaveGame* SaveGame = Cast<UStrategySaveGame>(UGameplayStatics::CreateSaveGameObject(UStrategySaveGame::StaticClass()));
    if (!SaveGame) return;

    SaveGame->CurrentDay = GetTimeManager()->GetCurrentDay();
    SaveGame->HumanResources = GetResourceManager()->GetResources(EFactionType::Human);
    SaveGame->EnemyResources = GetResourceManager()->GetResources(EFactionType::Enemy);
    SaveGame->LastSavedTime = FDateTime::Now();
    SaveGame->HumanSoldierCount = GetSoldierManager()->GetRoster(EFactionType::Human).Num();
    SaveGame->HumanSummary = FText::FromString(FString::Printf(TEXT("%d Soldiers"), SaveGame->HumanSoldierCount));

    if (bSitesPersistenceEnabled)
    {
        if (UBaseManagerSubsystem* BaseMgr = GetBaseManager())
        {
            SaveGame->SavedSites = BaseMgr->SerializeAllSites();
        }
        if (UFactionIntelSubsystem* IntelMgr = GetFactionIntelManager())
        {
            SaveGame->SavedIntelHuman = IntelMgr->SerializeIntel(EFactionType::Human);
            SaveGame->SavedIntelEnemy = IntelMgr->SerializeIntel(EFactionType::Enemy);
        }
        SaveGame->SaveSchemaVersion = StrategyIntelSaveSchemaVersion;
        SaveGame->bIsContinuedCampaign = true;
    }
    else
    {
        SaveGame->SavedSites.Empty();
        SaveGame->SaveSchemaVersion = 0;
        SaveGame->bIsContinuedCampaign = false;
    }

    UGameplayStatics::SaveGameToSlot(SaveGame, SlotName, 0);

    if (bSitesPersistenceEnabled)
    {
        UE_LOG(LogTemp, Display, TEXT("[SAVE] Campaign saved to slot %d (Day %d, %d sites, schema %d)"),
            SlotIndex, SaveGame->CurrentDay, SaveGame->SavedSites.Num(), SaveGame->SaveSchemaVersion);
    }
    else
    {
        UE_LOG(LogTemp, Display, TEXT("[SAVE] Campaign saved to slot %d (Day %d, sites skipped — bSitesPersistenceEnabled=false)"),
            SlotIndex, SaveGame->CurrentDay);
    }
}

// Deserializes sites, intel, calendar, and resources from a numbered save slot.
void UStrategyCampaignSubsystem::LoadCampaign(int32 SlotIndex)
{
    if (SlotIndex < 1) SlotIndex = 1;
    FString SlotName = FString::Printf(TEXT("SaveSlot%02d"), SlotIndex);

    UStrategySaveGame* Loaded = Cast<UStrategySaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
    if (!Loaded)
    {
        UE_LOG(LogTemp, Error, TEXT("[SAVE] No save found in slot %d — load aborted"), SlotIndex);
        return;
    }

    if (Loaded->SaveSchemaVersion < StrategySiteMapSaveSchemaVersion)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[SAVE] Save slot %d has schema version %d (requires >= %d) — load aborted. Re-save after PR-4 or use StartSimulation for a new game."),
            SlotIndex, Loaded->SaveSchemaVersion, StrategySiteMapSaveSchemaVersion);
        return;
    }

    UBaseManagerSubsystem* BaseMgr = GetBaseManager();
    UMissionManagerSubsystem* MissionMgr = GetMissionManager();
    if (!BaseMgr || !MissionMgr)
    {
        UE_LOG(LogTemp, Error, TEXT("[SAVE] Required subsystems missing — load aborted"));
        return;
    }

    BaseMgr->AllPotentialSites.Empty();
    BaseMgr->DiscoveredSitesHuman.Empty();
    BaseMgr->DiscoveredSitesEnemy.Empty();

    MissionMgr->ClearRuntimeMissionStateForSiteMapLoad();

    const int32 ExistingBases = BaseMgr->GetBases(EFactionType::Human).Num() + BaseMgr->GetBases(EFactionType::Enemy).Num();
    if (ExistingBases > 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[SAVE] WARNING: bases exist before site load — clearing bases only"));
        BaseMgr->ResetAllBases();
    }

    BaseMgr->DeserializeAllSites(Loaded->SavedSites);

    if (UFactionIntelSubsystem* IntelMgr = GetFactionIntelManager())
    {
        IntelMgr->ClearAllIntel();
        if (Loaded->SaveSchemaVersion >= StrategyIntelSaveSchemaVersion)
        {
            IntelMgr->DeserializeIntel(EFactionType::Human, Loaded->SavedIntelHuman, BaseMgr);
            IntelMgr->DeserializeIntel(EFactionType::Enemy, Loaded->SavedIntelEnemy, BaseMgr);
            UE_LOG(LogTemp, Display, TEXT("[INTEL] Restored intel snapshots — Human:%d Enemy:%d"),
                Loaded->SavedIntelHuman.Num(), Loaded->SavedIntelEnemy.Num());
        }
        else
        {
            IntelMgr->SeedIntelFromDiscoveredSites(BaseMgr);
        }
    }

    GetTimeManager()->AdvanceDays(Loaded->CurrentDay - GetTimeManager()->GetCurrentDay());
    GetResourceManager()->SetResources(EFactionType::Human, Loaded->HumanResources);
    GetResourceManager()->SetResources(EFactionType::Enemy, Loaded->EnemyResources);

    bSitesPersistenceEnabled = true;

    const int32 SiteCount = BaseMgr->AllPotentialSites.Num();
    const int32 HumanBaseCount = BaseMgr->GetBases(EFactionType::Human).Num();
    const int32 EnemyBaseCount = BaseMgr->GetBases(EFactionType::Enemy).Num();

    if (HumanBaseCount == 0 && EnemyBaseCount == 0)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[SAVE] Site map loaded (%d sites, 0 missions). Simulation NOT runnable — no bases. Use StartSimulation for playable sessions."),
            SiteCount);
    }
    else
    {
        UE_LOG(LogTemp, Display, TEXT("[SAVE] Site map loaded (%d sites, 0 missions)."), SiteCount);
    }
}

// Delegates strategic pause to the time manager during contested salvage.
void UStrategyCampaignSubsystem::PauseStrategicClock()
{
    if (UTimeManagerSubsystem* TimeMgr = GetTimeManager())
    {
        TimeMgr->SetStrategicClockPaused(true);
    }
}

// Clears strategic pause on the time manager after contest resolution.
void UStrategyCampaignSubsystem::ResumeStrategicClock()
{
    if (UTimeManagerSubsystem* TimeMgr = GetTimeManager())
    {
        TimeMgr->SetStrategicClockPaused(false);
    }
}

// Stores contested wreck site, missions, and force snapshots for salvage contest UI.
void UStrategyCampaignSubsystem::ActivateSalvageContest(UStrategySiteDefinition* Site, UMissionGroup* HumanMission,
    UMissionGroup* EnemyMission, const FSalvageContestForceSnapshot& HumanSnapshot,
    const FSalvageContestForceSnapshot& EnemySnapshot)
{
    bSalvageContestActive = true;
    ContestedSalvageSite = Site;
    ContestedHumanSalvageMission = HumanMission;
    ContestedEnemySalvageMission = EnemyMission;
    ContestedHumanSnapshot = HumanSnapshot;
    ContestedEnemySnapshot = EnemySnapshot;
}

// Resets all transient salvage contest fields without aborting missions.
void UStrategyCampaignSubsystem::ClearSalvageContestState()
{
    bSalvageContestActive = false;
    ContestedSalvageSite = nullptr;
    ContestedHumanSalvageMission = nullptr;
    ContestedEnemySalvageMission = nullptr;
    ContestedHumanSnapshot = FSalvageContestForceSnapshot();
    ContestedEnemySnapshot = FSalvageContestForceSnapshot();
}

// Applies contest outcome, aborts affected salvage missions, clears state, and resumes the clock.
void UStrategyCampaignSubsystem::ResolveSalvageContest(ESalvageContestOutcome Outcome)
{
    if (!bSalvageContestActive)
    {
        UE_LOG(LogTemp, Warning, TEXT("[SALVAGE CONTEST] ResolveSalvageContest called with no active contest"));
        return;
    }

    UMissionManagerSubsystem* MissionMgr = GetMissionManager();
    if (!MissionMgr)
    {
        UE_LOG(LogTemp, Error, TEXT("[SALVAGE CONTEST] Mission manager missing — cannot resolve contest"));
        return;
    }

    const FString SiteName = ContestedSalvageSite ? ContestedSalvageSite->SiteName : TEXT("Unknown");

    switch (Outcome)
    {
    case ESalvageContestOutcome::FactionAWins:
        if (ContestedEnemySalvageMission)
        {
            MissionMgr->AbortSalvageMission(ContestedEnemySalvageMission, true);
        }
        UE_LOG(LogTemp, Display, TEXT("[SALVAGE CONTEST] Human wins at '%s' — Enemy salvage aborted"), *SiteName);
        break;
    case ESalvageContestOutcome::FactionBWins:
        if (ContestedHumanSalvageMission)
        {
            MissionMgr->AbortSalvageMission(ContestedHumanSalvageMission, true);
        }
        UE_LOG(LogTemp, Display, TEXT("[SALVAGE CONTEST] Enemy wins at '%s' — Human salvage aborted"), *SiteName);
        break;
    case ESalvageContestOutcome::FactionAAborts:
        if (ContestedHumanSalvageMission)
        {
            MissionMgr->AbortSalvageMission(ContestedHumanSalvageMission, true);
        }
        UE_LOG(LogTemp, Display, TEXT("[SALVAGE CONTEST] Human withdrew from '%s' — wreck unchanged"), *SiteName);
        break;
    case ESalvageContestOutcome::FactionBAborts:
        if (ContestedEnemySalvageMission)
        {
            MissionMgr->AbortSalvageMission(ContestedEnemySalvageMission, true);
        }
        UE_LOG(LogTemp, Display, TEXT("[SALVAGE CONTEST] Enemy withdrew from '%s' — wreck unchanged"), *SiteName);
        break;
    case ESalvageContestOutcome::MutualRetreat:
        if (ContestedHumanSalvageMission)
        {
            MissionMgr->AbortSalvageMission(ContestedHumanSalvageMission, true);
        }
        if (ContestedEnemySalvageMission)
        {
            MissionMgr->AbortSalvageMission(ContestedEnemySalvageMission, true);
        }
        UE_LOG(LogTemp, Display, TEXT("[SALVAGE CONTEST] Mutual retreat at '%s' — wreck unchanged"), *SiteName);
        break;
    default:
        break;
    }

    ClearSalvageContestState();
    ResumeStrategicClock();
}

// Loads save metadata from slots 1–10 for the save selection UI.
TArray<UStrategySaveGame*> UStrategyCampaignSubsystem::GetAllSaveMetadata() const
{
    TArray<UStrategySaveGame*> Saves;
    for (int32 i = 1; i <= 10; ++i)
    {
        FString SlotName = FString::Printf(TEXT("SaveSlot%02d"), i);
        UStrategySaveGame* Save = Cast<UStrategySaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
        if (Save)
            Saves.Add(Save);
    }
    return Saves;
}

// Updates victor-side POW capture and KIA chances with clamped debug values.
void UStrategyCampaignSubsystem::SetVictoryChances(float NewPOWCaptureChance, float NewKIAChanceOnVictory)
{
    POWCaptureChanceOnVictory = FMath::Clamp(NewPOWCaptureChance, 0.0f, 1.0f);
    KIAChanceOnVictory = FMath::Clamp(NewKIAChanceOnVictory, 0.0f, 1.0f);

    UE_LOG(LogTemp, Display, TEXT("[POW/KIA] Victory chances updated → POW Capture: %.0f%% | KIA on victory: %.0f%%"),
        POWCaptureChanceOnVictory * 100.0f, KIAChanceOnVictory * 100.0f);
}

// Updates defender KIA chance on defeat with a clamped debug value.
void UStrategyCampaignSubsystem::SetDefeatKIAChance(float NewEnemyKIAChanceOnDefeat)
{
    EnemyKIAChanceOnDefeat = FMath::Clamp(NewEnemyKIAChanceOnDefeat, 0.0f, 1.0f);
    UE_LOG(LogTemp, Display, TEXT("[KIA] Defeat KIA chance updated → %.0f%%"), EnemyKIAChanceOnDefeat * 100.0f);
}

// Orphan debug helper (not a UStrategyCampaignSubsystem member) — logs forced autopsy request.
UFUNCTION(BlueprintCallable, Category = "POW/KIA|Debug")
void ForceAutopsy(EFactionType Faction)
{
    // For instant testing
    UE_LOG(LogTemp, Display, TEXT("[KIA DEBUG] Forcing autopsy on %s KIA bodies"), *UEnum::GetValueAsString(Faction));
    // The daily tick will handle it next frame, or you can call ProcessAutopsyDaily directly if needed
}
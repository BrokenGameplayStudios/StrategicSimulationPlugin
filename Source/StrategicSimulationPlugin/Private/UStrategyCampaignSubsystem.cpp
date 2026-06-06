#include "UStrategyCampaignSubsystem.h"
#include "UStrategySaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "UMissionManagerSubsystem.h"
#include "UBaseManagerSubsystem.h"
#include "UStrategyBase.h"
#include "UStrategyFacility.h"

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
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Campaign could NOT get TimeManager for MissionManager binding!"));
    }

    UE_LOG(LogTemp, Display, TEXT("UStrategyCampaignSubsystem initialized — All managers + AI forced active"));
}

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
    UE_LOG(LogTemp, Display, TEXT("[CAMPAIGN] %s passed — calling AI automatically"), *DateHeader);

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

    // === AI CALLS ===
    UE_LOG(LogTemp, Display, TEXT("[CAMPAIGN] Checking AI simulation toggles..."));

    if (UAIControllerSubsystem* AI = GetAIController())
    {
        if (AI->bSimulateHumanAI)
        {
            UE_LOG(LogTemp, Display, TEXT("[CAMPAIGN] → Human AI enabled — calling RunAIForFaction"));
            AI->RunAIForFaction(EFactionType::Human, NewDay);
        }

        if (AI->bSimulateEnemyAI)
        {
            UE_LOG(LogTemp, Display, TEXT("[CAMPAIGN] → Enemy AI enabled — calling RunAIForFaction"));
            AI->RunAIForFaction(EFactionType::Enemy, NewDay);
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[CAMPAIGN] Could not find AIControllerSubsystem!"));
    }
}

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

    UE_LOG(LogTemp, Display, TEXT("[RESET] Simulation has been fully cleared."));
}

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

    UE_LOG(LogTemp, Display, TEXT("=== DATA ASSET INITIALIZATION DEBUG START ==="));

    if (UFacilityDatabase* FacilityDB = FacilityDatabaseAsset.Get())
    {
        UE_LOG(LogTemp, Display, TEXT("[FACILITY DATABASE] Loaded %d facilities:"), FacilityDB->AvailableFacilities.Num());
        for (const TSoftObjectPtr<UFacilityDefinition>& SoftDef : FacilityDB->AvailableFacilities)
        {
            if (UFacilityDefinition* Def = SoftDef.Get())
            {
                FString RepairInfo = (Def->RepairHealthPerDay > 0) ?
                    FString::Printf(TEXT(" | Repair: +%d HP/day"), Def->RepairHealthPerDay) : TEXT("");

                FString UnlocksList = " (Unlocks: ";
                bool First = true;

                for (const TSoftObjectPtr<UResearchTechDefinition>& SoftResearch : Def->UnlocksResearch)
                {
                    if (UResearchTechDefinition* Research = SoftResearch.Get())
                    {
                        if (!First) UnlocksList += ", ";
                        UnlocksList += Research->ProjectName.ToString();
                        First = false;
                    }
                }

                if (First) UnlocksList += "None";
                UnlocksList += ")";

                UE_LOG(LogTemp, Display, TEXT("  • %s | Type: %s | Capacity: %d | Production Slots: %d | Speed: %.1f | MaxBuilt: %d | Power: +%d / -%d | Build Cost: %d Money, %d Metal, %d Biologicals, %d Chemicals | Build Time: %d days (Prerequisites: %s)%s"),
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
                    *RepairInfo);
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
                UE_LOG(LogTemp, Display, TEXT("  • %s | Type: %s | Capacity: %d soldiers | Max Mission Days: %d | Attack: %d | Build Cost: %d Money, %d Metal, %d Biologicals, %d Chemicals | Production: %d days"),
                    *Veh->VehicleName.ToString(),
                    *UEnum::GetValueAsString(Veh->VehicleType),
                    Veh->SoldierCapacity,
                    Veh->MaxMissionDurationDays,
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

void UStrategyCampaignSubsystem::StopSimulation()
{
    GetTimeManager()->SetTimeScale(0.0f);
    UE_LOG(LogTemp, Display, TEXT("SIMULATION STOPPED"));
}

FString UStrategyCampaignSubsystem::GetFormattedDate() const
{
    int32 Day = GetTimeManager()->GetCurrentDay();
    return FString::Printf(TEXT("Day %d"), Day);
}

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

bool UStrategyCampaignSubsystem::HasCompletedResearch(EFactionType Faction, UResearchTechDefinition* Tech) const
{
    if (!Tech) return false;
    return true; // placeholder
}

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

UResourceManagerSubsystem* UStrategyCampaignSubsystem::GetResourceManager() const { return GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>(); }
USoldierManagerSubsystem* UStrategyCampaignSubsystem::GetSoldierManager() const { return GetGameInstance()->GetSubsystem<USoldierManagerSubsystem>(); }
UResearchManagerSubsystem* UStrategyCampaignSubsystem::GetResearchManager() const { return GetGameInstance()->GetSubsystem<UResearchManagerSubsystem>(); }
UEngineeringManagerSubsystem* UStrategyCampaignSubsystem::GetEngineeringManager() const { return GetGameInstance()->GetSubsystem<UEngineeringManagerSubsystem>(); }
UBaseManagerSubsystem* UStrategyCampaignSubsystem::GetBaseManager() const { return GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>(); }
UTimeManagerSubsystem* UStrategyCampaignSubsystem::GetTimeManager() const { return GetGameInstance()->GetSubsystem<UTimeManagerSubsystem>(); }
UAIControllerSubsystem* UStrategyCampaignSubsystem::GetAIController() const { return GetGameInstance()->GetSubsystem<UAIControllerSubsystem>(); }
UMissionManagerSubsystem* UStrategyCampaignSubsystem::GetMissionManager() const { return GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>(); }

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

    UGameplayStatics::SaveGameToSlot(SaveGame, SlotName, 0);
    UE_LOG(LogTemp, Display, TEXT("CAMPAIGN SAVED to slot %d (Day %d)"), SlotIndex, SaveGame->CurrentDay);
}

void UStrategyCampaignSubsystem::LoadCampaign(int32 SlotIndex)
{
    if (SlotIndex < 1) SlotIndex = 1;
    FString SlotName = FString::Printf(TEXT("SaveSlot%02d"), SlotIndex);

    UStrategySaveGame* Loaded = Cast<UStrategySaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
    if (!Loaded)
    {
        UE_LOG(LogTemp, Warning, TEXT("No save found in slot %d — starting fresh"), SlotIndex);
        return;
    }

    GetTimeManager()->AdvanceDays(Loaded->CurrentDay - GetTimeManager()->GetCurrentDay());
    GetResourceManager()->SetResources(EFactionType::Human, Loaded->HumanResources);
    GetResourceManager()->SetResources(EFactionType::Enemy, Loaded->EnemyResources);

    UE_LOG(LogTemp, Display, TEXT("CAMPAIGN LOADED from slot %d (Day %d)"), SlotIndex, Loaded->CurrentDay);
}

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

// === CLEAN VICTORY-SIDE DEBUG (POW + KIA on win) ===
UFUNCTION(BlueprintCallable, Category = "POW/KIA|Debug")
void SetVictoryChances(float NewPOWCaptureChance, float NewKIAChanceOnVictory)
{
    POWCaptureChanceOnVictory = FMath::Clamp(NewPOWCaptureChance, 0.0f, 1.0f);
    KIAChanceOnVictory = FMath::Clamp(NewKIAChanceOnVictory, 0.0f, 1.0f);

    UE_LOG(LogTemp, Display, TEXT("[POW/KIA] Victory chances updated → POW Capture: %.0f%% | KIA on victory: %.0f%%"),
        POWCaptureChanceOnVictory * 100.0f, KIAChanceOnVictory * 100.0f);
}

// === CLEAN DEFEAT-SIDE KIA DEBUG ===
UFUNCTION(BlueprintCallable, Category = "POW/KIA|Debug")
void SetDefeatKIAChance(float NewEnemyKIAChanceOnDefeat)
{
    EnemyKIAChanceOnDefeat = FMath::Clamp(NewEnemyKIAChanceOnDefeat, 0.0f, 1.0f);
    UE_LOG(LogTemp, Display, TEXT("[KIA] Defeat KIA chance updated → %.0f%%"), EnemyKIAChanceOnDefeat * 100.0f);
}

UFUNCTION(BlueprintCallable, Category = "POW/KIA|Debug")
void ForceAutopsy(EFactionType Faction)
{
    // For instant testing
    UE_LOG(LogTemp, Display, TEXT("[KIA DEBUG] Forcing autopsy on %s KIA bodies"), *UEnum::GetValueAsString(Faction));
    // The daily tick will handle it next frame, or you can call ProcessAutopsyDaily directly if needed
}
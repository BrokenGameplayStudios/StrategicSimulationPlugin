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
    UE_LOG(LogTemp, Display, TEXT("[CAMPAIGN] Day %d passed — calling AI automatically"), NewDay);

    // === CRITICAL FIX: Repairs run BEFORE AI so returned vehicles reclaim their reserved hanger slots first ===
    if (UBaseManagerSubsystem* BaseMgr = GetBaseManager())
    {
        BaseMgr->SimulateDailyRepairs(EFactionType::Human);
        BaseMgr->SimulateDailyRepairs(EFactionType::Enemy);
        UE_LOG(LogTemp, Display, TEXT("[CAMPAIGN] Daily repairs completed for both factions"));
    }

    if (UAIControllerSubsystem* AI = GetAIController())
    {
        AI->RunAIForFaction(EFactionType::Enemy, NewDay);
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

                UE_LOG(LogTemp, Display, TEXT("  • %s | Type: %s | Capacity: %d | MaxBuilt: %d | Power: +%d / -%d | Build Cost: %d Money, %d Supplies | Build Time: %d days%s"),
                    *Def->FacilityName.ToString(),
                    *UEnum::GetValueAsString(Def->FacilityType),
                    Def->Capacity,
                    Def->MaxBuilt,
                    Def->PowerProvided,
                    Def->PowerDraw,
                    Def->BuildCost.Money,
                    Def->BuildCost.Supplies,
                    Def->BuildTimeDays,
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
                UE_LOG(LogTemp, Display, TEXT("  • %s | Starting XP: %d"), *ClassDef->ClassName.ToString(), ClassDef->StartingXP);
            }
        }
    }

    if (UResearchDatabase* ResearchDB = ResearchDatabaseAsset.Get())
    {
        UE_LOG(LogTemp, Display, TEXT("[RESEARCH DATABASE] Loaded %d research techs:"), ResearchDB->AvailableTechs.Num());
        for (const TSoftObjectPtr<UResearchTechDefinition>& SoftTech : ResearchDB->AvailableTechs)
        {
            if (UResearchTechDefinition* Tech = SoftTech.Get())
            {
                FString Unlocks = " (Unlocks: ";
                bool First = true;
                for (const TSoftObjectPtr<UItemDefinition>& ItemSoft : Tech->UnlocksItems)
                {
                    if (UItemDefinition* Item = ItemSoft.Get())
                    {
                        Unlocks += First ? "" : ", ";
                        Unlocks += Item->ItemName.ToString();
                        First = false;
                    }
                }
                Unlocks += ")";

                UE_LOG(LogTemp, Display, TEXT("  • %s | Research Days: %d%s"),
                    *Tech->ProjectName.ToString(), Tech->ResearchDays, *Unlocks);
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
                UE_LOG(LogTemp, Display, TEXT("  • %s | Cost: %d Money, %d Supplies | Production Days: %d"),
                    *Item->ItemName.ToString(),
                    Item->PurchaseCost.Money,
                    Item->PurchaseCost.Supplies,
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
                UE_LOG(LogTemp, Display, TEXT("  • %s | Type: %s | Capacity: %d soldiers | Max Mission Days: %d | Attack: %d | Build Cost: %d Money, %d Supplies | Production: %d days"),
                    *Veh->VehicleName.ToString(),
                    *UEnum::GetValueAsString(Veh->VehicleType),
                    Veh->SoldierCapacity,
                    Veh->MaxMissionDurationDays,
                    Veh->AttackPower,
                    Veh->BuildCost.Money,
                    Veh->BuildCost.Supplies,
                    Veh->ProductionDays);
            }
        }
    }

    if (UResourceManagerSubsystem* ResourceMgr = GetResourceManager())
    {
        FResourceStockpile HumanRes = ResourceMgr->GetResources(EFactionType::Human);
        FResourceStockpile EnemyRes = ResourceMgr->GetResources(EFactionType::Enemy);
        UE_LOG(LogTemp, Display, TEXT("[RESOURCES] Human start: %d💰 %d📦 %d🛠️ %d🧬 %d⚗️"), HumanRes.Money, HumanRes.Supplies, HumanRes.Metals, HumanRes.Biologicals, HumanRes.Chemicals);
        UE_LOG(LogTemp, Display, TEXT("[RESOURCES] Enemy start: %d💰 %d📦 %d🛠️ %d🧬 %d⚗️"), EnemyRes.Money, EnemyRes.Supplies, EnemyRes.Metals, EnemyRes.Biologicals, EnemyRes.Chemicals);
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
                                UE_LOG(LogTemp, Display, TEXT("[UNLOCK] ✅ %s unlocked via Tech %s"),
                                    *ItemDef->ItemName.ToString(), *Tech->TechName.ToString());
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
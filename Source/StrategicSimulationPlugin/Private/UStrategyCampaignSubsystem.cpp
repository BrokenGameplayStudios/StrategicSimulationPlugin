#include "UStrategyCampaignSubsystem.h"
#include "UStrategySaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"

void UStrategyCampaignSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Force all subsystems
    GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>();
    GetGameInstance()->GetSubsystem<UTimeManagerSubsystem>();
    GetGameInstance()->GetSubsystem<USoldierManagerSubsystem>();
    GetGameInstance()->GetSubsystem<UEngineeringManagerSubsystem>();
    GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    GetGameInstance()->GetSubsystem<UResearchManagerSubsystem>();
    GetGameInstance()->GetSubsystem<UAIControllerSubsystem>();

    // Bind to every day that passes
    if (UTimeManagerSubsystem* TimeMgr = GetTimeManager())
    {
        TimeMgr->OnDayPassed.AddDynamic(this, &UStrategyCampaignSubsystem::OnDayPassed);
        UE_LOG(LogTemp, Display, TEXT("✅ Campaign — OnDayPassed bound to AI"));
    }

    UE_LOG(LogTemp, Display, TEXT("UStrategyCampaignSubsystem initialized — All managers + AI forced active"));
}

void UStrategyCampaignSubsystem::OnDayPassed(int32 NewDay)
{
    UE_LOG(LogTemp, Display, TEXT("🔥 [CAMPAIGN] Day %d passed — calling AI automatically"), NewDay);

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

    // === COMPREHENSIVE DATA ASSET DEBUG PRINT (for balancing) ===
    UE_LOG(LogTemp, Display, TEXT("=== DATA ASSET INITIALIZATION DEBUG START ==="));

    // 1. Facility Database
    if (UFacilityDatabase* FacilityDB = FacilityDatabaseAsset.Get())
    {
        UE_LOG(LogTemp, Display, TEXT("[FACILITY DATABASE] Loaded %d facilities:"), FacilityDB->AvailableFacilities.Num());
        for (const TSoftObjectPtr<UFacilityDefinition>& SoftDef : FacilityDB->AvailableFacilities)
        {
            if (UFacilityDefinition* Def = SoftDef.Get())
            {
                UE_LOG(LogTemp, Display, TEXT("  • %s | Type: %s | Capacity: %d | MaxBuilt: %d | Power: +%d / -%d | Build Cost: %d Money, %d Supplies | Build Time: %d days"),
                    *Def->FacilityName.ToString(),
                    *UEnum::GetValueAsString(Def->FacilityType),
                    Def->Capacity,
                    Def->MaxBuilt,
                    Def->PowerProvided,
                    Def->PowerDraw,
                    Def->BuildCost.Money,
                    Def->BuildCost.Supplies,
                    Def->BuildTimeDays);
            }
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[FACILITY DATABASE] FacilityDatabaseAsset is NULL!"));
    }

    // 2. Soldier Class Database
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
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[SOLDIER DATABASE] SoldierClassDatabaseAsset is NULL!"));
    }

    // 3. Research Database
    if (UResearchDatabase* ResearchDB = ResearchDatabaseAsset.Get())
    {
        UE_LOG(LogTemp, Display, TEXT("[RESEARCH DATABASE] Loaded %d research techs:"), ResearchDB->AvailableTechs.Num());
        for (const TSoftObjectPtr<UResearchTechDefinition>& SoftTech : ResearchDB->AvailableTechs)
        {
            if (UResearchTechDefinition* Tech = SoftTech.Get())
            {
                UE_LOG(LogTemp, Display, TEXT("  • %s | Research Days: %d"), *Tech->ProjectName.ToString(), Tech->ResearchDays);
            }
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[RESEARCH DATABASE] ResearchDatabaseAsset is NULL!"));
    }

    // 4. Item Database
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
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[ITEM DATABASE] ItemDatabaseAsset is NULL!"));
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
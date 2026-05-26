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

void UStrategyCampaignSubsystem::StartSimulation()
{
    GetTimeManager()->SetTimeScale(1.0f);
    UE_LOG(LogTemp, Display, TEXT("SIMULATION STARTED"));
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

    // TODO: Once we have a real research queue, check completed research here.
    // For now we assume that if the facility that unlocks the research is built, the tech is available.
    // This is the "Research unlocks Tech" step.
    return true; // placeholder for Phase 22 research automation
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
            // 1. Facility → UnlocksResearch
            for (const auto& ResearchSoft : Fac->FacilityDefinition->UnlocksResearch)
            {
                UResearchTechDefinition* Research = ResearchSoft.Get();
                if (!Research) continue;

                if (HasCompletedResearch(Faction, Research))
                {
                    // 2. Research → UnlocksTech
                    for (const auto& TechSoft : Research->UnlocksTech)
                    {
                        UStrategyTechDefinition* Tech = TechSoft.Get();
                        if (!Tech) continue;

                        // 3. Tech → UnlocksItems
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
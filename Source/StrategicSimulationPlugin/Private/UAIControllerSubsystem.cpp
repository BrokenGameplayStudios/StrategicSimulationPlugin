#include "UAIControllerSubsystem.h"
#include "UResourceManagerSubsystem.h"
#include "USoldierManagerSubsystem.h"
#include "UEngineeringManagerSubsystem.h"
#include "UItemDatabase.h"
#include "UStrategyCampaignSubsystem.h"
#include "USoldierClassDefinition.h"
#include "Engine/Engine.h"

void UAIControllerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    if (UTimeManagerSubsystem* TimeMgr = GetGameInstance()->GetSubsystem<UTimeManagerSubsystem>())
    {
        TimeMgr->OnDayPassed.AddDynamic(this, &UAIControllerSubsystem::OnDayPassed);
        UE_LOG(LogTemp, Display, TEXT("✅ UAIControllerSubsystem — OnDayPassed bound successfully"));
    }

    UE_LOG(LogTemp, Display, TEXT("✅ UAIControllerSubsystem initialized — Enemy AI is NOW ACTIVE"));
}

void UAIControllerSubsystem::OnDayPassed(int32 NewDay)
{
    UE_LOG(LogTemp, Display, TEXT("🔥 [AI] === ENEMY AI DECISION TRIGGERED — Real Day %d ==="), NewDay);
    RunAIForFaction(EFactionType::Enemy, NewDay);
}

void UAIControllerSubsystem::RunAIForFaction(EFactionType Faction, int32 CurrentDay)
{
    UE_LOG(LogTemp, Display, TEXT("[AI] %s — Day %d decision"), *UEnum::GetValueAsString(Faction), CurrentDay);

    // Priority 1: Build critical facilities (LivingQuarters first for recruitment)
    if (TryBuildFacility(Faction, EFacilityType::LivingQuarters))
        return;   // built something — end turn (remove 'return' if you want AI to do multiple actions per day)

    // TODO Phase 22: Workshop, Laboratory, research, etc.

    TryRecruit(Faction);
    TryBuyAndEquip(Faction);
}

bool UAIControllerSubsystem::TryRecruit(EFactionType Faction)
{
    USoldierManagerSubsystem* SoldierMgr = GetGameInstance()->GetSubsystem<USoldierManagerSubsystem>();
    UResourceManagerSubsystem* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>();

    if (!SoldierMgr || !ResourceMgr) return false;

    FResourceStockpile Res = ResourceMgr->GetResources(Faction);

    if (Res.Money >= 500)
    {
        // Update this path if your Rookie asset has a different name
        USoldierClassDefinition* RookieClass = LoadObject<USoldierClassDefinition>(nullptr, TEXT("/StrategicSimulationPlugin/Data/DA_Soldier_Rookie.DA_Soldier_Rookie_C"));

        if (RookieClass && SoldierMgr->RecruitSoldier(Faction, RookieClass))
        {
            UE_LOG(LogTemp, Display, TEXT("[AI] ✅ %s recruited a new soldier (cost 500 Money)"), *UEnum::GetValueAsString(Faction));
            return true;
        }
    }
    return false;
}

bool UAIControllerSubsystem::TryBuyAndEquip(EFactionType Faction)
{
    UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();
    UItemDatabase* ItemDB = Campaign ? Campaign->ItemDatabaseAsset.Get() : nullptr;

    UEngineeringManagerSubsystem* EngMgr = GetGameInstance()->GetSubsystem<UEngineeringManagerSubsystem>();
    USoldierManagerSubsystem* SoldierMgr = GetGameInstance()->GetSubsystem<USoldierManagerSubsystem>();
    UResourceManagerSubsystem* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>();

    if (!EngMgr || !SoldierMgr || !ResourceMgr || !ItemDB)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AI] %s — Missing managers or ItemDatabase!"), *UEnum::GetValueAsString(Faction));
        return false;
    }

    TArray<UStrategySoldier*> Roster = SoldierMgr->GetRoster(Faction);
    if (Roster.Num() == 0) return false;

    FResourceStockpile Res = ResourceMgr->GetResources(Faction);

    for (const TSoftObjectPtr<UItemDefinition>& SoftItem : ItemDB->BuyableItems)
    {
        if (UItemDefinition* Item = SoftItem.Get())
        {
            if (Res.Money >= Item->PurchaseCost.Money)
            {
                UStrategySoldier* Soldier = Roster[0];
                if (EngMgr->PurchaseItem(Faction, Item, Soldier))
                {
                    UE_LOG(LogTemp, Display, TEXT("[AI] ✅ SUCCESS — %s bought and equipped %s on %s"),
                        *UEnum::GetValueAsString(Faction), *Item->ItemName.ToString(), *Soldier->SoldierName);
                    return true;
                }
            }
        }
    }
    return false;
}

void UAIControllerSubsystem::Debug_RunAI()
{
    UE_LOG(LogTemp, Display, TEXT("[AI DEBUG] Manual AI run requested by player"));
    RunAIForFaction(EFactionType::Enemy, 999);
}

bool UAIControllerSubsystem::TryBuildFacility(EFactionType Faction, EFacilityType FacilityTypeToBuild)
{
    UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();
    UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    UResourceManagerSubsystem* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>();

    if (!Campaign || !BaseMgr || !ResourceMgr) return false;

    UFacilityDefinition* FacilityDef = nullptr;

    if (FacilityTypeToBuild == EFacilityType::LivingQuarters)
        FacilityDef = Campaign->BasicLivingQuartersAsset.Get();
    else if (FacilityTypeToBuild == EFacilityType::Workshop)
        FacilityDef = Campaign->BasicWorkshopAsset.Get();
    else if (FacilityTypeToBuild == EFacilityType::Laboratory)
        FacilityDef = Campaign->BasicLaboratoryAsset.Get();
    else if (FacilityTypeToBuild == EFacilityType::Special) // MedicalBay example
        FacilityDef = Campaign->BasicMedicalBayAsset.Get();
    else
        return false;

    if (!FacilityDef)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AI] Missing FacilityDefinition for %s — assign it in GameInitializer!"),
            *UEnum::GetValueAsString(FacilityTypeToBuild));
        return false;
    }

    // Simple guard: don't rebuild LivingQuarters if we already have capacity
    if (FacilityTypeToBuild == EFacilityType::LivingQuarters && BaseMgr->GetTotalBarracksCapacity(Faction) > 0)
        return false;

    // Resource check
    FResourceStockpile Res = ResourceMgr->GetResources(Faction);
    if (Res.Money < FacilityDef->BuildCost.Money) return false;

    if (BaseMgr->BuildFacility(Faction, FacilityDef))
    {
        UE_LOG(LogTemp, Display, TEXT("[AI] ✅ %s started construction of %s"),
            *UEnum::GetValueAsString(Faction), *FacilityDef->FacilityName.ToString());
        return true;
    }
    return false;
}
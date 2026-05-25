#include "UAIControllerSubsystem.h"
#include "UResourceManagerSubsystem.h"
#include "USoldierManagerSubsystem.h"
#include "UEngineeringManagerSubsystem.h"
#include "UItemDatabase.h"
#include "USoldierClassDatabase.h"
#include "UStrategyCampaignSubsystem.h"
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

    // NEW: Always advance ALL facility construction first (reliable)
    if (UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>())
    {
        BaseMgr->AdvanceFacilityConstruction(EFactionType::Enemy);
    }

    RunAIForFaction(EFactionType::Enemy, NewDay);
}

void UAIControllerSubsystem::RunAIForFaction(EFactionType Faction, int32 CurrentDay)
{
    UE_LOG(LogTemp, Display, TEXT("[AI] %s — Day %d decision"), *UEnum::GetValueAsString(Faction), CurrentDay);

    // QUICK FIX: Always advance facility construction (reliable fallback since OnDayPassed binding is flaky)
    if (UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>())
    {
        BaseMgr->AdvanceFacilityConstruction(Faction);
    }

    // Priority 1: Build critical facilities
    if (TryBuildFacility(Faction, EFacilityType::LivingQuarters))
        return;

    // TODO Phase 22 (after databases): Workshop, Laboratory, research, etc.

    TryRecruit(Faction);
    TryBuyAndEquip(Faction);
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

    FResourceStockpile Res = ResourceMgr->GetResources(Faction);

    // Find the soldier with the fewest items (spreads gear)
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
    if (!TargetSoldier) return false;

    UE_LOG(LogTemp, Display, TEXT("[PURCHASE] === %s attempting to buy item (spreading to soldier with %d items) ==="),
        *UEnum::GetValueAsString(Faction), TargetSoldier->CurrentLoadout.Num());

    // Try to buy the first affordable item from the database
    for (const TSoftObjectPtr<UItemDefinition>& SoftItem : ItemDB->BuyableItems)
    {
        UItemDefinition* ItemDef = SoftItem.Get();
        if (!ItemDef) continue;

        if (Res.Money >= ItemDef->PurchaseCost.Money)
        {
            if (EngineeringMgr->PurchaseItem(Faction, ItemDef, TargetSoldier))
            {
                UE_LOG(LogTemp, Display, TEXT("[AI] ✅ %s bought and equipped %s on soldier with %d items"),
                    *UEnum::GetValueAsString(Faction), *ItemDef->ItemName.ToString(), TargetSoldier->CurrentLoadout.Num());
                return true;
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

    // Find the first facility in the database that matches the requested type
    UFacilityDefinition* FacilityDef = nullptr;
    if (UFacilityDatabase* DB = Campaign->FacilityDatabaseAsset.Get())
    {
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
    }

    if (!FacilityDef)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AI] No facility of type %s in FacilityDatabase!"),
            *UEnum::GetValueAsString(FacilityTypeToBuild));
        return false;
    }

    if (BaseMgr->HasFacilityOfType(Faction, FacilityTypeToBuild))
        return false;

    FResourceStockpile Res = ResourceMgr->GetResources(Faction);
    if (Res.Money < FacilityDef->BuildCost.Money) return false;

    if (BaseMgr->BuildFacility(Faction, FacilityDef))
    {
        UE_LOG(LogTemp, Display, TEXT("[AI] ✅ %s started construction of %s (%d days)"),
            *UEnum::GetValueAsString(Faction), *FacilityDef->FacilityName.ToString(), FacilityDef->BuildTimeDays);
        return true;
    }
    return false;
}

bool UAIControllerSubsystem::TryRecruit(EFactionType Faction)
{
    USoldierManagerSubsystem* SoldierMgr = GetGameInstance()->GetSubsystem<USoldierManagerSubsystem>();
    UResourceManagerSubsystem* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>();
    UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();

    if (!SoldierMgr || !ResourceMgr || !Campaign) return false;

    USoldierClassDefinition* RookieClass = nullptr;
    if (USoldierClassDatabase* DB = Campaign->SoldierClassDatabaseAsset.Get())
    {
        if (DB->AvailableSoldierClasses.Num() > 0)
            RookieClass = DB->AvailableSoldierClasses[0].Get();
    }

    if (!RookieClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AI] No soldier class in SoldierClassDatabase!"));
        return false;
    }

    FResourceStockpile Res = ResourceMgr->GetResources(Faction);
    if (Res.Money >= 500 && SoldierMgr->RecruitSoldier(Faction, RookieClass))
    {
        UE_LOG(LogTemp, Display, TEXT("[AI] ✅ %s recruited a new soldier"), *UEnum::GetValueAsString(Faction));
        return true;
    }
    return false;
}
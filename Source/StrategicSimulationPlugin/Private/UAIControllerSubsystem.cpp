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
    UE_LOG(LogTemp, Display, TEXT("[AI] %s — Day %d decision (full build order)"), *UEnum::GetValueAsString(Faction), CurrentDay);

    // 1. Advance facility construction
    if (UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>())
    {
        BaseMgr->AdvanceFacilityConstruction(Faction);
    }

    // 2. Advance research progress every day
    if (UResearchManagerSubsystem* ResearchMgr = GetGameInstance()->GetSubsystem<UResearchManagerSubsystem>())
    {
        ResearchMgr->AdvanceDay(Faction);
    }

    // 3. Build facilities
    if (TryBuildFacility(Faction, EFacilityType::LivingQuarters)) return;
    if (TryBuildFacility(Faction, EFacilityType::Workshop)) return;
    if (TryBuildFacility(Faction, EFacilityType::Laboratory)) return;

    // 4. Research
    if (TryResearch(Faction)) return;

    // 5. Recruit
    TryRecruit(Faction);

    // 6. Buy unlocked gear
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

    UE_LOG(LogTemp, Display, TEXT("[PURCHASE] === %s starting buy round (spreading gear) ==="),
        *UEnum::GetValueAsString(Faction));

    bool bBoughtAnything = false;
    int32 PurchasesThisDay = 0;
    const int32 MaxPurchasesPerDay = 3;

    while (PurchasesThisDay < MaxPurchasesPerDay)
    {
        // Re-select the soldier with the fewest items every purchase
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
        if (!TargetSoldier) break;

        if (MinItems >= 4) break; // stop over-equipping one soldier

        FResourceStockpile Res = ResourceMgr->GetResources(Faction);

        bool bPurchasedThisLoop = false;

        for (const TSoftObjectPtr<UItemDefinition>& SoftItem : ItemDB->BuyableItems)
        {
            UItemDefinition* ItemDef = SoftItem.Get();
            if (!ItemDef) continue;

            // FULL UNLOCK CHAIN CHECK
            if (!Campaign->IsItemUnlocked(Faction, ItemDef))
                continue;

            // NEW: Prevent buying duplicates on the same soldier
            if (TargetSoldier->CurrentLoadout.Contains(ItemDef))
                continue;

            if (Res.Money >= ItemDef->PurchaseCost.Money)
            {
                if (EngineeringMgr->PurchaseItem(Faction, ItemDef, TargetSoldier))
                {
                    UE_LOG(LogTemp, Display, TEXT("[AI] ✅ Bought %s on soldier (now has %d items)"),
                        *ItemDef->ItemName.ToString(), TargetSoldier->CurrentLoadout.Num());
                    bPurchasedThisLoop = true;
                    bBoughtAnything = true;
                    PurchasesThisDay++;
                    break;
                }
            }
        }

        if (!bPurchasedThisLoop) break;
    }

    return bBoughtAnything;
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

bool UAIControllerSubsystem::TryResearch(EFactionType Faction)
{
    UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();
    UResearchManagerSubsystem* ResearchMgr = GetGameInstance()->GetSubsystem<UResearchManagerSubsystem>();
    UResourceManagerSubsystem* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>();

    if (!Campaign || !ResearchMgr || !ResourceMgr) return false;

    UResearchDatabase* ResearchDB = Campaign->ResearchDatabaseAsset.Get();
    if (!ResearchDB || ResearchDB->AvailableTechs.Num() == 0) return false;

    for (const TSoftObjectPtr<UResearchTechDefinition>& SoftResearch : ResearchDB->AvailableTechs)
    {
        UResearchTechDefinition* ResearchDef = SoftResearch.Get();
        if (!ResearchDef) continue;

        // Skip if already in progress or completed
        if (ResearchMgr->IsResearchInProgress(Faction, ResearchDef) || ResearchMgr->HasCompletedResearch(Faction, ResearchDef))
            continue;

        // Check cost
        FResourceStockpile Res = ResourceMgr->GetResources(Faction);
        if (Res.Money >= ResearchDef->ResearchCost.Money)
        {
            if (ResearchMgr->StartResearch(Faction, ResearchDef))
            {
                UE_LOG(LogTemp, Display, TEXT("[AI] ✅ %s started research: %s (%d days)"),
                    *UEnum::GetValueAsString(Faction), *ResearchDef->ProjectName.ToString(), ResearchDef->ResearchDays);
                return true;
            }
        }
    }
    return false;
}
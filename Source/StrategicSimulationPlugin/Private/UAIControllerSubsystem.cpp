#include "UAIControllerSubsystem.h"
#include "UResourceManagerSubsystem.h"
#include "USoldierManagerSubsystem.h"
#include "UEngineeringManagerSubsystem.h"
#include "UStrategyEventDispatcher.h"
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

void UAIControllerSubsystem::Debug_RunAI()
{
    UE_LOG(LogTemp, Display, TEXT("[AI DEBUG] Manual AI run requested by player"));
    RunAIForFaction(EFactionType::Enemy, 999);
}

void UAIControllerSubsystem::RunAIForFaction(EFactionType Faction, int32 CurrentDay)
{
    UE_LOG(LogTemp, Display, TEXT("[AI] %s — Day %d decision (full build order)"), *UEnum::GetValueAsString(Faction), CurrentDay);

    // 1. Advance construction and research every day
    if (UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>())
        BaseMgr->AdvanceFacilityConstruction(Faction);

    if (UResearchManagerSubsystem* ResearchMgr = GetGameInstance()->GetSubsystem<UResearchManagerSubsystem>())
        ResearchMgr->AdvanceDay(Faction);

    // 2. Facility-based income (Phase 23 — only this should add resources now)
    if (UResourceManagerSubsystem* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>())
        ResourceMgr->ApplyFacilityIncome(Faction);

    // 3. Build order — prioritize basic barracks early for recruiting
    if (TryBuildFacility(Faction, EFacilityType::Command)) return;
    if (TryBuildFacility(Faction, EFacilityType::PowerPlant)) return;
    if (TryBuildFacility(Faction, EFacilityType::LivingQuarters)) return;   // recruit as soon as possible
    if (TryBuildFacility(Faction, EFacilityType::Medical)) return;
    if (TryBuildFacility(Faction, EFacilityType::Workshop)) return;
    if (TryBuildFacility(Faction, EFacilityType::Laboratory)) return;
    if (TryBuildFacility(Faction, EFacilityType::Storage)) return;
    if (TryBuildFacility(Faction, EFacilityType::Defense)) return;

    if (TryBuildFacility(Faction, EFacilityType::Special)) return;

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

    UE_LOG(LogTemp, Display, TEXT("[PURCHASE] === %s starting buy round (smart priority) ==="), *UEnum::GetValueAsString(Faction));

    bool bBoughtAnything = false;
    int32 PurchasesThisDay = 0;
    const int32 MaxPurchasesPerDay = 4;

    while (PurchasesThisDay < MaxPurchasesPerDay)
    {
        // Re-select poorest soldier every purchase
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

        if (MinItems >= 10) break;   // prevent over-equipping

        FResourceStockpile Res = ResourceMgr->GetResources(Faction);

        bool bPurchasedThisLoop = false;

        for (const TSoftObjectPtr<UItemDefinition>& SoftItem : ItemDB->BuyableItems)
        {
            UItemDefinition* ItemDef = SoftItem.Get();
            if (!ItemDef) continue;

            if (!Campaign->IsItemUnlocked(Faction, ItemDef)) continue;
            if (TargetSoldier->CurrentLoadout.Contains(ItemDef)) continue;

            if (Res.Money >= ItemDef->PurchaseCost.Money)
            {
                if (EngineeringMgr->PurchaseItem(Faction, ItemDef, TargetSoldier))
                {
                    UE_LOG(LogTemp, Display, TEXT("[AI] ✅ Bought %s on soldier (now has %d items)"),
                        *ItemDef->ItemName.ToString(), TargetSoldier->CurrentLoadout.Num());

                    // Broadcast loadout changed event so UI can refresh efficiently
                    if (UStrategyEventDispatcher* EventDisp = GetGameInstance()->GetSubsystem<UStrategyEventDispatcher>())
                    {
                        EventDisp->OnSoldierLoadoutChanged.Broadcast(Faction, TargetSoldier);
                    }

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

bool UAIControllerSubsystem::TryBuildFacility(EFactionType Faction, EFacilityType FacilityTypeToBuild)
{
    UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();
    UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    UResourceManagerSubsystem* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>();

    if (!Campaign || !BaseMgr || !ResourceMgr) return false;

    UFacilityDatabase* DB = Campaign->FacilityDatabaseAsset.Get();
    if (!DB)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AI] FacilityDatabase is null!"));
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
        UE_LOG(LogTemp, Warning, TEXT("[AI] No facility of type %s in FacilityDatabase!"), *UEnum::GetValueAsString(FacilityTypeToBuild));
        return false;
    }

    // NEW: Respect per-facility MaxBuilt limit
    int32 CurrentCount = BaseMgr->GetCurrentCountOfType(Faction, FacilityTypeToBuild);
    if (CurrentCount >= FacilityDef->MaxBuilt)
    {
        UE_LOG(LogTemp, Verbose, TEXT("[AI] %s already has max %d of %s"),
            *UEnum::GetValueAsString(Faction), FacilityDef->MaxBuilt, *FacilityDef->FacilityName.ToString());
        return false;
    }

    // Resource check
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
    UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();

    if (!SoldierMgr || !ResourceMgr || !Campaign || !BaseMgr)
    {
        UE_LOG(LogTemp, Warning, TEXT("[RECRUIT] Missing required subsystems!"));
        return false;
    }

    USoldierClassDatabase* SoldierDB = Campaign->SoldierClassDatabaseAsset.Get();
    if (!SoldierDB)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AI] SoldierClassDatabase is null!"));
        return false;
    }

    UE_LOG(LogTemp, Display, TEXT("[AI] SoldierClassDatabase has %d classes"), SoldierDB->AvailableSoldierClasses.Num());

    USoldierClassDefinition* ClassDef = nullptr;
    for (const TSoftObjectPtr<USoldierClassDefinition>& SoftClass : SoldierDB->AvailableSoldierClasses)
    {
        if (USoldierClassDefinition* Def = SoftClass.Get())
        {
            UE_LOG(LogTemp, Display, TEXT("[AI] Found soldier class: %s"), *Def->ClassName.ToString());
            ClassDef = Def;
            break;
        }
    }

    if (!ClassDef)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AI] No soldier class in SoldierClassDatabase!"));
        return false;
    }

    int32 CurrentCapacity = BaseMgr->GetTotalBarracksCapacity(Faction);
    int32 CurrentSoldiers = SoldierMgr->GetRoster(Faction).Num();

    UE_LOG(LogTemp, Display, TEXT("[AI] Barracks capacity for %s: %d/%d"), *UEnum::GetValueAsString(Faction), CurrentSoldiers, CurrentCapacity);

    if (CurrentSoldiers >= CurrentCapacity)
    {
        UE_LOG(LogTemp, Warning, TEXT("[RECRUIT] %s barracks full (%d/%d) — cannot recruit"),
            *UEnum::GetValueAsString(Faction), CurrentSoldiers, CurrentCapacity);
        return false;
    }

    FResourceStockpile Res = ResourceMgr->GetResources(Faction);
    if (Res.Money < 500)
    {
        UE_LOG(LogTemp, Warning, TEXT("[RECRUIT] %s cannot afford recruit (needs 500 Money)"), *UEnum::GetValueAsString(Faction));
        return false;
    }

    if (SoldierMgr->RecruitSoldier(Faction, ClassDef))
    {
        UE_LOG(LogTemp, Display, TEXT("[AI] ✅ %s recruited a new soldier (%s)"), *UEnum::GetValueAsString(Faction), *ClassDef->ClassName.ToString());
        return true;
    }

    UE_LOG(LogTemp, Warning, TEXT("[RECRUIT] RecruitSoldier call failed"));
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
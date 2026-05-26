#include "UAIControllerSubsystem.h"
#include "UResourceManagerSubsystem.h"
#include "USoldierManagerSubsystem.h"
#include "UEngineeringManagerSubsystem.h"
#include "UStrategyEventDispatcher.h"
#include "UItemDatabase.h"
#include "USoldierClassDatabase.h"
#include "UStrategyCampaignSubsystem.h"
#include "UFacilityDatabase.h"
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
    if (!bAIEnabled || Faction != EFactionType::Enemy) return;

    UE_LOG(LogTemp, Display, TEXT("[AI] %s — Day %d decision (full build order)"), *UEnum::GetValueAsString(Faction), CurrentDay);

    UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    UResearchManagerSubsystem* ResearchMgr = GetGameInstance()->GetSubsystem<UResearchManagerSubsystem>();
    UEngineeringManagerSubsystem* EngMgr = GetGameInstance()->GetSubsystem<UEngineeringManagerSubsystem>();
    UResourceManagerSubsystem* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>();
    USoldierManagerSubsystem* SoldierMgr = GetGameInstance()->GetSubsystem<USoldierManagerSubsystem>();

    if (!BaseMgr || !ResearchMgr || !EngMgr || !ResourceMgr || !SoldierMgr) return;

    // 1. Daily maintenance (always happens)
    BaseMgr->AdvanceFacilityConstruction(Faction);
    ResearchMgr->AdvanceDay(Faction);
    ResourceMgr->ApplyFacilityIncome(Faction);
    EngMgr->OnDayPassed(CurrentDay);

    // Power status
    int32 NetPower = BaseMgr->GetNetPower(Faction);
    UE_LOG(LogTemp, Display, TEXT("[POWER] %s Net Power: %d (Provided %d | Draw %d)"),
        *UEnum::GetValueAsString(Faction), NetPower, BaseMgr->GetTotalPowerProvided(Faction), BaseMgr->GetTotalPowerDrawn(Faction));

    // 2. Emergency power fix
    if (NetPower < 0)
    {
        if (TryBuildFacility(Faction, EFacilityType::PowerPlant)) return;
    }

    // === PHASE 4: AI BASE EXPANSION (your exact rules) ===
    if (BaseMgr->CanBuildNewBase(Faction))
    {
        if (BaseMgr->HasFacilityOfType(Faction, EFacilityType::Hanger) &&
            BaseMgr->HasFacilityOfType(Faction, EFacilityType::Workshop) &&
            BaseMgr->HasFacilityOfType(Faction, EFacilityType::Laboratory) &&
            BaseMgr->HasFacilityOfType(Faction, EFacilityType::LivingQuarters))
        {
            FVector2D NewLocation = FVector2D(FMath::RandRange(100.f, 1820.f), FMath::RandRange(100.f, 980.f));
            FText BaseName = FText::FromString(FString::Printf(TEXT("Forward Base %d"), BaseMgr->GetBases(Faction).Num() + 1));

            UStrategyBase* NewBase = BaseMgr->BuildNewBase(Faction, BaseName, NewLocation);
            if (NewBase)
            {
                // No need to call TryBuildFacility — BuildNewBase now automatically starts the Command Center
                return; // only one major action per day
            }
        }
    }

    // === NEW HIGH-PRIORITY: Finish Command Centers in ANY incomplete base ===
    // This prevents the AI from trying to build Power/anything else in a new base before its Command Center
    for (UStrategyBase* Base : BaseMgr->GetBases(Faction))
    {
        if (Base && !Base->HasOperationalCommandCenter())
        {
            if (TryBuildFacility(Faction, EFacilityType::Command, Base))
            {
                UE_LOG(LogTemp, Display, TEXT("[AI] Prioritizing Command Center completion in base '%s'"), *Base->BaseName.ToString());
                return; // only one major action per day
            }
        }
    }

    // 3. Early game priority (get the base running fast)
    if (BaseMgr->GetCurrentCountOfType(Faction, EFacilityType::Command) == 0)
    {
        if (TryBuildFacility(Faction, EFacilityType::Command)) return;
    }
    if (BaseMgr->GetCurrentCountOfType(Faction, EFacilityType::PowerPlant) == 0)
    {
        if (TryBuildFacility(Faction, EFacilityType::PowerPlant)) return;
    }

    // 4. Get at least 2 barracks quickly
    int32 BarracksCount = BaseMgr->GetCurrentCountOfType(Faction, EFacilityType::LivingQuarters);
    if (BarracksCount < 2)
    {
        if (TryBuildFacility(Faction, EFacilityType::LivingQuarters)) return;
    }

    // 5. Recruit as soon as we have barracks
    int32 CurrentSoldiers = SoldierMgr->GetRoster(Faction).Num();
    int32 MaxCapacity = BaseMgr->GetTotalBarracksCapacity(Faction);
    if (CurrentSoldiers < MaxCapacity && BarracksCount >= 1)
    {
        TryRecruit(Faction);
    }

    // 6. Research and production once we have a basic base
    if (BarracksCount >= 1)
    {
        TryResearch(Faction);
        EngMgr->TryProduce(Faction);
    }

    // 7. Expand the base (after we have soldiers and basic research)
    if (CurrentSoldiers >= 4)
    {
        TryBuildFacility(Faction, EFacilityType::Medical);
        TryBuildFacility(Faction, EFacilityType::Workshop);
        TryBuildFacility(Faction, EFacilityType::Laboratory);
        TryBuildFacility(Faction, EFacilityType::Storage);
        TryBuildFacility(Faction, EFacilityType::Defense);
        TryBuildFacility(Faction, EFacilityType::Hanger);
    }

    // 8. Always try to outfit soldiers
    TryBuyAndEquip(Faction);

    // Optional: extra power if still low
    if (NetPower < 20)
    {
        TryBuildFacility(Faction, EFacilityType::PowerPlant);
    }
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

bool UAIControllerSubsystem::TryBuildFacility(EFactionType Faction, EFacilityType FacilityTypeToBuild, UStrategyBase* TargetBase /*= nullptr*/)
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

    // Respect per-facility MaxBuilt limit
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

    // FIXED: Pass TargetBase when building in a new base
    if (BaseMgr->BuildFacility(Faction, FacilityDef, TargetBase))
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

void UAIControllerSubsystem::SetAIEnabled(bool bEnable)
{
    bAIEnabled = bEnable;
    UE_LOG(LogTemp, Display, TEXT("AI Controller %s for Enemy faction"), bAIEnabled ? TEXT("ENABLED") : TEXT("DISABLED"));
}

bool UAIControllerSubsystem::IsAIEnabled() const
{
    return bAIEnabled;
}
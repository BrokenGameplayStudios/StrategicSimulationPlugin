#include "UAIControllerSubsystem.h"
#include "UResourceManagerSubsystem.h"
#include "USoldierManagerSubsystem.h"
#include "USoldierClassDatabase.h"
#include "UEngineeringManagerSubsystem.h"
#include "UStrategyEventDispatcher.h"
#include "UItemDatabase.h"
#include "UStrategyCampaignSubsystem.h"
#include "UFacilityDatabase.h"
#include "UStrategyBase.h"
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

    UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    UResourceManagerSubsystem* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>();
    USoldierManagerSubsystem* SoldierMgr = GetGameInstance()->GetSubsystem<USoldierManagerSubsystem>();
    UResearchManagerSubsystem* ResearchMgr = GetGameInstance()->GetSubsystem<UResearchManagerSubsystem>();
    UEngineeringManagerSubsystem* EngineeringMgr = GetGameInstance()->GetSubsystem<UEngineeringManagerSubsystem>();

    if (!BaseMgr || !ResourceMgr) return;

    UE_LOG(LogTemp, Display, TEXT("[AI] %s — Day %d decision (full build order) - Bases: %d"),
        *UEnum::GetValueAsString(Faction), CurrentDay, BaseMgr->GetBases(Faction).Num());

    // === FORCE INITIAL BASE CREATION ===
    if (BaseMgr->GetBases(Faction).Num() == 0)
    {
        UE_LOG(LogTemp, Display, TEXT("[AI] No bases exist — creating initial Command Center base"));
        FVector2D NewLocation = FVector2D(960.0f, 540.0f);
        FText BaseName = FText::FromString("Command Center");

        if (UStrategyBase* NewBase = BaseMgr->BuildNewBase(Faction, BaseName, NewLocation))
        {
            UE_LOG(LogTemp, Display, TEXT("[AI] ✅ Initial 'Command Center' base created successfully"));
            return;
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("[AI] FAILED to create initial base!"));
            return;
        }
    }

    // === CRITICAL DAILY PROGRESS ===
    BaseMgr->AdvanceFacilityConstruction(Faction);
    ResourceMgr->ApplyFacilityIncome(Faction);

    // === PHASE 4: BASE EXPANSION ===
    if (BaseMgr->CanBuildNewBase(Faction))
    {
        int32 OperationalHangers = BaseMgr->GetNumberOfOperationalHangers(Faction);
        if (OperationalHangers >= 1)
        {
            FVector2D NewLocation = FVector2D(FMath::RandRange(100.0f, 1820.0f), FMath::RandRange(100.0f, 980.0f));
            FText BaseName = FText::FromString(FString::Printf(TEXT("Forward Base %d"), BaseMgr->GetBases(Faction).Num() + 1));

            if (UStrategyBase* NewBase = BaseMgr->BuildNewBase(Faction, BaseName, NewLocation))
            {
                UE_LOG(LogTemp, Display, TEXT("[AI] ✅ Expanded to new base '%s'"), *BaseName.ToString());
                return;
            }
        }
    }

    // === DEVELOP EVERY BASE IN PARALLEL ===
    for (UStrategyBase* Base : BaseMgr->GetBases(Faction))
    {
        if (!Base) continue;

        if (!Base->HasOperationalCommandCenter())
        {
            UE_LOG(LogTemp, Display, TEXT("[AI] Skipping base '%s' — Command Center not yet operational"), *Base->BaseName.ToString());
            continue;
        }

        UE_LOG(LogTemp, Display, TEXT("[AI] Developing base '%s' (Command Center operational, Net Power: %d)"),
            *Base->BaseName.ToString(), Base->GetNetPower());

        // 1. POWER IS THE ABSOLUTE HIGHEST PRIORITY
        if (!Base->HasFacilityOfType(EFacilityType::PowerPlant))
        {
            UE_LOG(LogTemp, Display, TEXT("[AI] → Trying PowerPlant in '%s'"), *Base->BaseName.ToString());
            if (TryBuildFacility(Faction, EFacilityType::PowerPlant, Base))
                UE_LOG(LogTemp, Display, TEXT("[AI] → SUCCESS PowerPlant started in '%s'"), *Base->BaseName.ToString());
        }

        // 2. Core facilities (one of each)
        if (!Base->HasFacilityOfType(EFacilityType::LivingQuarters))
        {
            UE_LOG(LogTemp, Display, TEXT("[AI] → Trying LivingQuarters in '%s'"), *Base->BaseName.ToString());
            if (TryBuildFacility(Faction, EFacilityType::LivingQuarters, Base))
                UE_LOG(LogTemp, Display, TEXT("[AI] → SUCCESS LivingQuarters started in '%s'"), *Base->BaseName.ToString());
        }

        if (!Base->HasFacilityOfType(EFacilityType::Storage))
        {
            UE_LOG(LogTemp, Display, TEXT("[AI] → Trying Storage in '%s'"), *Base->BaseName.ToString());
            if (TryBuildFacility(Faction, EFacilityType::Storage, Base))
                UE_LOG(LogTemp, Display, TEXT("[AI] → SUCCESS Storage started in '%s'"), *Base->BaseName.ToString());
        }

        if (!Base->HasFacilityOfType(EFacilityType::Workshop))
        {
            UE_LOG(LogTemp, Display, TEXT("[AI] → Trying Workshop in '%s'"), *Base->BaseName.ToString());
            if (TryBuildFacility(Faction, EFacilityType::Workshop, Base))
                UE_LOG(LogTemp, Display, TEXT("[AI] → SUCCESS Workshop started in '%s'"), *Base->BaseName.ToString());
        }

        if (!Base->HasFacilityOfType(EFacilityType::Laboratory))
        {
            UE_LOG(LogTemp, Display, TEXT("[AI] → Trying Laboratory in '%s'"), *Base->BaseName.ToString());
            if (TryBuildFacility(Faction, EFacilityType::Laboratory, Base))
                UE_LOG(LogTemp, Display, TEXT("[AI] → SUCCESS Laboratory started in '%s'"), *Base->BaseName.ToString());
        }

        if (!Base->HasFacilityOfType(EFacilityType::Hanger))
        {
            UE_LOG(LogTemp, Display, TEXT("[AI] → Trying Hanger in '%s'"), *Base->BaseName.ToString());
            if (TryBuildFacility(Faction, EFacilityType::Hanger, Base))
                UE_LOG(LogTemp, Display, TEXT("[AI] → SUCCESS Hanger started in '%s'"), *Base->BaseName.ToString());
        }

        // 3. Extras only after core is complete
        if (Base->GetTotalCapacityForType(EFacilityType::LivingQuarters) < 12)
        {
            UE_LOG(LogTemp, Display, TEXT("[AI] → Trying extra LivingQuarters in '%s'"), *Base->BaseName.ToString());
            if (TryBuildFacility(Faction, EFacilityType::LivingQuarters, Base))
                UE_LOG(LogTemp, Display, TEXT("[AI] → SUCCESS extra LivingQuarters started in '%s'"), *Base->BaseName.ToString());
        }

        // Extra storage and hangers
        UE_LOG(LogTemp, Display, TEXT("[AI] → Trying extra Storage/Hanger in '%s'"), *Base->BaseName.ToString());
        TryBuildFacility(Faction, EFacilityType::Storage, Base);
        TryBuildFacility(Faction, EFacilityType::Hanger, Base);
    }

    // === RECRUIT, RESEARCH, PURCHASE, PRODUCTION ===
    // Now uses the new TryRecruit() which handles per-base barracks capacity + TargetBase
    if (SoldierMgr && TryRecruit(Faction))
    {
        return;
    }

    if (ResearchMgr && TryResearch(Faction)) return;
    if (TryBuyAndEquip(Faction)) return;
    if (EngineeringMgr && EngineeringMgr->TryProduce(Faction)) return;

    UE_LOG(LogTemp, Display, TEXT("[AI] %s — End of day %d (no major action taken)"), *UEnum::GetValueAsString(Faction), CurrentDay);
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

    // Respect MaxBuilt limit
    int32 CurrentCount = BaseMgr->GetCurrentCountOfType(Faction, FacilityTypeToBuild);
    if (CurrentCount >= FacilityDef->MaxBuilt)
    {
        return false;
    }

    // Resource check
    FResourceStockpile Res = ResourceMgr->GetResources(Faction);
    if (Res.Money < FacilityDef->BuildCost.Money) return false;

    // Use the provided target base (or let BuildFacility decide)
    if (BaseMgr->BuildFacility(Faction, FacilityDef, TargetBase))
    {
        UE_LOG(LogTemp, Display, TEXT("[AI] ✅ %s started construction of %s in base '%s' (%d days)"),
            *UEnum::GetValueAsString(Faction), *FacilityDef->FacilityName.ToString(),
            TargetBase ? *TargetBase->BaseName.ToString() : TEXT("default base"), FacilityDef->BuildTimeDays);
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
    if (!SoldierDB || SoldierDB->AvailableSoldierClasses.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AI] No soldier classes available!"));
        return false;
    }

    USoldierClassDefinition* ClassDef = SoldierDB->AvailableSoldierClasses[0].Get();
    if (!ClassDef)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AI] Soldier class is null!"));
        return false;
    }

    // NEW: Find a base with available barracks capacity (strictly per-base)
    UStrategyBase* TargetBase = nullptr;
    const TArray<UStrategyBase*>& Bases = BaseMgr->GetBases(Faction);
    for (UStrategyBase* Base : Bases)
    {
        if (Base)
        {
            int32 Cap = Base->GetTotalCapacityForType(EFacilityType::LivingQuarters);
            int32 Stationed = SoldierMgr->GetNumSoldiersStationedAt(Base, Faction);
            if (Stationed < Cap)
            {
                TargetBase = Base;
                break; // first base with space
            }
        }
    }

    if (!TargetBase)
    {
        UE_LOG(LogTemp, Warning, TEXT("[RECRUIT] No base with available barracks capacity for %s"), *UEnum::GetValueAsString(Faction));
        return false;
    }

    FResourceStockpile Res = ResourceMgr->GetResources(Faction);
    if (Res.Money < 500)
    {
        UE_LOG(LogTemp, Warning, TEXT("[RECRUIT] %s cannot afford recruit (needs 500 Money)"), *UEnum::GetValueAsString(Faction));
        return false;
    }

    if (SoldierMgr->RecruitSoldier(Faction, ClassDef, TargetBase))
    {
        UE_LOG(LogTemp, Display, TEXT("[AI] %s recruited a new soldier (%s) at base '%s'"),
            *UEnum::GetValueAsString(Faction), *ClassDef->ClassName.ToString(), *TargetBase->BaseName.ToString());
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
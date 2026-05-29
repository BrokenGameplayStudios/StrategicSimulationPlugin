#include "UAIControllerSubsystem.h"
#include "UResourceManagerSubsystem.h"
#include "USoldierManagerSubsystem.h"
#include "USoldierClassDatabase.h"
#include "UEngineeringManagerSubsystem.h"
#include "UStrategyEventDispatcher.h"
#include "UItemDatabase.h"
#include "UStrategyCampaignSubsystem.h"
#include "UFacilityDatabase.h"
#include "UVehicleDatabase.h"
#include "UStrategyBase.h"
#include "UStrategyVehicle.h"
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

    BaseMgr->AdvanceFacilityConstruction(Faction);
    ResourceMgr->ApplyFacilityIncome(Faction);

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

    for (UStrategyBase* Base : BaseMgr->GetBases(Faction))
    {
        if (!Base) continue;
        if (!Base->HasOperationalCommandCenter()) continue;

        UE_LOG(LogTemp, Display, TEXT("[AI] Developing base '%s' (Net Power: %d)"), *Base->BaseName.ToString(), Base->GetNetPower());

        if (Base->GetNetPower() < 0 || !Base->HasOperationalFacilityOfType(EFacilityType::PowerPlant))
        {
            if (TryBuildFacility(Faction, EFacilityType::PowerPlant, Base))
                continue;
        }

        int32 CurrentCapacity = Base->GetTotalCapacityForType(EFacilityType::LivingQuarters);
        int32 CurrentSoldiers = SoldierMgr ? SoldierMgr->GetNumSoldiersStationedAt(Base, Faction) : 0;

        if (CurrentCapacity < 36 || CurrentSoldiers >= CurrentCapacity - 4)
        {
            UE_LOG(LogTemp, Display, TEXT("[AI] → BARRACKS NEAR FULL (%d/%d) — Trying extra LivingQuarters in '%s'"),
                CurrentSoldiers, CurrentCapacity, *Base->BaseName.ToString());
            if (TryBuildFacility(Faction, EFacilityType::LivingQuarters, Base))
                UE_LOG(LogTemp, Display, TEXT("[AI] → SUCCESS extra LivingQuarters started in '%s'"), *Base->BaseName.ToString());
        }

        if (!Base->HasFacilityOfType(EFacilityType::Storage)) TryBuildFacility(Faction, EFacilityType::Storage, Base);
        if (!Base->HasFacilityOfType(EFacilityType::Workshop)) TryBuildFacility(Faction, EFacilityType::Workshop, Base);
        if (!Base->HasFacilityOfType(EFacilityType::Laboratory)) TryBuildFacility(Faction, EFacilityType::Laboratory, Base);

        // Medical Bay (now included naturally)
        if (!Base->HasFacilityOfType(EFacilityType::Medical)) TryBuildFacility(Faction, EFacilityType::Medical, Base);

        if (!Base->HasOperationalFacilityOfType(EFacilityType::Hanger)) TryBuildFacility(Faction, EFacilityType::Hanger, Base);

        // === VehicleRepair bay logic (1 per operational hanger) ===
        int32 OperationalHangers = 0;
        for (UStrategyFacility* Fac : Base->Facilities)
        {
            if (Fac && Fac->FacilityDefinition && Fac->FacilityDefinition->FacilityType == EFacilityType::Hanger && Fac->bIsOperational)
                OperationalHangers++;
        }

        int32 CurrentRepairBays = 0;
        int32 RepairUnderConstruction = 0;
        for (UStrategyFacility* Fac : Base->Facilities)
        {
            if (Fac && Fac->FacilityDefinition && Fac->FacilityDefinition->FacilityType == EFacilityType::VehicleRepair)
            {
                if (Fac->bIsOperational)
                    CurrentRepairBays++;
                else
                    RepairUnderConstruction++;
            }
        }

        if (CurrentRepairBays + RepairUnderConstruction < OperationalHangers)
        {
            UE_LOG(LogTemp, Display, TEXT("[AI] → Building VehicleRepair bay for hanger in '%s'"), *Base->BaseName.ToString());
            TryBuildFacility(Faction, EFacilityType::VehicleRepair, Base);
        }

        if (Base->HasOperationalFacilityOfType(EFacilityType::Hanger))
        {
            if (TryBuildVehicle(Faction, Base))
                continue;
        }

        if (Base->HasOperationalFacilityOfType(EFacilityType::Hanger))
        {
            if (UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
            {
                if (UMissionGroup* Mission = MissionMgr->LaunchMissionFromBase(Base, 15))
                {
                    UE_LOG(LogTemp, Display, TEXT("[AI] Launched mission from base '%s'"), *Base->BaseName.ToString());
                }
            }
        }

        UE_LOG(LogTemp, Display, TEXT("[AI] → Trying extra Storage/Hanger/Power in '%s'"), *Base->BaseName.ToString());
        TryBuildFacility(Faction, EFacilityType::Storage, Base);
        TryBuildFacility(Faction, EFacilityType::Hanger, Base);
        if (Base->GetNetPower() < 100) TryBuildFacility(Faction, EFacilityType::PowerPlant, Base);
    }

    bool bRecruited = (SoldierMgr && TryRecruit(Faction));
    if (bRecruited) UE_LOG(LogTemp, Display, TEXT("[AI] Recruited soldier — continuing..."));

    if (ResearchMgr && TryResearch(Faction)) UE_LOG(LogTemp, Display, TEXT("[AI] Research action taken"));
    if (TryBuyAndEquip(Faction)) UE_LOG(LogTemp, Display, TEXT("[AI] Purchase/equip action taken"));
    if (EngineeringMgr && EngineeringMgr->TryProduce(Faction)) UE_LOG(LogTemp, Display, TEXT("[AI] Production action taken"));

    UE_LOG(LogTemp, Display, TEXT("[AI] %s — End of day %d (actions completed)"), *UEnum::GetValueAsString(Faction), CurrentDay);
}

// === IMPROVED TryBuildVehicle — respects HomeHanger reservations for vehicles on mission ===
bool UAIControllerSubsystem::TryBuildVehicle(EFactionType Faction, UStrategyBase* TargetBase)
{
    UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>();
    if (!Campaign) return false;

    UVehicleDatabase* VehicleDB = Campaign->VehicleDatabaseAsset.Get();
    if (!VehicleDB || VehicleDB->AvailableVehicles.Num() == 0) return false;

    UVehicleDefinition* VehDef = VehicleDB->AvailableVehicles[0].Get();
    if (!VehDef) return false;

    UResourceManagerSubsystem* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>();
    if (!ResourceMgr) return false;

    FResourceStockpile Res = ResourceMgr->GetResources(Faction);
    if (Res.Money < VehDef->BuildCost.Money || Res.Supplies < VehDef->BuildCost.Supplies)
    {
        return false;
    }

    for (UStrategyFacility* Hanger : TargetBase->Facilities)
    {
        if (!Hanger || !Hanger->FacilityDefinition || Hanger->FacilityDefinition->FacilityType != EFacilityType::Hanger || !Hanger->bIsOperational)
            continue;

        int32 Capacity = Hanger->FacilityDefinition->Capacity;
        int32 CurrentParked = Hanger->ParkedVehicles.Num();

        // Count slots reserved by vehicles currently on mission that own this hanger
        int32 ReservedByOnMission = 0;
        for (UStrategyFacility* AnyHanger : TargetBase->Facilities)
        {
            for (UStrategyVehicle* V : AnyHanger->ParkedVehicles)
            {
                if (V && V->HomeHanger == Hanger && V->CurrentMission != nullptr)
                    ReservedByOnMission++;
            }
        }

        int32 EffectiveFreeSlots = Capacity - CurrentParked - ReservedByOnMission;

        if (EffectiveFreeSlots > 0)
        {
            ResourceMgr->AddResources(Faction, { -VehDef->BuildCost.Money, -VehDef->BuildCost.Supplies, 0, 0 });

            UStrategyVehicle* NewVehicle = NewObject<UStrategyVehicle>();
            NewVehicle->VehicleDefinition = VehDef;
            NewVehicle->HomeBase = TargetBase;
            NewVehicle->RemainingFuelDays = VehDef->MaxMissionDurationDays;

            NewVehicle->CurrentHanger = Hanger;
            Hanger->ParkedVehicles.Add(NewVehicle);

            UE_LOG(LogTemp, Display, TEXT("[AI] %s completed construction of vehicle '%s' in base '%s' → parked in hanger (Slot %d of %d)"),
                *UEnum::GetValueAsString(Faction), *VehDef->VehicleName.ToString(), *TargetBase->BaseName.ToString(),
                CurrentParked + 1, Capacity);

            return true;
        }
    }

    UE_LOG(LogTemp, Verbose, TEXT("[AI] No truly free hanger slots in base '%s' (all slots reserved by vehicles on mission)"), *TargetBase->BaseName.ToString());
    return false;
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

        if (MinItems >= 10) break;

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

bool UAIControllerSubsystem::TryBuildFacility(EFactionType Faction, EFacilityType FacilityTypeToBuild, UStrategyBase* TargetBase)
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

    if (TargetBase)
    {
        int32 CurrentCountInBase = 0;
        for (UStrategyFacility* Fac : TargetBase->Facilities)
        {
            if (Fac && Fac->FacilityDefinition && Fac->FacilityDefinition->FacilityType == FacilityTypeToBuild)
            {
                CurrentCountInBase++;
            }
        }

        if (CurrentCountInBase >= FacilityDef->MaxBuilt)
        {
            UE_LOG(LogTemp, Verbose, TEXT("[AI] MaxBuilt reached for %s in base '%s' (%d/%d) — skipping"),
                *UEnum::GetValueAsString(FacilityTypeToBuild), *TargetBase->BaseName.ToString(), CurrentCountInBase, FacilityDef->MaxBuilt);
            return false;
        }
    }

    FResourceStockpile Res = ResourceMgr->GetResources(Faction);
    if (Res.Money < FacilityDef->BuildCost.Money)
    {
        UE_LOG(LogTemp, Verbose, TEXT("[AI] Not enough money for %s (needs %d)"), *FacilityDef->FacilityName.ToString(), FacilityDef->BuildCost.Money);
        return false;
    }

    if (BaseMgr->BuildFacility(Faction, FacilityDef, TargetBase))
    {
        UE_LOG(LogTemp, Display, TEXT("[AI] %s started construction of %s in base '%s' (%d days)"),
            *UEnum::GetValueAsString(Faction), *FacilityDef->FacilityName.ToString(),
            TargetBase ? *TargetBase->BaseName.ToString() : TEXT("default base"), FacilityDef->BuildTimeDays);
        return true;
    }

    UE_LOG(LogTemp, Warning, TEXT("[AI] BuildFacility failed for %s in base '%s'"),
        *FacilityDef->FacilityName.ToString(), TargetBase ? *TargetBase->BaseName.ToString() : TEXT("unknown"));
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

    UStrategyBase* TargetBase = nullptr;
    int32 BestAvailableSlots = 0;

    const TArray<UStrategyBase*>& Bases = BaseMgr->GetBases(Faction);
    for (UStrategyBase* Base : Bases)
    {
        if (Base)
        {
            int32 Cap = Base->GetTotalCapacityForType(EFacilityType::LivingQuarters);
            int32 Stationed = SoldierMgr->GetNumSoldiersStationedAt(Base, Faction);
            int32 Available = Cap - Stationed;

            if (Available > BestAvailableSlots)
            {
                BestAvailableSlots = Available;
                TargetBase = Base;
            }
        }
    }

    if (!TargetBase || BestAvailableSlots <= 0)
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
        int32 CurrentCapacity = TargetBase->GetTotalCapacityForType(EFacilityType::LivingQuarters);
        int32 CurrentSoldiers = SoldierMgr->GetNumSoldiersStationedAt(TargetBase, Faction);

        UE_LOG(LogTemp, Display, TEXT("[AI] %s recruited a new soldier (%s) at base '%s' - Capacity %d/%d"),
            *UEnum::GetValueAsString(Faction), *ClassDef->ClassName.ToString(), *TargetBase->BaseName.ToString(),
            CurrentSoldiers, CurrentCapacity);

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

    if (!Campaign || !ResearchMgr || !ResourceMgr)
    {
        UE_LOG(LogTemp, Warning, TEXT("[RESEARCH] Missing required subsystems!"));
        return false;
    }

    UResearchDatabase* ResearchDB = Campaign->ResearchDatabaseAsset.Get();
    if (!ResearchDB)
    {
        UE_LOG(LogTemp, Warning, TEXT("[RESEARCH] ResearchDatabaseAsset not loaded!"));
        return false;
    }
    if (ResearchDB->AvailableTechs.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[RESEARCH] No AvailableTechs in ResearchDatabase!"));
        return false;
    }

    UE_LOG(LogTemp, Display, TEXT("[RESEARCH] %s checking %d available techs..."),
        *UEnum::GetValueAsString(Faction), ResearchDB->AvailableTechs.Num());

    FResourceStockpile Res = ResourceMgr->GetResources(Faction);

    for (const TSoftObjectPtr<UResearchTechDefinition>& SoftResearch : ResearchDB->AvailableTechs)
    {
        UResearchTechDefinition* ResearchDef = SoftResearch.Get();
        if (!ResearchDef) continue;

        if (ResearchMgr->IsResearchInProgress(Faction, ResearchDef) || ResearchMgr->HasCompletedResearch(Faction, ResearchDef))
        {
            UE_LOG(LogTemp, Verbose, TEXT("[RESEARCH] Skipping %s — already in progress or completed"), *ResearchDef->ProjectName.ToString());
            continue;
        }

        if (Res.Money < ResearchDef->ResearchCost.Money)
        {
            UE_LOG(LogTemp, Verbose, TEXT("[RESEARCH] Skipping %s — not enough money (%d needed)"),
                *ResearchDef->ProjectName.ToString(), ResearchDef->ResearchCost.Money);
            continue;
        }

        if (ResearchMgr->StartResearch(Faction, ResearchDef))
        {
            UE_LOG(LogTemp, Display, TEXT("[AI] %s started research: %s (%d days)"),
                *UEnum::GetValueAsString(Faction), *ResearchDef->ProjectName.ToString(), ResearchDef->ResearchDays);
            return true;
        }
    }

    UE_LOG(LogTemp, Verbose, TEXT("[RESEARCH] No suitable tech found this day"));
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
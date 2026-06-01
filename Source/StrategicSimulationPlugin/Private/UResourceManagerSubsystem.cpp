#include "UResourceManagerSubsystem.h"
#include "UBaseManagerSubsystem.h"
#include "Engine/Engine.h"

void UResourceManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Default starting values (you can override these from AStrategyGameInitializer)
    HumanStartingResources = FResourceStockpile{ 28000, 3200, 1200, 1500, 0, 0 };
    EnemyStartingResources = FResourceStockpile{ 25000, 2800, 1000, 1200, 0, 0 };

    FactionResources.Add(EFactionType::Human, HumanStartingResources);
    FactionResources.Add(EFactionType::Enemy, EnemyStartingResources);

    UE_LOG(LogTemp, Display, TEXT("[RESOURCES] Human start: %d💰 %d🛠️ %d🧬 %d⚗️ %d🌌 %d📚"),
        HumanStartingResources.Money, HumanStartingResources.Metals, HumanStartingResources.Biologicals,
        HumanStartingResources.Chemicals, HumanStartingResources.ExoticMaterial, HumanStartingResources.ResearchPoints);

    UE_LOG(LogTemp, Display, TEXT("[RESOURCES] Enemy start: %d💰 %d🛠️ %d🧬 %d⚗️ %d🌌 %d📚"),
        EnemyStartingResources.Money, EnemyStartingResources.Metals, EnemyStartingResources.Biologicals,
        EnemyStartingResources.Chemicals, EnemyStartingResources.ExoticMaterial, EnemyStartingResources.ResearchPoints);

    UE_LOG(LogTemp, Display, TEXT("UResourceManagerSubsystem initialized — configurable starting resources ready"));
}

void UResourceManagerSubsystem::SetHumanStartingResources(const FResourceStockpile& NewStart)
{
    HumanStartingResources = NewStart;
    if (FactionResources.Contains(EFactionType::Human))
        FactionResources[EFactionType::Human] = NewStart;
}

void UResourceManagerSubsystem::SetEnemyStartingResources(const FResourceStockpile& NewStart)
{
    EnemyStartingResources = NewStart;
    if (FactionResources.Contains(EFactionType::Enemy))
        FactionResources[EFactionType::Enemy] = NewStart;
}

FResourceStockpile UResourceManagerSubsystem::GetResources(EFactionType Faction) const
{
    return FactionResources.FindRef(Faction);
}

void UResourceManagerSubsystem::AddResources(EFactionType Faction, const FResourceStockpile& Amount)
{
    FResourceStockpile& Current = FactionResources.FindOrAdd(Faction);
    Current.Money += Amount.Money;    
    Current.Metals += Amount.Metals;
    Current.Biologicals += Amount.Biologicals;
    Current.Chemicals += Amount.Chemicals;
    Current.ExoticMaterial += Amount.ExoticMaterial;
    Current.ResearchPoints += Amount.ResearchPoints;
}

void UResourceManagerSubsystem::SetResources(EFactionType Faction, const FResourceStockpile& NewStock)
{
    FactionResources.FindOrAdd(Faction) = NewStock;
}

void UResourceManagerSubsystem::ApplyFacilityIncome(EFactionType Faction)
{
    UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    if (!BaseMgr) return;

    const TArray<UStrategyBase*>& Bases = BaseMgr->GetBases(Faction);

    int32 TotalMoney = 0;
    int32 TotalSupplies = 0;

    UE_LOG(LogTemp, Verbose, TEXT("[INCOME DEBUG] %s has %d bases"), *UEnum::GetValueAsString(Faction), Bases.Num());

    for (UStrategyBase* Base : Bases)
    {
        if (!Base) continue;

        for (UStrategyFacility* Fac : Base->Facilities)
        {
            if (!Fac || !Fac->bIsOperational) continue;

            if (!Fac->FacilityDefinition)
            {
                UE_LOG(LogTemp, Warning, TEXT("[INCOME] Facility in '%s' has no FacilityDefinition — skipping income"), *Base->BaseName.ToString());
                continue;
            }

            int32 MoneyIncome = Fac->FacilityDefinition->MoneyIncomePerDay;
            int32 SuppliesIncome = Fac->FacilityDefinition->SuppliesIncomePerDay;

            TotalMoney += MoneyIncome;
            TotalSupplies += SuppliesIncome;

            UE_LOG(LogTemp, Verbose, TEXT("[INCOME] %s in '%s' → +%d Money, +%d Supplies"),
                *Fac->FacilityDefinition->FacilityName.ToString(), *Base->BaseName.ToString(), MoneyIncome, SuppliesIncome);
        }
    }

    if (TotalMoney > 0 || TotalSupplies > 0)
    {
        FResourceStockpile Income{ TotalMoney, TotalSupplies, 0, 0 };
        AddResources(Faction, Income);

        UE_LOG(LogTemp, Display, TEXT("[INCOME] %s gained %d Money and %d Supplies from facilities"),
            *UEnum::GetValueAsString(Faction), TotalMoney, TotalSupplies);
    }
    else
    {
        UE_LOG(LogTemp, Verbose, TEXT("[INCOME] %s — no income generated this day"), *UEnum::GetValueAsString(Faction));
    }
}

void UResourceManagerSubsystem::PrintAllResources() const
{
    UE_LOG(LogTemp, Display, TEXT("=== RESOURCE MANAGER DEBUG ==="));
    for (auto& Pair : FactionResources)
    {
        FString FactionName = UEnum::GetValueAsString(Pair.Key);
        const FResourceStockpile& S = Pair.Value;
        UE_LOG(LogTemp, Display, TEXT("%s -> Money=%d | Metals=%d | Biologicals=%d | Chemicals=%d | Exotic=%d | Research=%d"),
            *FactionName, S.Money, S.Metals, S.Biologicals, S.Chemicals, S.ExoticMaterial, S.ResearchPoints);
    }
}

void UResourceManagerSubsystem::ResetResources(EFactionType Faction)
{
    FResourceStockpile& Stock = FactionResources.FindOrAdd(Faction);
    Stock = FResourceStockpile(); // zeros everything

    if (Faction == EFactionType::Enemy)
    {
        Stock.Money = 5000;
        Stock.Metals = 1000;
        Stock.Biologicals = 500;
		Stock.Chemicals = 200;
    }
}

bool UResourceManagerSubsystem::CanAfford(EFactionType Faction, const FResourceStockpile& Cost) const
{
    if (const FResourceStockpile* Current = FactionResources.Find(Faction))
    {
        return *Current >= Cost;
    }
    return false; // no resources = can't afford
}

bool UResourceManagerSubsystem::SubtractResources(EFactionType Faction, const FResourceStockpile& Cost)
{
    if (!CanAfford(Faction, Cost))
    {
        UE_LOG(LogTemp, Warning, TEXT("[RESOURCES] %s cannot afford the requested cost!"), *UEnum::GetValueAsString(Faction));
        return false;
    }

    FResourceStockpile& Current = FactionResources.FindOrAdd(Faction);
    Current.Subtract(Cost);

    UE_LOG(LogTemp, Display, TEXT("[RESOURCES] %s spent — Money:%d | Metals:%d | Biologicals:%d | Chemicals:%d"),
        *UEnum::GetValueAsString(Faction), Cost.Money, Cost.Metals, Cost.Biologicals, Cost.Chemicals);

    return true;
}
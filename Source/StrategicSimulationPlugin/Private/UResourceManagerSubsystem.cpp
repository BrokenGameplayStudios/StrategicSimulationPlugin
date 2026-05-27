#include "UResourceManagerSubsystem.h"
#include "UBaseManagerSubsystem.h"
#include "Engine/Engine.h"

void UResourceManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // === TIGHTER STARTING RESOURCES (much more strategic early game) ===
    // Human (player) - enough to build first base + 1-2 facilities, but must choose carefully
    FactionResources.Add(EFactionType::Human, FResourceStockpile{ 9500, 1800, 100, 50 });

    // Enemy (AI) - slightly weaker start so they don't snowball too fast
    FactionResources.Add(EFactionType::Enemy, FResourceStockpile{ 8500, 1400, 150, 30 });

    UE_LOG(LogTemp, Display, TEXT("UResourceManagerSubsystem initialized — both factions ready!"));
}

FResourceStockpile UResourceManagerSubsystem::GetResources(EFactionType Faction) const
{
    return FactionResources.FindRef(Faction);
}

void UResourceManagerSubsystem::AddResources(EFactionType Faction, const FResourceStockpile& Amount)
{
    FResourceStockpile& Current = FactionResources.FindOrAdd(Faction);
    Current.Money += Amount.Money;
    Current.Supplies += Amount.Supplies;
    Current.ExoticMaterial += Amount.ExoticMaterial;
    Current.ResearchPoints += Amount.ResearchPoints;
}

void UResourceManagerSubsystem::PrintAllResources() const
{
    UE_LOG(LogTemp, Display, TEXT("=== RESOURCE MANAGER DEBUG ==="));
    for (auto& Pair : FactionResources)
    {
        FString FactionName = UEnum::GetValueAsString(Pair.Key);
        UE_LOG(LogTemp, Display, TEXT("%s -> Money=%d | Supplies=%d | Exotic=%d | Research=%d"),
            *FactionName,
            Pair.Value.Money,
            Pair.Value.Supplies,
            Pair.Value.ExoticMaterial,
            Pair.Value.ResearchPoints);
    }
}

void UResourceManagerSubsystem::SetResources(EFactionType Faction, const FResourceStockpile& NewStock)
{
    FactionResources.FindOrAdd(Faction) = NewStock;
    UE_LOG(LogTemp, Display, TEXT("Resources set for %s"), *UEnum::GetValueAsString(Faction));
}

void UResourceManagerSubsystem::ApplyFacilityIncome(EFactionType Faction)
{
    UBaseManagerSubsystem* BaseMgr = GetGameInstance()->GetSubsystem<UBaseManagerSubsystem>();
    if (!BaseMgr) return;

    const TArray<UStrategyFacility*>& Facilities = BaseMgr->GetFacilities(Faction);

    int32 TotalMoney = 0;
    int32 TotalSupplies = 0;

    for (UStrategyFacility* Fac : Facilities)
    {
        if (Fac && Fac->bIsOperational && Fac->FacilityDefinition)
        {
            TotalMoney += Fac->FacilityDefinition->MoneyIncomePerDay;
            TotalSupplies += Fac->FacilityDefinition->SuppliesIncomePerDay;
        }
    }

    if (TotalMoney > 0 || TotalSupplies > 0)
    {
        FResourceStockpile Income;
        Income.Money = TotalMoney;
        Income.Supplies = TotalSupplies;

        AddResources(Faction, Income);

        UE_LOG(LogTemp, Display, TEXT("[INCOME] %s gained %d Money and %d Supplies from facilities"),
            *UEnum::GetValueAsString(Faction), TotalMoney, TotalSupplies);
    }
}

void UResourceManagerSubsystem::ResetResources(EFactionType Faction)
{
    if (Faction == EFactionType::Human)
    {
        FactionResources.Add(EFactionType::Human, FResourceStockpile{ 10000, 5000, 200, 100 });
    }
    else
    {
        FactionResources.Add(EFactionType::Enemy, FResourceStockpile{ 8000, 4000, 300, 50 });
    }

    UE_LOG(LogTemp, Display, TEXT("[RESET] Resources reset for %s to starting values"), *UEnum::GetValueAsString(Faction));
}
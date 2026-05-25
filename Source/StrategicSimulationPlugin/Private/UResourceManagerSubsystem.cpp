#include "UResourceManagerSubsystem.h"
#include "Engine/Engine.h"

void UResourceManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Initialize both factions with starting resources
    FactionResources.Add(EFactionType::Human, FResourceStockpile{ 10000, 5000, 200, 100 });
    FactionResources.Add(EFactionType::Enemy, FResourceStockpile{ 8000, 4000, 300, 50 });

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

void UResourceManagerSubsystem::TickResources(float DeltaTime)
{
    // Simple monthly-style income (we'll make this more realistic in Phase 5)
    FResourceStockpile HumanIncome{ 500, 300, 20, 50 };
    FResourceStockpile EnemyIncome{ 400, 250, 30, 30 };

    AddResources(EFactionType::Human, HumanIncome);
    AddResources(EFactionType::Enemy, EnemyIncome);

    UE_LOG(LogTemp, Display, TEXT("Resources ticked — income added to both factions"));
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
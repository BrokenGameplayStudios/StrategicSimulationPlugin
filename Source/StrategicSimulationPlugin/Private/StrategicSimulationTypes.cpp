#include "StrategicSimulationTypes.h"

/** Component-wise sum of all resource fields. */
FResourceStockpile FResourceStockpile::operator+(const FResourceStockpile& Other) const
{
    FResourceStockpile Result = *this;
    Result.Money += Other.Money;
    Result.ExoticMaterial += Other.ExoticMaterial;
    Result.ResearchPoints += Other.ResearchPoints;
    Result.Metals += Other.Metals;
    Result.Biologicals += Other.Biologicals;
    Result.Chemicals += Other.Chemicals;
    return Result;
}

/** Component-wise difference of all resource fields. */
FResourceStockpile FResourceStockpile::operator-(const FResourceStockpile& Other) const
{
    FResourceStockpile Result = *this;
    Result.Money -= Other.Money;
    Result.ExoticMaterial -= Other.ExoticMaterial;
    Result.ResearchPoints -= Other.ResearchPoints;
    Result.Metals -= Other.Metals;
    Result.Biologicals -= Other.Biologicals;
    Result.Chemicals -= Other.Chemicals;
    return Result;
}

/** True when this stockpile has at least as much of every resource type as Other. */
bool FResourceStockpile::operator>=(const FResourceStockpile& Other) const
{
    return Money >= Other.Money &&
        ExoticMaterial >= Other.ExoticMaterial &&
        ResearchPoints >= Other.ResearchPoints &&
        Metals >= Other.Metals &&
        Biologicals >= Other.Biologicals &&
        Chemicals >= Other.Chemicals;
}

/** Adds Other into this stockpile in place. */
void FResourceStockpile::Add(const FResourceStockpile& Other)
{
    *this = *this + Other;
}

/** Subtracts Other from this stockpile in place. */
void FResourceStockpile::Subtract(const FResourceStockpile& Other)
{
    *this = *this - Other;
}
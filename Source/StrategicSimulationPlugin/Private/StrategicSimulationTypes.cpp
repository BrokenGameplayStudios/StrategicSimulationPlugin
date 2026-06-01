#include "StrategicSimulationTypes.h"

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

bool FResourceStockpile::operator>=(const FResourceStockpile& Other) const
{
    return Money >= Other.Money &&
        ExoticMaterial >= Other.ExoticMaterial &&
        ResearchPoints >= Other.ResearchPoints &&
        Metals >= Other.Metals &&
        Biologicals >= Other.Biologicals &&
        Chemicals >= Other.Chemicals;
}

void FResourceStockpile::Add(const FResourceStockpile& Other)
{
    *this = *this + Other;
}

void FResourceStockpile::Subtract(const FResourceStockpile& Other)
{
    *this = *this - Other;
}
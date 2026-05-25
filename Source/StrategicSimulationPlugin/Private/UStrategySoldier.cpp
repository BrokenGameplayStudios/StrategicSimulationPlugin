#include "UStrategySoldier.h"
#include "Engine/Engine.h"

void UStrategySoldier::PrintInfo() const
{
    UE_LOG(LogTemp, Display, TEXT("  Soldier: %s (Rank %d)"), *SoldierName, Rank);
    if (ClassDefinition)
        UE_LOG(LogTemp, Display, TEXT("    Class: %s"), *ClassDefinition->ClassName.ToString());

    UE_LOG(LogTemp, Display, TEXT("    Stats -> Health:%d Aim:%d Defense:%d Will:%d Mobility:%d"),
        CurrentStats.Health, CurrentStats.Aim, CurrentStats.Defense, CurrentStats.Willpower, CurrentStats.Mobility);

    UE_LOG(LogTemp, Display, TEXT("    XP: %d | Wounded: %s"), XP, bIsWounded ? TEXT("YES") : TEXT("NO"));

    UE_LOG(LogTemp, Display, TEXT("    Equipment (%d items):"), CurrentLoadout.Num());
    for (const TSoftObjectPtr<UItemDefinition>& Item : CurrentLoadout)
    {
        if (UItemDefinition* Def = Item.Get())
        {
            UE_LOG(LogTemp, Display, TEXT("      - %s"), *Def->ItemName.ToString());
        }
        else
        {
            UE_LOG(LogTemp, Display, TEXT("      - (Pending Load)"));
        }
    }
    UE_LOG(LogTemp, Display, TEXT(""));
}
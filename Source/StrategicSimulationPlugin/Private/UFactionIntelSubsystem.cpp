#include "UFactionIntelSubsystem.h"
#include "UBaseManagerSubsystem.h"
#include "UStrategyCampaignSubsystem.h"
#include "Engine/Engine.h"

void UFactionIntelSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogTemp, Display, TEXT("UFactionIntelSubsystem initialized — per-faction site intel ready"));
}

bool UFactionIntelSubsystem::IsStaleIntelEnabled() const
{
    if (UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>())
    {
        return Campaign->bStaleIntelEnabled;
    }
    return true;
}

TMap<FGuid, FSiteIntelSnapshot>& UFactionIntelSubsystem::GetIntelMap(EFactionType Faction)
{
    return (Faction == EFactionType::Human) ? HumanIntelBySiteId : EnemyIntelBySiteId;
}

const TMap<FGuid, FSiteIntelSnapshot>& UFactionIntelSubsystem::GetIntelMap(EFactionType Faction) const
{
    return (Faction == EFactionType::Human) ? HumanIntelBySiteId : EnemyIntelBySiteId;
}

bool UFactionIntelSubsystem::CaptureGroundTruth(UStrategySiteDefinition* Site, FSiteIntelSnapshot& OutSnapshot)
{
    if (!Site)
    {
        return false;
    }

    OutSnapshot.SiteId = Site->SiteId;
    OutSnapshot.bLocationKnown = true;
    OutSnapshot.LastKnownLocation = Site->Location;
    OutSnapshot.LastKnownResources = Site->CurrentResources;
    OutSnapshot.bLastKnownHasBase = Site->bHasBeenUsed && Site->SiteType == EStrategySiteType::PotentialBase;
    return true;
}

void UFactionIntelSubsystem::ObserveSite(EFactionType Faction, UStrategySiteDefinition* Site, EDiscoveryReason Reason,
    float ObservedGameHours)
{
    if (!Site || !IsStaleIntelEnabled())
    {
        return;
    }

    if (Faction != EFactionType::Human && Faction != EFactionType::Enemy)
    {
        return;
    }

    FSiteIntelSnapshot Snapshot;
    if (!CaptureGroundTruth(Site, Snapshot))
    {
        return;
    }

    Snapshot.bHasFreshIntel = true;
    Snapshot.LastObservedGameHours = ObservedGameHours;
    Snapshot.LastReason = Reason;

    TMap<FGuid, FSiteIntelSnapshot>& IntelMap = GetIntelMap(Faction);
    IntelMap.Add(Site->SiteId, Snapshot);

    UE_LOG(LogTemp, Verbose, TEXT("[INTEL] %s observed %s at (%.0f, %.0f) via %s — Mt:%d base:%s fresh:YES"),
        *UEnum::GetValueAsString(Faction),
        *Site->SiteName,
        Site->Location.X, Site->Location.Y,
        *StaticEnum<EDiscoveryReason>()->GetNameStringByValue(static_cast<int64>(Reason)),
        Snapshot.LastKnownResources.Metals,
        Snapshot.bLastKnownHasBase ? TEXT("yes") : TEXT("no"));
}

bool UFactionIntelSubsystem::HasKnownSiteLocation(EFactionType Faction, const UStrategySiteDefinition* Site) const
{
    if (!Site)
    {
        return false;
    }

    if (!IsStaleIntelEnabled())
    {
        return true;
    }

    const FSiteIntelSnapshot* Snapshot = GetIntelMap(Faction).Find(Site->SiteId);
    return Snapshot && Snapshot->bLocationKnown;
}

bool UFactionIntelSubsystem::GetSiteIntelSnapshot(EFactionType Faction, const UStrategySiteDefinition* Site,
    FSiteIntelSnapshot& OutSnapshot) const
{
    if (!Site)
    {
        return false;
    }

    if (const FSiteIntelSnapshot* Found = GetIntelMap(Faction).Find(Site->SiteId))
    {
        OutSnapshot = *Found;
        return true;
    }

    return false;
}

FResourceStockpile UFactionIntelSubsystem::GetDisplayResources(EFactionType ViewerFaction,
    const UStrategySiteDefinition* Site) const
{
    if (!Site)
    {
        return FResourceStockpile();
    }

    if (!IsStaleIntelEnabled())
    {
        return Site->CurrentResources;
    }

    if (const FSiteIntelSnapshot* Snapshot = GetIntelMap(ViewerFaction).Find(Site->SiteId))
    {
        return Snapshot->LastKnownResources;
    }

    return Site->CurrentResources;
}

bool UFactionIntelSubsystem::GetDisplayHasBase(EFactionType ViewerFaction, const UStrategySiteDefinition* Site) const
{
    if (!Site)
    {
        return false;
    }

    if (!IsStaleIntelEnabled())
    {
        return Site->bHasBeenUsed && Site->SiteType == EStrategySiteType::PotentialBase;
    }

    if (const FSiteIntelSnapshot* Snapshot = GetIntelMap(ViewerFaction).Find(Site->SiteId))
    {
        return Snapshot->bLastKnownHasBase;
    }

    return Site->bHasBeenUsed && Site->SiteType == EStrategySiteType::PotentialBase;
}

bool UFactionIntelSubsystem::IsIntelFresh(EFactionType ViewerFaction, const UStrategySiteDefinition* Site) const
{
    if (!Site || !IsStaleIntelEnabled())
    {
        return true;
    }

    if (const FSiteIntelSnapshot* Snapshot = GetIntelMap(ViewerFaction).Find(Site->SiteId))
    {
        return Snapshot->bHasFreshIntel;
    }

    return false;
}

void UFactionIntelSubsystem::ClearFreshIntelFlags()
{
    if (!IsStaleIntelEnabled())
    {
        return;
    }

    for (TPair<FGuid, FSiteIntelSnapshot>& Pair : HumanIntelBySiteId)
    {
        Pair.Value.bHasFreshIntel = false;
    }

    for (TPair<FGuid, FSiteIntelSnapshot>& Pair : EnemyIntelBySiteId)
    {
        Pair.Value.bHasFreshIntel = false;
    }
}

void UFactionIntelSubsystem::ClearAllIntel()
{
    HumanIntelBySiteId.Empty();
    EnemyIntelBySiteId.Empty();
}

void UFactionIntelSubsystem::SeedIntelFromDiscoveredSites(UBaseManagerSubsystem* BaseManager)
{
    if (!BaseManager || !IsStaleIntelEnabled())
    {
        return;
    }

    auto SeedFaction = [&](EFactionType Faction, const TArray<UStrategySiteDefinition*>& Discovered)
    {
        for (UStrategySiteDefinition* Site : Discovered)
        {
            if (!Site)
            {
                continue;
            }

            FSiteIntelSnapshot Snapshot;
            if (!CaptureGroundTruth(Site, Snapshot))
            {
                continue;
            }

            Snapshot.bHasFreshIntel = false;
            Snapshot.LastObservedGameHours = 0.0f;
            Snapshot.LastReason = EDiscoveryReason::Radar;
            GetIntelMap(Faction).Add(Site->SiteId, Snapshot);
        }
    };

    SeedFaction(EFactionType::Human, BaseManager->DiscoveredSitesHuman);
    SeedFaction(EFactionType::Enemy, BaseManager->DiscoveredSitesEnemy);

    UE_LOG(LogTemp, Display, TEXT("[INTEL] Seeded snapshots from discovery lists — Human:%d Enemy:%d"),
        HumanIntelBySiteId.Num(), EnemyIntelBySiteId.Num());
}

TArray<FSiteIntelSnapshot> UFactionIntelSubsystem::SerializeIntel(EFactionType Faction) const
{
    TArray<FSiteIntelSnapshot> Result;
    Result.Reserve(GetIntelMap(Faction).Num());

    for (const TPair<FGuid, FSiteIntelSnapshot>& Pair : GetIntelMap(Faction))
    {
        FSiteIntelSnapshot Copy = Pair.Value;
        Copy.bHasFreshIntel = false;
        Result.Add(Copy);
    }

    return Result;
}

void UFactionIntelSubsystem::DeserializeIntel(EFactionType Faction, const TArray<FSiteIntelSnapshot>& SavedIntel,
    UBaseManagerSubsystem* BaseManager)
{
    TMap<FGuid, FSiteIntelSnapshot>& IntelMap = GetIntelMap(Faction);
    IntelMap.Empty();

    for (const FSiteIntelSnapshot& Snapshot : SavedIntel)
    {
        if (!Snapshot.SiteId.IsValid() || !Snapshot.bLocationKnown)
        {
            continue;
        }

        IntelMap.Add(Snapshot.SiteId, Snapshot);
    }

    if (BaseManager && IntelMap.Num() == 0)
    {
        const TArray<UStrategySiteDefinition*>& Discovered = (Faction == EFactionType::Human)
            ? BaseManager->DiscoveredSitesHuman
            : BaseManager->DiscoveredSitesEnemy;
        for (UStrategySiteDefinition* Site : Discovered)
        {
            if (!Site)
            {
                continue;
            }

            FSiteIntelSnapshot Snapshot;
            if (CaptureGroundTruth(Site, Snapshot))
            {
                Snapshot.bHasFreshIntel = false;
                IntelMap.Add(Site->SiteId, Snapshot);
            }
        }
    }
}
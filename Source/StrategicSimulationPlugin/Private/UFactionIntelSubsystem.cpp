#include "UFactionIntelSubsystem.h"
#include "UBaseManagerSubsystem.h"
#include "UStrategyCampaignSubsystem.h"
#include "Engine/Engine.h"

/** Subsystem startup — logs readiness; intel maps start empty. */
void UFactionIntelSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogTemp, Display, TEXT("UFactionIntelSubsystem initialized — per-faction site intel ready"));
}

/** Reads bStaleIntelEnabled from the campaign subsystem (defaults to true if missing). */
bool UFactionIntelSubsystem::IsStaleIntelEnabled() const
{
    if (UStrategyCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>())
    {
        return Campaign->bStaleIntelEnabled;
    }
    return true;
}

/** Selects HumanIntelBySiteId or EnemyIntelBySiteId for read/write access. */
TMap<FGuid, FSiteIntelSnapshot>& UFactionIntelSubsystem::GetIntelMap(EFactionType Faction)
{
    return (Faction == EFactionType::Human) ? HumanIntelBySiteId : EnemyIntelBySiteId;
}

/** Const variant of GetIntelMap for query-only code paths. */
const TMap<FGuid, FSiteIntelSnapshot>& UFactionIntelSubsystem::GetIntelMap(EFactionType Faction) const
{
    return (Faction == EFactionType::Human) ? HumanIntelBySiteId : EnemyIntelBySiteId;
}

/** Snapshots SiteId, location, CurrentResources, and whether a base has been built on a PotentialBase site. */
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

/** Writes or replaces the faction's snapshot for Site with fresh observation metadata. */
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

/** UI/save-facing resource view: last-known stockpile per viewer faction under stale intel rules. */
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

/** UI-facing base-built flag: last-known bLastKnownHasBase or live site state. */
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

/** True when the site was observed this step; always true if stale intel is off or no snapshot exists. */
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

/** End-of-step hook: every snapshot's bHasFreshIntel is set false until the next ObserveSite. */
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

/** Empties both faction intel maps (campaign restart / debug). */
void UFactionIntelSubsystem::ClearAllIntel()
{
    HumanIntelBySiteId.Empty();
    EnemyIntelBySiteId.Empty();
}

/** Builds non-fresh snapshots from DiscoveredSitesHuman/Enemy so UI has last-known data after load. */
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

/** Exports all snapshots for one faction; bHasFreshIntel is forced false for persistence. */
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

/** Imports saved snapshots; if empty, seeds from the faction's discovered-site list via BaseManager. */
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
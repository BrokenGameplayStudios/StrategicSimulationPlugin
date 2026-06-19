# Engineering Archive

Internal design documents and PR implementation notes. **Gameplay setup, data authoring, and tuning are in the [user wiki](../README.md).**

## Current implementation (June 2026)

Shipped in plugin source — see user docs for usage:

| System | User doc | Status |
|--------|----------|--------|
| Salvage wrecks, fog, contested hook | [salvage.md](../salvage.md) | Shipped (PR-1–7, 6b) |
| Stale intel, LOS, base radar, patrol/intercept | [radar-and-intel.md](../radar-and-intel.md) | Shipped (PR-8–14) |
| Base expansion (vehicle guard) | [missions-and-ai.md](../missions-and-ai.md) | Shipped (breaking change) |
| Vehicle crew | [missions-and-ai.md](../missions-and-ai.md) | Shipped |
| Data asset categories | [data-assets.md](../data-assets.md) | Shipped |
| API cleanup (Tier A/B) | [README.md § Removed APIs](../README.md) | Shipped |

**Not in source / stubs:** tactical map load, full Continue save, combat POW tuning, research unlock gating, strategic base damage.

**Removed since original specs:** `AddDiscoveredSiteAtLocation`, `TryBuildBaseOnSite`, `LaunchMissionFromBase`, `UProductionManagerSubsystem`, `PerformDailyBuildOrder`, `GetFormattedDate`, `DiscoveringFaction` field, several POW/containment daily APIs.

## Documents in this folder

| Document | Role |
|----------|------|
| [design-salvage-sites-summary.md](./design-salvage-sites-summary.md) | **Current** salvage implementation summary |
| [design-radar-intel-patrol-summary.md](./design-radar-intel-patrol-summary.md) | **Current** radar/intel implementation summary |
| [design-salvage-sites.md](./design-salvage-sites.md) | Full PR-1–7 design spec (archive; see status banner) |
| [design-radar-intel-patrol.md](./design-radar-intel-patrol.md) | Full PR-8–14 design spec (archive) |
| [design-salvage-sites-review.md](./design-salvage-sites-review.md) | Design review log (archive) |

Read the **summary** files first. Full specs retain PR-level detail for archaeology; superseded API names are called out in their status banners.

## Source entry points

| Subsystem | Header |
|-----------|--------|
| Campaign lifecycle | `UStrategyCampaignSubsystem.h` |
| Level bootstrap | `AStrategyGameInitializer.h` |
| Time / pause | `UTimeManagerSubsystem.h` |
| Bases & sites | `UBaseManagerSubsystem.h` |
| Missions & vehicles | `UMissionManagerSubsystem.h` |
| AI | `UAIControllerSubsystem.h` |
| Radar contacts | `URadarContactSubsystem.h` |
| Intel snapshots | `UFactionIntelSubsystem.h` |
| Exploration / patrol | `UExplorationSubsystem.h` |
| Events (UI) | `UStrategyEventDispatcher.h` |
| Definitions | `U*Definition.h`, `*Database.h` |
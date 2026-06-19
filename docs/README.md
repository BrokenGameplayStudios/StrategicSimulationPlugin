# Strategic Simulation — Documentation Wiki

Guides for using the plugin in your project: level setup, data authoring, tuning, gameplay systems, UI wiring, and debug tools. Most workflows need Blueprint only.

| I want to… | Read |
|------------|------|
| Set up a level and run the first simulation | [Getting started](getting-started.md) |
| Author items, facilities, soldiers, vehicles, research | [Data assets](data-assets.md) |
| Tune map, AI, salvage, radar, and resources | [Tuning guide](tuning.md) |
| Control pause, time scale, and the simulation clock | [Time & simulation](time-and-simulation.md) |
| Understand missions, combat, crew, and daily AI | [Missions & AI](missions-and-ai.md) |
| Work with wrecks and salvage | [Salvage](salvage.md) |
| Use radar, contacts, and intel | [Radar & intel](radar-and-intel.md) |
| Wire UI and Blueprint events | [UI & events](ui-and-events.md) |
| Use debug HUD and QA save/load | [Debug tools](debug-tools.md) |
| Look up terms | [Glossary](glossary.md) |
| See what changed | [Changelog](../CHANGELOG.md) |

## What the plugin does today

- Two factions (Human + Enemy) with per-faction AI toggles
- Bases, facilities, soldiers, research, vehicles, and faction resources
- **Live** vehicle missions on a logical 2D map (no abstract day-countdown missions)
- Vehicle **crew** requirements (min 1 soldier, max-fill from origin base)
- **Base expansion** via vehicle-guarded Command Center construction (not instant build)
- Combat between armed vehicles; wrecks become salvage sites
- Command Center passive radar, threat contacts, spectate-friendly radar overlay
- Border patrol, reactive interception, and optional player/designer intercept
- Fog-of-war salvage map and stale intel tooltips
- Facility-based production queues (soldiers, vehicles, items, research, construction)
- Site-map save/load for QA (not a full campaign Continue)

## Not available yet

- Strategic damage when fighters reach enemy bases (arrival is logged only)
- Tactical map / player PvE battle load
- Full campaign save (soldiers, vehicles, bases, missions across sessions)
- Research unlock gating (`HasCompletedResearch` / `IsItemUnlocked` are stubs)
- Combat POW/KIA tuning props (defined on campaign but not applied in combat code)
- Per-base containment/autopsy daily processing (facilities exist; only wreck KIA/MIA path is live)

## Removed APIs (do not use)

These were removed in the Tier A/B cleanup. Use the replacements below.

| Removed | Use instead |
|---------|-------------|
| `AddDiscoveredSiteAtLocation` | `AddDiscoveredSite(Faction, Site, Reason)` |
| `TryBuildBaseOnSite` | `StartBaseExpansion` + vehicle guard flow |
| `LaunchMissionFromBase` | `UMissionManagerSubsystem::StartMission` |
| `PerformDailyBuildOrder` | Daily AI runs automatically; `Debug_RunAI` for manual tick |
| `GetFormattedDate` (campaign) | `UTimeManagerSubsystem::GetFormattedDateString` |
| `UProductionManagerSubsystem` / `TryProduce` | `UStrategyFacility::StartProduction` |
| `OnSoldierDismissed`, `OnItemProduced`, `OnMonthlyEvent` | Not emitted; use `OnProductionCompleted`, roster events |

## Engineering docs

Internal design specs and PR history live in [docs/dev/](dev/README.md) — for contributors, not gameplay setup.
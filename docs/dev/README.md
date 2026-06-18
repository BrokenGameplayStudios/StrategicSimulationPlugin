# Engineering Archive

This folder contains internal design documents and PR implementation notes. **Gameplay setup and tuning are documented in the [user wiki](../README.md).**

## Contents

| Document | Topic |
|----------|--------|
| [design-salvage-sites.md](./design-salvage-sites.md) | Full salvage system design spec |
| [design-salvage-sites-summary.md](./design-salvage-sites-summary.md) | Salvage executive summary |
| [design-salvage-sites-review.md](./design-salvage-sites-review.md) | Design review log (archive) |
| [design-radar-intel-patrol.md](./design-radar-intel-patrol.md) | Radar, intel, patrol design spec |
| [design-radar-intel-patrol-summary.md](./design-radar-intel-patrol-summary.md) | Radar/intel executive summary |

## Source entry points

| Subsystem | Header |
|-----------|--------|
| Campaign lifecycle | `UStrategyCampaignSubsystem.h` |
| Time / pause | `UTimeManagerSubsystem.h` |
| Bases & sites | `UBaseManagerSubsystem.h` |
| Missions & vehicles | `UMissionManagerSubsystem.h` |
| AI | `UAIControllerSubsystem.h` |
| Radar contacts | `URadarContactSubsystem.h` |
| Intel snapshots | `UFactionIntelSubsystem.h` |
| Exploration / patrol | `UExplorationSubsystem.h` |
| Events (UI) | `UStrategyEventDispatcher.h` |
# Strategic Simulation — Documentation Wiki

Guides for using the plugin in your project: level setup, tuning, gameplay systems, UI wiring, and debug tools. No C++ required for most workflows.

| I want to… | Read |
|------------|------|
| Set up a level and run the first simulation | [Getting started](getting-started.md) |
| Tune map, AI, salvage, radar, and resources | [Tuning guide](tuning.md) |
| Control pause, time scale, and the simulation clock | [Time & simulation](time-and-simulation.md) |
| Understand missions, combat, and daily AI | [Missions & AI](missions-and-ai.md) |
| Work with wrecks and salvage | [Salvage](salvage.md) |
| Use radar, contacts, and intel | [Radar & intel](radar-and-intel.md) |
| Wire UI and Blueprint events | [UI & events](ui-and-events.md) |
| Use debug HUD and QA save/load | [Debug tools](debug-tools.md) |
| Look up terms | [Glossary](glossary.md) |
| See what changed | [Changelog](../CHANGELOG.md) |

## What the plugin does today

- Two factions (Human + Enemy) with configurable AI
- Bases, facilities, soldiers, research, vehicles, and faction resources
- Live vehicle missions on a logical map with radar discovery
- Combat between armed vehicles; wrecks become salvage sites
- Command Center passive radar, threat contacts, spectate-friendly radar overlay, and optional player/designer intercept
- Vehicle crew requirements (min 1 soldier, max-fill from origin base)
- Border patrol and AI interception toward inbound threats
- Fog-of-war salvage map and stale intel tooltips
- Site-map save/load for QA (not a full campaign Continue)

## Not available yet

- Strategic damage when fighters reach enemy bases (arrival is logged only)
- Tactical map / player PvE battle load
- Full campaign save (soldiers, vehicles, bases across sessions)
- Research unlock gating (stub always allows)
- Soldier casualties on abstract mission resolution (wreck KIA/MIA on vehicle destruction is implemented)

## Engineering docs

Internal design specs live in [docs/dev/](dev/README.md) — for contributors, not gameplay setup.
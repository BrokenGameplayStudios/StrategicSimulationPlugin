# Strategic Simulation Plugin

Unreal Engine 5 strategic layer plugin (XCOM-style geoscape): factions build bases, run economy and research, and fly live missions across a 2D logical map.

**Broken Gameplay Studios** · Requires **CommonUI** · UE 5.7+

## Quick start

1. Enable this plugin and **CommonUI** in your project.
2. Place **`AStrategyGameInitializer`** in your level and assign all six database assets (see [Getting started](docs/getting-started.md)).
3. From your HUD Blueprint, call **`UStrategyCampaignSubsystem::StartSimulation()`**, then unpause time with **`UTimeManagerSubsystem::TogglePause()`** or **`StartSimulation()`**.

Optional: place **`AStrategyTestActor`** to auto-spawn `WBP_StrategicHUD` and map overlays on play.

## Documentation

| Resource | Description |
|----------|-------------|
| **[Documentation Wiki](docs/README.md)** | Main index — setup, tuning, gameplay systems, UI, debug |
| **[Changelog](CHANGELOG.md)** | Version history |

## Repository layout

```
StrategicSimulationPlugin/
├── Content/Data/          # Database assets and definitions
├── Content/UI/            # Sample widgets (HUD, time control, roster)
├── docs/                  # User wiki
├── docs/dev/              # Engineering archive (not for gameplay setup)
└── Source/                # C++ module
```

## Requirements

- Unreal Engine 5.7 (tested)
- [CommonUI](https://docs.unrealengine.com/) plugin enabled in host project

---

*Strategic Simulation Plugin — [Documentation Wiki](docs/README.md)*
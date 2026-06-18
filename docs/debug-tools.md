# Debug tools

## Debug map HUD

Set **AStrategyDebugHUD** as the level GameMode HUD class.

| Console command | Effect |
|-----------------|--------|
| `ToggleDebugHUD` | Master debug visibility |
| `ToggleStrategyMap` | Logical map overlay |
| `ShowSiteInfo 0` | Inspect site index 0 |
| `ClearSiteInfo` | Clear inspector |

HUD properties:

- `bShowStrategyMap` — sites, bases, vehicles, paths
- `bShowVehiclePaths` — mission waypoint lines
- `bShowFriendlyRadarContacts` / `bShowEnemyRadarContacts` — entry-point diamonds

Requires **`bAllowDebugExecCommands`** on campaign (set via initializer).

## Output log filters

Filter **LogTemp** in the Output Log:

| Tag | Content |
|-----|---------|
| `[AI]` / `[AI TICK]` | Daily AI decisions |
| `[MISSION]` / `[MISSION TARGET]` | Missions and targets |
| `[LIVE MISSION]` | Vehicle movement |
| `[BASE RADAR]` / `[INTERCEPT]` / `[PATROL]` | Radar and defense |
| `[COMBAT]` / `[SALVAGE]` | Combat and wrecks |
| `[DISCOVERY]` | Site discovery |
| `[CAMPAIGN]` | Day rollover |

## QA save / load

```
SaveCampaign(SlotIndex)   — writes site map + intel
LoadCampaign(SlotIndex)   — restores sites only
GetAllSaveMetadata        — list slots
```

After load there are **no bases** — simulation is not runnable until a fresh `StartSimulation` in a new session.

## Manual AI

`Debug_RunAI` on campaign or `UAIControllerSubsystem` — runs daily AI immediately for enabled factions.
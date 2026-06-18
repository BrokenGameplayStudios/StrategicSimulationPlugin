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
- `bShowFriendlyRadarContacts` / `bShowEnemyRadarContacts` — entry-point diamonds (cyan Human, magenta Enemy)

Requires **`bAllowDebugExecCommands`** on campaign (set via initializer).

### Strategy map overlay (when `bShowStrategyMap` is on)

| Element | Appearance |
|---------|------------|
| Human / Enemy bases | Blue / red boxes; **orange** while CC is under construction (`CC: Nd` label) |
| Potential base nodes | Gray when empty; **faction color** when a base exists or is building |
| Site index numbers | Centered above the node (outside the inspector yellow ring) |
| Inspector highlight | Yellow ring on `ShowSiteInfo N` selection |
| Vehicle radar rings | 40% opacity |
| Discovery squares | Blue / red at 80% opacity |
| Radar LOS blockers | **Brown** outlines |
| Passive radar contacts | Cyan diamonds (Human), magenta diamonds (Enemy) |

### On-screen debug text (`ToggleDebugHUD`)

Includes a **CC Under Construction** section when forward bases are building, e.g.:

`Enemy 'Forward Base 02': 4 day(s) left, on site 21...`

### Site inspector (`ShowSiteInfo N`)

Bottom panel shows site type, resources, and base details. When a Command Center is still building:

- **Status:** `Under Construction — N day(s) left`
- **Base section:** same under-construction line above facilities/extraction

## Output log filters

Filter **LogTemp** in the Output Log:

| Tag | Content |
|-----|---------|
| `[AI]` / `[AI TICK]` | Daily AI decisions |
| `[MISSION]` / `[MISSION TARGET]` | Missions and targets |
| `[LIVE MISSION]` | Vehicle movement |
| `[BASE RADAR]` / `[INTERCEPT]` / `[PATROL]` | Radar and defense |
| `[COMBAT]` / `[SALVAGE]` | Combat and wrecks |
| `[CREW]` | Soldier assignment before mission launch |
| `[CC BUILD]` | Daily Command Center construction progress |
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
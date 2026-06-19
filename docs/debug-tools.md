# Debug tools

## Debug map HUD

Set **AStrategyDebugHUD** as the level GameMode HUD class.

| Console command | Effect |
|-----------------|--------|
| `ToggleDebugHUD` | Master debug visibility |
| `ToggleStrategyMap` | Logical map overlay |
| `ShowSiteInfo 0` | Inspect site index 0 |
| `ClearSiteInfo` | Clear inspector |

Requires **`bAllowDebugExecCommands`** on campaign (from initializer).

### HUD properties

| Property | Effect |
|----------|--------|
| `bShowStrategyMap` | Sites, bases, vehicles, paths |
| `bShowVehiclePaths` | Mission waypoint lines |
| `bShowFriendlyRadarContacts` | Cyan entry-point diamonds |
| `bShowEnemyRadarContacts` | Magenta entry-point diamonds |

### Strategy map overlay

| Element | Appearance |
|---------|------------|
| Human / Enemy bases | Blue / red boxes; **orange** while CC building (`CC: Nd`) |
| Potential base nodes | Gray empty; **faction color** when base exists or building |
| Site index numbers | Centered above node (outside inspector ring) |
| Inspector selection | Yellow ring on `ShowSiteInfo N` |
| Vehicle radar rings | 40% opacity |
| Discovery squares | Blue / red at 80% opacity |
| Radar LOS blockers | **Brown** outlines |
| Passive contacts | Cyan (Human), magenta (Enemy) diamonds |

### On-screen text (`ToggleDebugHUD`)

Includes **CC Under Construction** when forward bases are building, e.g.:

`Enemy 'Forward Base 02': 4 day(s) left, on site 21...`

### Site inspector (`ShowSiteInfo N`)

Bottom panel: site type, resources, salvage MIA/KIA, base facilities. When CC is building:

- **Status:** `Under Construction — N day(s) left`
- Base section shows the same line above extraction stats

Uses **`GetSiteStatusDisplayText(Site, BaseManager)`**.

## Test actor

**AStrategyTestActor** spawns sample HUD + map overlays and binds test handlers for soldier recruit, research complete, site discovered, and salvage contest events.

## Output log filters

Filter **LogTemp**:

| Tag | Content |
|-----|---------|
| `[AI]` / `[AI TICK]` | Daily AI |
| `[MISSION]` / `[LIVE MISSION]` | Missions and movement |
| `[BASE RADAR]` / `[INTERCEPT]` / `[PATROL]` | Radar and defense |
| `[COMBAT]` / `[SALVAGE]` | Combat and wrecks |
| `[CREW]` | Soldier assignment before launch |
| `[CC BUILD]` | Command Center construction |
| `[DISCOVERY]` | Site discovery |
| `[CAMPAIGN]` | Day rollover |
| `[SAVE]` | QA save/load |
| `[SALVAGE CONTEST]` | Contested salvage |
| `[UNLOCK]` | Research/facility unlocks (if `Show Unlock Messages`) |

## QA save / load

| API | Effect |
|-----|--------|
| `SaveCampaign(SlotIndex)` | Writes site map + intel (if `bSitesPersistenceEnabled`) |
| `LoadCampaign(SlotIndex)` | Restores sites and intel snapshots |
| `GetAllSaveMetadata()` | Scan slots 1–10 |

**Schema:** v2 = sites; v3 = + per-faction intel (`StrategyIntelSaveSchemaVersion`).

**Not persisted:** bases, facilities, vehicles, soldiers, missions, rosters.

Load clears mission state and may reset bases — **not runnable** until fresh `StartSimulation` in a new session.

## Manual AI

`Debug_RunAI` on campaign or **`UAIControllerSubsystem`** — runs daily AI immediately for enabled factions.

## Base debug strings

`UBaseManagerSubsystem::DebugPrintFullBaseState`, `GetBaseStateDebugString` — facility/soldier/vehicle counts per base.
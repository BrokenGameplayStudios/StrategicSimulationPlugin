# Radar & intel

## Vehicle radar

Vehicles ping on an interval while on mission. Within range and line of sight they can:

- Discover undiscovered **sites** (potential bases, resource nodes)
- Detect **enemy vehicles** (parked or in flight)
- Scan **enemy bases** along the flight path

Blocked pings respect **`bRadarLOSEnabled`** and mountain **blocker zones** on the initializer.

## Command Center passive radar

Operational Command Centers sweep on **`BaseRadarPingIntervalHours`** within **`BaseRadarRangePixels`** without sending a vehicle.

Creates **radar contacts** for enemy vehicles in range:

- **Entry point** — where the threat first entered range (used for intercept and patrol)
- **Heading and speed** — estimated from movement
- **Inbound flag** — vehicle moving toward friendly territory
- **Threatened base** — nearest friendly base at risk

Contacts expire after **`RadarContactExpiryHours`** without refresh.

## Player map overlay

**UStrategyRadarContactMapWidget** draws contact diamonds on the HUD:

- Hover for tooltip (faction, base, entry point, heading, speed, staleness)
- Toasts on new contacts for the widget's `ViewerFaction`
- **`bShowOpposingFactionContacts`** (default on) — also draws the other faction's contacts (cyan = Human, magenta = Enemy)

Requires **`bBasePassiveRadarEnabled`**. Spawned by test actor at z-order 11 or embed in your HUD.

### AI vs AI / spectate mode

When **`UAIControllerSubsystem`** is simulating a faction (`IsSimulatingHumanAI` / `IsSimulatingEnemyAI`):

- **Left-click intercept is off** by default (`bAllowPlayerClickToIntercept = false`)
- Tooltips show *"AI handles interception when enabled"* instead of *"Click to launch interception"*
- **Reactive AI** still dispatches gunships via `URadarContactSubsystem` when `bAIReactiveInterceptionEnabled` is on

Set **`bAllowPlayerClickToIntercept = true`** on the widget to restore click-to-intercept during AI runs (testing only).

### Manual intercept (designer UI)

Wire a button instead of map click:

| Blueprint call | Purpose |
|----------------|---------|
| `GetHoveredContactId` | Contact under cursor |
| `GetHoveredContactFaction` | Human or Enemy owner of that contact |
| `GetHoveredTooltipText` | Same text as the hover panel |
| `IsClickToInterceptAllowed` | Hide/disable button in spectate mode |
| `TryInterceptContactById` | Dispatch for `ViewerFaction` |
| `TryInterceptContactByIdForFaction` | Dispatch for either faction's hovered contact |

Backend: `UMissionManagerSubsystem::TryLaunchInterceptionAtContactAuto` (requires crew — see [Missions & AI](missions-and-ai.md)).

## AI response

- **Reactive interception** — idle gunships launch on inbound contacts when AI reactive mode is on
- **Defensive patrol** — vehicles patrol toward entry lanes when threats exist
- **Hot spokes** — recon patrols prefer directions where threats were recently detected

## Enemy detection alert

When enemy radar picks up **your** inbound vehicle, **`OnOpposingFactionRadarAlert`** fires (if `bNotifyPlayerOfEnemyRadarContacts`). Radar map widget shows an intel toast.

Debug HUD can draw **magenta diamonds** for enemy-side contacts when `bShowEnemyRadarContactsOnDebugMap` is on.

## Stale intel

**UFactionIntelSubsystem** stores per-faction snapshots of site resources and base-built state. UI reads **last-known** values when intel is stale (`bStaleIntelEnabled`). Fresh intel updates on radar ping, discovery, or on-station observation.

Salvage tooltips append *"Intel stale"* when appropriate.

## Line of sight

Configure **`RadarBlockerZones`** on the initializer (circles or rectangles). Debug HUD draws blockers in **brown**.
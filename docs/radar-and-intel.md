# Radar & intel

## Vehicle radar

Vehicles ping on **`PingIntervalHours`** while on mission. Within range and line of sight they can:

- Discover undiscovered **sites** → `AddDiscoveredSite` + `OnSiteDiscovered`
- Detect **enemy vehicles** (parked or in flight)
- Scan **enemy bases** along the flight path

Respects **`bRadarLOSEnabled`** and **`RadarBlockerZones`** on the initializer (mountain blockers).

## Command Center passive radar

Operational Command Centers sweep every **`BaseRadarPingIntervalHours`** within **`BaseRadarRangePixels`**.

Creates **radar contacts** (`FRadarContact`) for enemy vehicles:

| Field | Use |
|-------|-----|
| First-detected position | On the **passive radar ring** (backtracked along flight path) — intercept/patrol target |
| Last position / velocity | Track updates |
| `bIsInboundThreat` | Moving toward friendly territory |
| `ThreatenedBaseName` | Nearest friendly base at risk |

Contacts expire after **`RadarContactExpiryHours`** without refresh.

Requires **`bBasePassiveRadarEnabled`**.

## Player map overlay

**UStrategyRadarContactMapWidget** draws contact diamonds:

- Hover tooltip via **`FormatRadarContactTooltipText`**
- Toasts on new contacts for widget **`ViewerFaction`**
- **`bShowOpposingFactionContacts`** (default on) — cyan = Human, magenta = Enemy

Spawned by test actor at z-order 11 or embed in your HUD.

### AI vs AI / spectate mode

When **`UAIControllerSubsystem`** is simulating a faction:

- **`bAllowPlayerClickToIntercept`** defaults **false**
- Tooltips say *"AI handles interception when enabled"*
- **`TryReactiveInterception`** still dispatches AI gunships when enabled

Set **`bAllowPlayerClickToIntercept = true`** on the widget for click-to-intercept during AI runs (testing).

### Manual intercept (designer UI)

| Blueprint call | Purpose |
|----------------|---------|
| `GetHoveredContactId` | Contact under cursor |
| `GetHoveredContactFaction` | Human or Enemy owner |
| `GetHoveredTooltipText` | Hover panel text |
| `IsClickToInterceptAllowed` | Hide button in spectate mode |
| `TryInterceptContactById` | Dispatch for `ViewerFaction` |
| `TryInterceptContactByIdForFaction` | Dispatch for either faction |

Backend: **`TryLaunchInterceptionAtContactAuto`** (requires crew — see [Missions & AI](missions-and-ai.md)).

## AI response

- **Reactive interception** — idle gunships on inbound contacts (`bAIReactiveInterceptionEnabled`)
- **Defensive patrol** — toward radar entry lanes when threats exist
- **Hot spokes** — recon prefers directions of recent inbound contacts (`UExplorationSubsystem`)

## Enemy detection alert

Enemy radar on **your** inbound vehicle fires **`OnOpposingFactionRadarAlert`** when `bNotifyPlayerOfEnemyRadarContacts` is on.

Debug HUD draws magenta contact diamonds when **`bShowEnemyRadarContactsOnDebugMap`** is on.

## Stale intel

**UFactionIntelSubsystem** stores per-faction **`FSiteIntelSnapshot`** (resources, base-built state, location known).

| API | Use |
|-----|-----|
| `GetDisplayResources` | UI resource read (stale-aware) |
| `GetDisplayHasBase` | Whether viewer believes a base exists |
| `IsIntelFresh` | Refreshed this simulation step |
| `ObserveSite` | Called on discovery / ping / visit |

Gated by **`bStaleIntelEnabled`**. When off, UI reads live site data.

**`HasKnownSiteLocation` and `GetSiteIntelSnapshot` were removed** from the public API; use display helpers above.

## Line of sight

**`RadarBlockerZones`** on initializer (circles or rectangles) → **`URadarTerrainSubsystem`**. Debug HUD draws blockers in **brown**.
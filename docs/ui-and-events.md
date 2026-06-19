# UI & events

## Base widget classes

| Class | Use |
|-------|-----|
| `UStrategyUserWidget` | CommonUI base for strategic HUD panels |
| `UStrategyActivatableWidget` | Menus and modal screens |
| `UStrategySalvageMapWidget` | Fog-aware wreck overlay |
| `UStrategyRadarContactMapWidget` | Radar contacts, hover intel, optional intercept |
| `UStrategicSimulationDisplayHelpers` | Blueprint library for markers and tooltips |

## Sample content widgets

Under `Content/UI/`:

- `WBP_StrategicHUD` — call `StartSimulation` from here
- `WBP_TimeControl` — pause and time scale
- `WBP_ResourcePanel`, `WBP_RosterScreen` — resources and soldiers
- `WBP_Notification` / `WBP_NotificationStack` — toasts

## Event dispatcher

Subscribe in Widget Blueprints to **UStrategyEventDispatcher** (game instance subsystem).

### Soldiers & production

| Delegate | Params | Typical UI use |
|----------|--------|----------------|
| `OnSoldierRecruited` | Faction, Soldier | Roster toast |
| `OnSoldierListChanged` | Faction, Soldiers[] | Refresh roster panel |
| `OnSoldierLoadoutChanged` | Faction, Soldier | Gear display |
| `OnResearchCompleted` | Faction, Tech | Tech unlocked toast |
| `OnVehicleCompleted` | Faction, Vehicle | Hangar notification |
| `OnFacilityCompleted` | Faction, Facility | Build complete |
| `OnProductionCompleted` | Faction, Item | Workshop item done |

**Removed (never emitted):** `OnSoldierDismissed`, `OnItemProduced`, `OnMonthlyEvent`.

### Sites & salvage

| Delegate | Params | Typical UI use |
|----------|--------|----------------|
| `OnSiteDiscovered` | Faction, Site, Reason | Map pin |
| `OnSalvageSiteCreated` | WreckOwner, KnownFactions[], Site | Wreck marker |
| `OnSalvageSiteRemoved` | SiteId, LastSalvagingFaction | Remove marker |
| `OnSalvageContestStarted` | Site, HumanSnapshot, EnemySnapshot | Launch tactical fight |

### Radar

| Delegate | Params | Typical UI use |
|----------|--------|----------------|
| `OnRadarContactUpdated` | Faction, Contact | Threat layer refresh |
| `OnRadarContactExpired` | Faction, Contact | Remove blip |
| `OnOpposingFactionRadarAlert` | Contact, AlertMessage | Enemy spotted you |

### Base expansion

| Delegate | Params | Typical UI use |
|----------|--------|----------------|
| `OnBaseExpansionOrdered` | Faction, Site, Vehicle | Expansion dispatched |
| `OnBaseExpansionClaimed` | Faction, Site, Base | Site claimed, CC building |
| `OnBaseExpansionCancelled` | Faction, Site | Guard lost — site reopened |
| `OnBaseExpansionGuardComplete` | Faction, Base, Vehicle | CC operational |

### Other delegates (not on event dispatcher)

| Delegate | Owner | Use |
|----------|-------|-----|
| `OnMissionCompleted` | `UMissionManagerSubsystem` | Mission wrap-up UI |
| `OnResearchListChanged` | `UResearchManagerSubsystem` | Lab queue UI |
| `OnFacilitiesChanged` | `UStrategyBase` | Base facility panel |
| `OnSimulationClockStateChanged` | `UTimeManagerSubsystem` | Pause/speed buttons |
| `OnDayPassed` | `UTimeManagerSubsystem` | Day counter |

## Time UI

Bind **`OnSimulationClockStateChanged`** on the time manager. Use **`IsSimulationClockHalted`** for a single paused indicator.

Date label: **`UTimeManagerSubsystem::GetFormattedDateString`** (not campaign `GetFormattedDate`).

## Display helpers

| Function | Use |
|----------|-----|
| `BuildSalvageMapMarkers` | Fog-aware wreck markers |
| `BuildRadarContactMapMarkers` | Contact diamonds (`bIncludeOpposingFactionContacts`, `bAllowClickDispatch`) |
| `GetSiteStatusDisplayText` | Inspector status including under-construction |
| `GetSiteStatusDisplayText(Site, BaseManager)` | Same with live base manager |
| `FormatSalvageTooltipText` | Wreck hover (stale intel) |
| `FormatRadarContactTooltipText` | Contact hover |

## Blueprint cheat sheet

| Action | Call |
|--------|------|
| Start campaign | `UStrategyCampaignSubsystem::StartSimulation` |
| Unpause | `UTimeManagerSubsystem::TogglePause` |
| Get managers | `GetResourceManager`, `GetBaseManager`, `GetMissionManager`, … on Campaign |
| Launch mission | `UMissionManagerSubsystem::StartMission` |
| Order expansion | `UBaseManagerSubsystem::StartBaseExpansion` |
| Intercept (player) | `TryLaunchInterceptionAtContactAuto` or radar widget |
| Intercept (HUD button) | `GetHoveredContactId` + `TryInterceptContactByIdForFaction` |
| Resolve salvage fight | `ResolveSalvageContest` |
| Save QA slot | `SaveCampaign(SlotIndex)` |
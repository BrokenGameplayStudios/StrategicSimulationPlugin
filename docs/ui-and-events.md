# UI & events

## Base widget classes

| Class | Use |
|-------|-----|
| `UStrategyUserWidget` | CommonUI base for strategic HUD panels |
| `UStrategyActivatableWidget` | Menus and modal screens |
| `UStrategySalvageMapWidget` | Fog-aware wreck overlay |
| `UStrategyRadarContactMapWidget` | Radar contacts, hover intel, optional click-to-intercept |
| `UStrategicSimulationDisplayHelpers` | Blueprint function library for markers and tooltips |

## Sample content widgets

Under `Content/UI/`:

- `WBP_StrategicHUD` — main HUD; call `StartSimulation` from here
- `WBP_TimeControl` — pause and time scale
- `WBP_ResourcePanel`, `WBP_RosterScreen` — resources and soldiers
- `WBP_Notification` / `WBP_NotificationStack` — toasts

## Event dispatcher

Subscribe in Widget Blueprints to **UStrategyEventDispatcher**:

| Delegate | Use in UI |
|----------|-----------|
| `OnSoldierRecruited` / `OnSoldierListChanged` | Refresh roster |
| `OnResearchCompleted` | Tech unlocked toast |
| `OnVehicleCompleted` / `OnFacilityCompleted` / `OnProductionCompleted` | Production notifications |
| `OnSiteDiscovered` | New site on map |
| `OnSalvageSiteCreated` / `OnSalvageSiteRemoved` | Wreck markers |
| `OnSalvageContestStarted` | Launch tactical salvage fight |
| `OnRadarContactUpdated` / `OnRadarContactExpired` | Threat map layer |
| `OnOpposingFactionRadarAlert` | Enemy detected your vehicle |

## Time UI

Bind **`UTimeManagerSubsystem::OnSimulationClockStateChanged`** to update pause/speed button state. Use **`IsSimulationClockHalted`** for a single paused indicator.

## Blueprint cheat sheet

| Action | Call |
|--------|------|
| Start campaign | `UStrategyCampaignSubsystem::StartSimulation` |
| Unpause | `UTimeManagerSubsystem::TogglePause` |
| Get managers | `GetResourceManager`, `GetBaseManager`, `GetMissionManager`, … on Campaign |
| Intercept contact (player) | `TryLaunchInterceptionAtContactAuto` or `UStrategyRadarContactMapWidget::TryInterceptContactById` |
| Intercept from HUD button | `GetHoveredContactId` + `TryInterceptContactByIdForFaction` on radar widget |
| Site status (inspector) | `GetSiteStatusDisplayText(Site, BaseManager)` — includes under-construction |
| Resolve salvage fight | `ResolveSalvageContest` |
| Build salvage markers | `UStrategicSimulationDisplayHelpers::BuildSalvageMapMarkers` |
| Build radar markers | `BuildRadarContactMapMarkers` (optional `bIncludeOpposingFactionContacts`, `bAllowClickDispatch`) |
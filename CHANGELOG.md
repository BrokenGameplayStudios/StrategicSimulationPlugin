# Changelog

User-facing changes to the Strategic Simulation Plugin. For setup and usage, see the [Documentation Wiki](docs/README.md).

## [Unreleased]

### Documentation
- Full rewrite of user wiki (`docs/`) — data assets guide, removed API table, crew/expansion/spectate coverage
- Dev archive summaries updated; full design specs marked archive with status banners

### API cleanup (Tier A/B)
- Removed unused/legacy APIs: `AddDiscoveredSiteAtLocation`, `TryBuildBaseOnSite`, `LaunchMissionFromBase`, `PerformDailyBuildOrder`, `GetFormattedDate`, `UProductionManagerSubsystem`, engineering `TryProduce`/`StartProduction`/`GetActiveProduction`, research `TryResearch`/`ResearchDatabase`, soldier `GetCommander`/`DismissSoldier`/`ReleasePOW`/`Debug_PrintTeamRoster`, base POW daily hooks, facility `CancelConstruction`/`ProcessContainmentDaily`/`ProcessAutopsyDaily`, vehicle `LaunchScoutingMission`, intel `HasKnownSiteLocation`/`GetSiteIntelSnapshot`, events `OnSoldierDismissed`/`OnItemProduced`/`OnMonthlyEvent`, campaign `SetVictoryChances`/`SetDefeatKIAChance`
- Removed fields: `DiscoveringFaction`, `PingRadiusPixels`, `MaxMissionDurationDays`
- `SimulateOneDay` is a no-op; live missions complete in `UpdateAllLiveVehicles`
- `ResetProduction` clears workshop item jobs on facilities

### Data asset editor layout
- Consistent **Identity → Economy → gameplay → Unlocks** categories on all definition headers
- New guide: [docs/data-assets.md](docs/data-assets.md)

### Vehicle crew
- Vehicles require **at least one soldier** aboard before any mission can launch (including reactive interception)
- Crew is drawn from **soldiers stationed at the origin base** who are not already on another mission or vehicle
- AI and player missions **max-fill** each vehicle to `SoldierCapacity` when soldiers are available
- **Deferred** missions assign crew at **launch time** (not when queued), so soldiers stay available until departure
- Idle vehicle selection skips ships that have no crew and no assignable soldiers at base
- New log tags: `[CREW]` (assignment), warning when a wreck had no crew aboard

### Radar contact map (AI vs AI / spectate)
- **Left-click intercept disabled** by default while Human or Enemy AI simulation is on (`bAllowPlayerClickToIntercept`)
- Hover tooltips still show contact intel; action line reads *"AI handles interception when enabled"* instead of *"Click to launch interception"*
- **`bShowOpposingFactionContacts`** (default on) draws both factions' pings — cyan for Human, magenta for Enemy
- Designer dispatch: `GetHoveredContactId`, `GetHoveredContactFaction`, `TryInterceptContactByIdForFaction`, `IsClickToInterceptAllowed`
- `FRadarContactMapMarker` includes `ContactFaction` for multi-faction overlays

### Debug strategy map HUD
- CC under-construction overlay: `Enemy 'Forward Base 02': 4 day(s) left, on site 21...`
- Site inspector **Status** and base section show **Under Construction — N day(s) left** when CC is building
- Radar circles **40%** opacity; inspector yellow ring and blue/red discovery squares **80%**
- Radar LOS blockers drawn **brown** (legend updated)
- Site index numbers **centered above** the node at **2×** offset (clear of inspector yellow ring)
- Potential base nodes use **faction color** when a base exists or is under construction
- `GetSiteStatusDisplayText(Site, BaseManager)` resolves under-construction state

### Base expansion
- **Breaking:** New bases require a **Base Expansion** vehicle mission — race to the site, guard Command Center construction, then return home
- AI expansion runs **before** daily mission scheduling and can **preempt** deferred Recon/Offensive/Salvage missions when no inbound radar threats
- Guard destroyed, returning, or docking away before CC completes **cancels** construction and reopens the site (no refund)
- CC construction only advances while the guard is on-station (within 96 px, not returning/docked)
- Fixed expansion charging **double** Command Center cost (deduction now happens once in `BuildFacility`)
- Live combat at contested sites (no salvage-style contest UI)
- Blueprint events: `OnBaseExpansionOrdered`, `OnBaseExpansionClaimed`, `OnBaseExpansionCancelled`, `OnBaseExpansionGuardComplete`
- Tuning: `MaxActiveExpansionMissionsPerFaction`, `bBaseExpansionRequiresVehicleGuard`

### Construction timing
- Forward-base Command Centers now respect `BuildTimeDays` from the facility definition (initial starting bases remain instant)
- Fixed construction advancing up to 3× per day because AI duplicated `AdvanceFacilityConstruction` calls
- Command Centers use a single day-countdown path (no parallel production-queue build that could finish early)

### Radar contacts
- First-detection marker placed on the **passive radar ring** (backtracked along flight path), not at the vehicle's ping position
- Debug HUD contact diamonds use uniform size and opacity for Human and Enemy tracks

## [1.0.0] — 2026-06-18

### Simulation core
- Dual-faction strategic layer with daily AI (build, recruit, research, equip, missions)
- Logical 2D map with generated sites, two starting Command Centers, and mineable site resources
- Live vehicle movement with range budgeting and phased pathing (outbound → on-station → return)
- Pause, time scale, and `OnSimulationClockStateChanged` for UI time controls
- Strategic clock pause during contested salvage

### Missions & combat
- Mission types: Recon, Offensive, Salvage, Defensive, Interception, Base Expansion (see [Unreleased])
- Staggered daily mission launches across the 24-hour game day
- Armed vehicular combat for Gunship/Heavy vehicles
- Offensive base-attack missions after configurable start day (default: day 5)
- AI reactive interception and border patrol toward radar entry points
- En-route inbound threat engagement for combat vehicles in transit

### Salvage
- Wreck sites when vehicles are destroyed; combat participants know location immediately
- Salvage missions for Transport/Support/Scout vehicles
- Fog-aware salvage map widget with stale intel tooltips
- Contested salvage hook (pause clock, resolve via `ResolveSalvageContest`)
- Site-map save/load for QA (not a full Continue Game)

### Radar & intel
- Vehicle radar discovery of sites and enemy vehicles
- Command Center passive radar with contact registry and click-to-intercept map widget
- First-detection entry points for intercept and patrol targeting
- Stale site intel snapshots per faction
- Radar line-of-sight blocker zones (mountains)
- Optional player alert when enemy radar detects friendly vehicles

### UI & tooling
- `UStrategyEventDispatcher` Blueprint events for roster, production, sites, radar, salvage
- Test harness (`AStrategyTestActor`) and sample HUD widgets
- Debug strategic map HUD with bases, vehicles, paths, and radar overlays

### Known limitations
- Base attack arrival is log-only (no strategic base damage yet)
- Research unlock gating is a stub
- Mission resolution does not apply soldier casualties (crew can be lost on **vehicle destruction** via KIA/MIA at wreck sites)
- Ammo is treated as infinite when `MaxAmmo == 0`
- No tactical map load for player PvE fights yet
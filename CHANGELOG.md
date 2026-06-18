# Changelog

User-facing changes to the Strategic Simulation Plugin. For setup and usage, see the [Documentation Wiki](docs/README.md).

## [Unreleased]

### Base expansion
- **Breaking:** New bases require a **Base Expansion** vehicle mission — race to the site, guard Command Center construction, then return home
- AI expansion runs **before** daily mission scheduling and can **preempt** deferred Recon/Offensive/Salvage missions when no inbound radar threats
- Guard destroyed before CC completes cancels construction and reopens the site (no refund)
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
- Mission resolution does not apply soldier casualties
- Ammo is treated as infinite when `MaxAmmo == 0`
- No tactical map load for player PvE fights yet
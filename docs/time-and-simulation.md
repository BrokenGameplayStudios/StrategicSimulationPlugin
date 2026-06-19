# Time & simulation

## Clock states

The simulation uses a **real-time tick** that advances strategic game hours according to **time scale**. Time does not advance when:

- User pause is on (`UTimeManagerSubsystem::IsPaused()`)
- Strategic clock is paused (contested salvage)
- Time scale is 0

Use **`IsSimulationClockHalted()`** in UI for a single paused indicator.

## Starting and stopping

| Action | Blueprint call |
|--------|----------------|
| Generate map + begin campaign | `UStrategyCampaignSubsystem::StartSimulation` |
| Unpause after start | `UTimeManagerSubsystem::TogglePause` or `StartSimulation` |
| Stop (time scale 0) | `UStrategyCampaignSubsystem::StopSimulation` |
| Full reset | `UStrategyCampaignSubsystem::ResetSimulation` |
| Set time scale | `UTimeManagerSubsystem::SetTimeScale` |
| Advance N days instantly | `UTimeManagerSubsystem::AdvanceDays` |

**Important:** `Campaign.StartSimulation` sets time scale to 1× but leaves the clock **paused** until you unpause explicitly.

## Daily simulation order

On each **`OnDayPassed`** (24h elapsed):

1. Facility construction and production advance
2. Daily repairs and resource extraction
3. Salvage wreck expiry
4. AI daily loop per faction (if enabled)
5. Mission manager legacy `SimulateOneDay` hook (no-op — live missions finish in real time)

Live vehicle movement runs every frame via **`UpdateAllLiveVehicles`** while the clock is not halted.

## UI events (time manager)

Bind **`OnSimulationClockStateChanged`** to refresh pause/speed buttons. Parameters: time scale, user paused, strategic clock paused.

| Delegate | When it fires |
|----------|---------------|
| `OnSimulationStarted` | Campaign start date set |
| `OnDayPassed` | Each full 24h simulation period |

These live on **UTimeManagerSubsystem**, not the event dispatcher.

## Strategic clock pause (salvage contests)

When two factions salvage the same wreck, the campaign calls **`PauseStrategicClock`**. Mission movement and daily ticks freeze until **`ResolveSalvageContest`** resumes the clock. User pause is independent — both can be active.

## Dates and scheduling

| API | Subsystem | Use |
|-----|-----------|-----|
| `GetFormattedDateString` | TimeManager | HUD date label |
| `GetSimulationDayNumber` | TimeManager | 1-based day index for AI |
| `GetElapsedSimulationHours` | TimeManager | Mission launch hours, radar timing |
| `GetCurrentDay` | TimeManager | Calendar day-of-month |

There is no `GetFormattedDate` on the campaign subsystem (removed).
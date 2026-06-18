# Time & simulation

## Clock states

The simulation uses a **real-time tick** that advances strategic game hours according to **time scale**. Time does not advance when:

- User pause is on (`UTimeManagerSubsystem::IsPaused()`)
- Strategic clock is paused (contested salvage)
- Time scale is 0

Use **`IsSimulationClockHalted()`** in UI to show a paused state.

## Starting and stopping

| Action | Blueprint call |
|--------|----------------|
| Generate map + begin campaign | `UStrategyCampaignSubsystem::StartSimulation` |
| Unpause after start | `UTimeManagerSubsystem::TogglePause` or `StartSimulation` |
| Stop | `UStrategyCampaignSubsystem::StopSimulation` |
| Full reset | `UStrategyCampaignSubsystem::ResetSimulation` |
| Set time scale | `UTimeManagerSubsystem::SetTimeScale` |

**Important:** `Campaign.StartSimulation` sets time scale to 1× but leaves the clock **paused** until you unpause explicitly.

## UI events

Bind **`OnSimulationClockStateChanged`** on `UTimeManagerSubsystem` to refresh pause/speed buttons. Parameters: time scale, user paused, strategic clock paused.

Other useful delegates:

| Delegate | When it fires |
|----------|---------------|
| `OnSimulationStarted` | Campaign or start date set |
| `OnDayPassed` | Each full 24h simulation period |

## Strategic clock pause (salvage contests)

When two factions salvage the same wreck, the campaign calls **`PauseStrategicClock`**. Mission movement and daily ticks freeze until **`ResolveSalvageContest`** resumes the clock. User pause is separate — both can be active.

## Dates

- **`GetFormattedDateString`** — readable in-game date for HUD
- **`GetSimulationDayNumber`** — 1-based day index for AI scheduling
- **`GetElapsedSimulationHours`** — monotonic hours since campaign start (mission timing)
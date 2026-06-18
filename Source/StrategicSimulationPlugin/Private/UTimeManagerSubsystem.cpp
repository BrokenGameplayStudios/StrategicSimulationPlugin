#include "UTimeManagerSubsystem.h"
#include "UResourceManagerSubsystem.h"
#include "UAIControllerSubsystem.h"
#include "UMissionManagerSubsystem.h"
#include "Engine/Engine.h"

// Seeds default campaign date, starts paused, and registers the ~60 Hz real-time tick timer.
void UTimeManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    CurrentGameDate = FDateTime(2026, 1, 1, 0, 0, 0);
    SimulationStartDate = CurrentGameDate;
    PreviousTickGameDate = CurrentGameDate;
    LastBroadcastSimulationDay = -1;

    UE_LOG(LogTemp, Display, TEXT("UTimeManagerSubsystem initialized — Game started on %s (Paused by default)"), *CurrentGameDate.ToString());

    GetWorld()->GetTimerManager().SetTimer(RealTimeTimer, this, &UTimeManagerSubsystem::RealTimeTick, 0.016f, true);
}

// Clears the real-time tick timer before subsystem teardown.
void UTimeManagerSubsystem::Deinitialize()
{
    GetWorld()->GetTimerManager().ClearTimer(RealTimeTimer);
    Super::Deinitialize();
}

// Converts zero-based elapsed days to the 1-based day number used by AI scheduling.
int32 UTimeManagerSubsystem::GetSimulationDayNumber() const
{
    return GetTotalSimulationDays() + 1;
}

// Computes hours between SimulationStartDate and the current in-game date.
float UTimeManagerSubsystem::GetElapsedSimulationHours() const
{
    if (SimulationStartDate.GetTicks() == 0)
    {
        return 0.0f;
    }

    const FTimespan Elapsed = GetCurrentGameDate() - SimulationStartDate;
    return static_cast<float>(Elapsed.GetTotalSeconds()) / 3600.0f;
}

// Advances calendar time, ticks live vehicles, and broadcasts OnDayPassed when a day boundary is crossed.
void UTimeManagerSubsystem::ProcessSimulationStep(float StepSeconds)
{
    if (StepSeconds <= 0.0f)
    {
        return;
    }

    PreviousTickGameDate = CurrentGameDate;
    CurrentGameDate += FTimespan::FromSeconds(StepSeconds);

    const float StepGameHours = StepSeconds / 3600.0f;
    if (UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
    {
        MissionMgr->UpdateAllLiveVehicles(StepGameHours);
    }

    const int32 CurrentSimulationDay = GetTotalSimulationDays();
    if (CurrentSimulationDay > LastBroadcastSimulationDay)
    {
        const int32 SimulationDayNumber = CurrentSimulationDay + 1;
        UE_LOG(LogTemp, Display, TEXT("[TIME] Simulation day advanced → Day %d at %.2f elapsed hours (period %d)"),
            SimulationDayNumber, GetElapsedSimulationHours(), CurrentSimulationDay);
        OnDayPassed.Broadcast(SimulationDayNumber);
        LastBroadcastSimulationDay = CurrentSimulationDay;
    }
}

// Converts real-time delta into scaled simulation steps, capped per frame for stability.
void UTimeManagerSubsystem::RealTimeTick()
{
    if (bIsPaused || bStrategicClockPaused || TimeScale <= 0.0f) return;

    float RemainingSeconds = 0.016f * TimeScale;
    constexpr int32 MaxStepsPerRealTick = 32;
    int32 StepsThisTick = 0;

    while (RemainingSeconds > KINDA_SMALL_NUMBER && StepsThisTick < MaxStepsPerRealTick)
    {
        const float StepSeconds = FMath::Min(RemainingSeconds, MaxSimulationSecondsPerTick);
        ProcessSimulationStep(StepSeconds);
        RemainingSeconds -= StepSeconds;
        ++StepsThisTick;
    }
}

// True when time scale is positive and user pause is off (strategic pause not considered).
bool UTimeManagerSubsystem::IsSimulating() const
{
    return GetTimeScale() > 0.0f && !IsPaused();
}

// Whole days elapsed since SimulationStartDate based on calendar span.
int32 UTimeManagerSubsystem::GetTotalSimulationDays() const
{
    if (SimulationStartDate.GetTicks() == 0)
        return 0;

    FTimespan Elapsed = GetCurrentGameDate() - SimulationStartDate;
    return Elapsed.GetDays();
}

// Pushes current clock state to OnSimulationClockStateChanged listeners.
void UTimeManagerSubsystem::BroadcastClockStateChanged()
{
    OnSimulationClockStateChanged.Broadcast(TimeScale, bIsPaused, bStrategicClockPaused);
}

// Unpauses and sets 1x scale; fires OnSimulationStarted and clock-state delegate.
void UTimeManagerSubsystem::StartSimulation()
{
    SetTimeScale(1.0f);
    bIsPaused = false;

    UE_LOG(LogTemp, Display, TEXT("SIMULATION STARTED (unpaused) — Current date remains %s"), *CurrentGameDate.ToString());
    OnSimulationStarted.Broadcast();
    BroadcastClockStateChanged();
}

// Resets current and start dates; rebroadcasts day index for AI without starting the clock.
void UTimeManagerSubsystem::SetStartingDate(FDateTime NewStartDate)
{
    CurrentGameDate = NewStartDate;
    SimulationStartDate = NewStartDate;
    PreviousTickGameDate = NewStartDate;
    LastBroadcastSimulationDay = GetTotalSimulationDays();

    UE_LOG(LogTemp, Display, TEXT("Starting date set to %s — awaiting campaign start for Day %d AI"),
        *CurrentGameDate.ToString(), GetSimulationDayNumber());
    OnSimulationStarted.Broadcast();
}

// Enables user pause and notifies clock-state listeners.
void UTimeManagerSubsystem::StopSimulation()
{
    bIsPaused = true;
    UE_LOG(LogTemp, Display, TEXT("SIMULATION STOPPED"));
    BroadcastClockStateChanged();
}

// Jumps calendar by whole days and emits OnDayPassed for each newly entered simulation day.
void UTimeManagerSubsystem::AdvanceDays(int32 NumDays)
{
    if (NumDays <= 0) return;

    CurrentGameDate += FTimespan::FromDays(NumDays);

    const int32 NewSimulationDay = GetTotalSimulationDays();
    while (NewSimulationDay > LastBroadcastSimulationDay)
    {
        ++LastBroadcastSimulationDay;
        OnDayPassed.Broadcast(LastBroadcastSimulationDay + 1);
    }

    UE_LOG(LogTemp, Display, TEXT("Advanced %d days — Simulation day: %d"), NumDays, GetSimulationDayNumber());
}

// Clamps and applies a new time-scale multiplier; warns on extreme values.
void UTimeManagerSubsystem::SetTimeScale(float NewScale)
{
    TimeScale = FMath::Max(0.0f, NewScale);
    UE_LOG(LogTemp, Display, TEXT("Time scale set to %.4fx (Paused=%s)"), TimeScale, bIsPaused ? TEXT("YES") : TEXT("NO"));

    if (TimeScale >= 1000.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TIME] Extreme time scale (%.0fx) — expect heavy CPU/log load. Use Verbose=OFF for long unattended runs."), TimeScale);
    }

    BroadcastClockStateChanged();
}

// Flips user pause and broadcasts the new clock state.
void UTimeManagerSubsystem::TogglePause()
{
    bIsPaused = !bIsPaused;
    UE_LOG(LogTemp, Display, TEXT("Pause toggled — Now %s"), bIsPaused ? TEXT("PAUSED") : TEXT("RUNNING"));
    BroadcastClockStateChanged();
}

// Sets salvage-contest strategic pause without affecting user pause toggle.
void UTimeManagerSubsystem::SetStrategicClockPaused(bool bPaused)
{
    if (bStrategicClockPaused == bPaused)
    {
        return;
    }

    bStrategicClockPaused = bPaused;
    UE_LOG(LogTemp, Display, TEXT("[TIME] Strategic clock %s"), bPaused ? TEXT("PAUSED (salvage contest)") : TEXT("RESUMED"));
    BroadcastClockStateChanged();
}

// Calendar day-of-month from CurrentGameDate.
int32 UTimeManagerSubsystem::GetCurrentDay() const
{
    return CurrentGameDate.GetDay();
}

// Snapshot of the advanced in-game date.
FDateTime UTimeManagerSubsystem::GetCurrentGameDate() const
{
    return CurrentGameDate;
}

// Maps CurrentGameDate weekday enum to display text.
FText UTimeManagerSubsystem::GetCurrentDayOfWeekName() const
{
    switch (CurrentGameDate.GetDayOfWeek())
    {
    case EDayOfWeek::Monday:    return FText::FromString("Monday");
    case EDayOfWeek::Tuesday:   return FText::FromString("Tuesday");
    case EDayOfWeek::Wednesday: return FText::FromString("Wednesday");
    case EDayOfWeek::Thursday:  return FText::FromString("Thursday");
    case EDayOfWeek::Friday:    return FText::FromString("Friday");
    case EDayOfWeek::Saturday:  return FText::FromString("Saturday");
    case EDayOfWeek::Sunday:    return FText::FromString("Sunday");
    default:                    return FText::FromString("Unknown");
    }
}

// Builds uppercase month name + day + year for HUD display.
FString UTimeManagerSubsystem::GetFormattedDateString() const
{
    // Returns nice readable date like "MARCH 3, 2027"
    FString MonthName;
    switch (CurrentGameDate.GetMonth())
    {
    case 1:  MonthName = TEXT("JANUARY");   break;
    case 2:  MonthName = TEXT("FEBRUARY");  break;
    case 3:  MonthName = TEXT("MARCH");     break;
    case 4:  MonthName = TEXT("APRIL");     break;
    case 5:  MonthName = TEXT("MAY");       break;
    case 6:  MonthName = TEXT("JUNE");      break;
    case 7:  MonthName = TEXT("JULY");      break;
    case 8:  MonthName = TEXT("AUGUST");    break;
    case 9:  MonthName = TEXT("SEPTEMBER"); break;
    case 10: MonthName = TEXT("OCTOBER");   break;
    case 11: MonthName = TEXT("NOVEMBER");  break;
    case 12: MonthName = TEXT("DECEMBER");  break;
    default: MonthName = TEXT("UNKNOWN");   break;
    }

    return FString::Printf(TEXT("%s %d, %d"), *MonthName, CurrentGameDate.GetDay(), CurrentGameDate.GetYear());
}
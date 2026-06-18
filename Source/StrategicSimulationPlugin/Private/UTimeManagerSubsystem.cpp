#include "UTimeManagerSubsystem.h"
#include "UResourceManagerSubsystem.h"
#include "UAIControllerSubsystem.h"
#include "UMissionManagerSubsystem.h"
#include "Engine/Engine.h"

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

void UTimeManagerSubsystem::Deinitialize()
{
    GetWorld()->GetTimerManager().ClearTimer(RealTimeTimer);
    Super::Deinitialize();
}

int32 UTimeManagerSubsystem::GetSimulationDayNumber() const
{
    return GetTotalSimulationDays() + 1;
}

float UTimeManagerSubsystem::GetElapsedSimulationHours() const
{
    if (SimulationStartDate.GetTicks() == 0)
    {
        return 0.0f;
    }

    const FTimespan Elapsed = GetCurrentGameDate() - SimulationStartDate;
    return static_cast<float>(Elapsed.GetTotalSeconds()) / 3600.0f;
}

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

void UTimeManagerSubsystem::RealTimeTick()
{
    if (bIsPaused || TimeScale <= 0.0f) return;

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

bool UTimeManagerSubsystem::IsSimulating() const
{
    return GetTimeScale() > 0.0f && !IsPaused();
}

int32 UTimeManagerSubsystem::GetTotalSimulationDays() const
{
    if (SimulationStartDate.GetTicks() == 0)
        return 0;

    FTimespan Elapsed = GetCurrentGameDate() - SimulationStartDate;
    return Elapsed.GetDays();
}

void UTimeManagerSubsystem::StartSimulation()
{
    SetTimeScale(1.0f);
    bIsPaused = false;

    UE_LOG(LogTemp, Display, TEXT("SIMULATION STARTED (unpaused) — Current date remains %s"), *CurrentGameDate.ToString());
    OnSimulationStarted.Broadcast();
}

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

void UTimeManagerSubsystem::StopSimulation()
{
    bIsPaused = true;
    UE_LOG(LogTemp, Display, TEXT("SIMULATION STOPPED"));
}

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

void UTimeManagerSubsystem::SetTimeScale(float NewScale)
{
    TimeScale = FMath::Max(0.0f, NewScale);
    UE_LOG(LogTemp, Display, TEXT("Time scale set to %.4fx (Paused=%s)"), TimeScale, bIsPaused ? TEXT("YES") : TEXT("NO"));

    if (TimeScale >= 1000.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TIME] Extreme time scale (%.0fx) — expect heavy CPU/log load. Use Verbose=OFF for long unattended runs."), TimeScale);
    }
}

void UTimeManagerSubsystem::TogglePause()
{
    bIsPaused = !bIsPaused;
    UE_LOG(LogTemp, Display, TEXT("Pause toggled — Now %s"), bIsPaused ? TEXT("PAUSED") : TEXT("RUNNING"));
}

int32 UTimeManagerSubsystem::GetCurrentDay() const
{
    return CurrentGameDate.GetDay();
}

FDateTime UTimeManagerSubsystem::GetCurrentGameDate() const
{
    return CurrentGameDate;
}

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
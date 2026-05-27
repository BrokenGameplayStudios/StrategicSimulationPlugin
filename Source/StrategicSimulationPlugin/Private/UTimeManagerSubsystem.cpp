#include "UTimeManagerSubsystem.h"
#include "UResourceManagerSubsystem.h"
#include "UAIControllerSubsystem.h"
#include "Engine/Engine.h"

void UTimeManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    CurrentGameDate = FDateTime(2026, 1, 1, 0, 0, 0);
    SimulationStartDate = CurrentGameDate;

    UE_LOG(LogTemp, Display, TEXT("UTimeManagerSubsystem initialized — Game started on %s (Paused by default)"), *CurrentGameDate.ToString());

    GetWorld()->GetTimerManager().SetTimer(RealTimeTimer, this, &UTimeManagerSubsystem::RealTimeTick, 0.016f, true);
}

void UTimeManagerSubsystem::Deinitialize()
{
    GetWorld()->GetTimerManager().ClearTimer(RealTimeTimer);
    Super::Deinitialize();
}

void UTimeManagerSubsystem::RealTimeTick()
{
    if (bIsPaused || TimeScale <= 0.0f) return;

    float DeltaSeconds = 0.016f * TimeScale;
    CurrentGameDate += FTimespan::FromSeconds(DeltaSeconds);

    static int32 LastDay = 0;
    int32 CurrentDayNum = CurrentGameDate.GetDay();

    if (CurrentDayNum != LastDay)
    {
        OnDayPassed.Broadcast(CurrentDayNum);
        LastDay = CurrentDayNum;

        if (UAIControllerSubsystem* AI = GetGameInstance()->GetSubsystem<UAIControllerSubsystem>())
        {
            if (AI->IsAIEnabled())
            {
                AI->RunAIForFaction(EFactionType::Enemy, CurrentDayNum);
            }
        }
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

    UE_LOG(LogTemp, Display, TEXT("Starting date set to %s"), *CurrentGameDate.ToString());
    OnSimulationStarted.Broadcast();   // UI / systems can now reset soldiers, resources, etc.
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
    OnDayPassed.Broadcast(CurrentGameDate.GetDay());

    UE_LOG(LogTemp, Display, TEXT("Advanced %d days — Current Day: %d"), NumDays, CurrentGameDate.GetDay());
}

void UTimeManagerSubsystem::SetTimeScale(float NewScale)
{
    TimeScale = FMath::Max(0.0f, NewScale);
    UE_LOG(LogTemp, Display, TEXT("Time scale set to %.4fx (Paused=%s)"), TimeScale, bIsPaused ? TEXT("YES") : TEXT("NO"));
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
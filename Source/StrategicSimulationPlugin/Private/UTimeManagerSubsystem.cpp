#include "UTimeManagerSubsystem.h"
#include "UResourceManagerSubsystem.h"
#include "UAIControllerSubsystem.h"
#include "Engine/Engine.h"

void UTimeManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    CurrentGameDate = FDateTime(2026, 1, 1, 0, 0, 0);

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

        // Direct AI call every day (this is the reliable way)
        if (UAIControllerSubsystem* AI = GetGameInstance()->GetSubsystem<UAIControllerSubsystem>())
        {
            AI->RunAIForFaction(EFactionType::Enemy, CurrentDayNum);
        }

        UResourceManagerSubsystem* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>();
        if (ResourceMgr) ResourceMgr->TickResources(1.0f);
    }
}

void UTimeManagerSubsystem::SetStartingDate(FDateTime NewStartDate)
{
    CurrentGameDate = NewStartDate;
    UE_LOG(LogTemp, Display, TEXT("Starting date set to %s"), *CurrentGameDate.ToString());
}

void UTimeManagerSubsystem::StartSimulation()
{
    bIsPaused = false;
    UE_LOG(LogTemp, Display, TEXT("SIMULATION STARTED"));
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

    UResourceManagerSubsystem* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceManagerSubsystem>();
    if (ResourceMgr)
    {
        for (int32 i = 0; i < NumDays; i++)
            ResourceMgr->TickResources(1.0f);
    }

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
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Misc/DateTime.h"
#include "StrategicSimulationTypes.h"
#include "UTimeManagerSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDayPassed, int32, NewDay);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSimulationStarted);
/** Broadcast when time scale or any pause flag changes. Bind UI speed/pause indicators here. Params: TimeScale, bIsPaused (user toggle), bStrategicClockPaused (salvage contest). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSimulationClockStateChanged, float, TimeScale, bool, bIsPaused, bool, bStrategicClockPaused);

/**
 * Game-instance subsystem that advances in-game calendar time and drives per-tick simulation steps.
 * Owns the strategic clock (scale, user pause, salvage-contest pause) and broadcasts day/clock events
 * consumed by campaign, AI, and mission systems.
 */
UCLASS()
class STRATEGICSIMULATIONPLUGIN_API UTimeManagerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /** Called once when the subsystem is created; seeds default date and starts the real-time tick timer. */
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /** Called on shutdown; clears the real-time tick timer. */
    virtual void Deinitialize() override;

    /** The in-game date/time when the current simulation run was started */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Simulation")
    FDateTime SimulationStartDate;

    /** Fires when SetStartingDate or StartSimulation is called (perfect for New Game / Reset logic) */
    UPROPERTY(BlueprintAssignable, Category = "Simulation")
    FOnSimulationStarted OnSimulationStarted;

    /**
     * Returns whether the simulation clock is actively advancing.
     * Call from UI or gameplay to reflect play/pause state.
     * @return True when time scale is positive and user pause is off.
     */
    UFUNCTION(BlueprintCallable, Category = "Simulation")
    bool IsSimulating() const;

    /**
     * Returns how many full 24-hour periods have elapsed since simulation start.
     * Call for day-based rules that use zero-based period counts.
     * @return Elapsed whole days since SimulationStartDate (0 on the first day).
     */
    UFUNCTION(BlueprintCallable, Category = "Simulation")
    int32 GetTotalSimulationDays() const;

    /**
     * Returns the 1-based simulation day index used by AI and mission scheduling.
     * Call when scheduling daily orders or displaying "Day N" to the player.
     * @return Day 1 = first 24 hours after start.
     */
    UFUNCTION(BlueprintCallable, Category = "Simulation")
    int32 GetSimulationDayNumber() const;

    /**
     * Returns monotonic elapsed in-game hours since SimulationStartDate.
     * Call for mission movement, salvage windows, and sub-day timing.
     * @return Hours since start (0 at SimulationStartDate).
     */
    UFUNCTION(BlueprintCallable, Category = "Simulation")
    float GetElapsedSimulationHours() const;

    /**
     * Starts or resumes simulation without changing the current calendar date.
     * Call from campaign Start or UI resume; sets scale to 1x and clears user pause.
     */
    UFUNCTION(BlueprintCallable, Category = "Simulation")
    void StartSimulation();

    /**
     * Sets the campaign calendar anchor and current date (New Game / load prep).
     * Call before StartSimulation when beginning or resetting a campaign date.
     * @param NewStartDate In-game date that becomes both current and start reference.
     */
    UFUNCTION(BlueprintCallable, Category = "Time")
    void SetStartingDate(FDateTime NewStartDate);

    /**
     * Stops time advancement by enabling user pause.
     * Call from UI pause or campaign StopSimulation; does not reset the calendar.
     */
    UFUNCTION(BlueprintCallable, Category = "Time")
    void StopSimulation();

    /**
     * Instantly jumps the calendar forward by whole days and fires OnDayPassed for each crossed day.
     * Call from save-load or debug; does not require the clock to be running.
     * @param NumDays Days to add (ignored if <= 0).
     */
    UFUNCTION(BlueprintCallable, Category = "Time")
    void AdvanceDays(int32 NumDays);

    /**
     * Sets how fast in-game time advances relative to real time.
     * Call from UI speed controls; values <= 0 halt the clock.
     * @param NewScale Multiplier (1.0 = real-time ratio at base tick rate).
     */
    UFUNCTION(BlueprintCallable, Category = "Time")
    void SetTimeScale(float NewScale);

    /**
     * Toggles user-initiated pause (distinct from strategic/salvage pause).
     * Call from UI pause button; broadcasts OnSimulationClockStateChanged.
     */
    UFUNCTION(BlueprintCallable, Category = "Time")
    void TogglePause();

    /**
     * Returns the calendar day-of-month component of the current in-game date.
     * Call for HUD date widgets (not the 1-based simulation day index).
     * @return Day of month (1–31).
     */
    UFUNCTION(BlueprintCallable, Category = "Time")
    int32 GetCurrentDay() const;

    /**
     * Returns a human-readable uppercase month/day/year string for the HUD.
     * Call from UI date displays.
     * @return Formatted string such as "MARCH 3, 2027".
     */
    UFUNCTION(BlueprintCallable, Category = "Time")
    FString GetFormattedDateString() const;

    /**
     * Returns the full current in-game date/time.
     * Call when persisting or comparing exact campaign timestamps.
     * @return Current FDateTime advanced by the simulation clock.
     */
    UFUNCTION(BlueprintCallable, Category = "Time")
    FDateTime GetCurrentGameDate() const;

    /**
     * Returns the English name of the current weekday.
     * Call from UI calendar or event scheduling displays.
     * @return Localized weekday text.
     */
    UFUNCTION(BlueprintCallable, Category = "Time")
    FText GetCurrentDayOfWeekName() const;

    /**
     * Returns the active time-scale multiplier.
     * Call from UI to show current speed setting.
     * @return Current TimeScale (0 halts advancement).
     */
    UFUNCTION(BlueprintCallable, Category = "Time")
    float GetTimeScale() const { return TimeScale; }

    /**
     * Returns whether the player has toggled pause.
     * Call from UI pause icon state; independent of strategic pause.
     * @return True when user pause is active.
     */
    UFUNCTION(BlueprintCallable, Category = "Time")
    bool IsPaused() const { return bIsPaused; }

    /**
     * Returns whether in-game time is frozen for any reason.
     * Call before assuming missions/vehicles will advance this frame.
     * True when user pause, strategic (salvage contest) pause, or zero/negative time scale.
     * @return True when the simulation clock will not advance on the next tick.
     */
    UFUNCTION(BlueprintPure, Category = "Time")
    bool IsSimulationClockHalted() const { return bIsPaused || bStrategicClockPaused || TimeScale <= 0.0f; }

    /**
     * Sets strategic (non-user) pause used during contested salvage resolution.
     * Call from UStrategyCampaignSubsystem::PauseStrategicClock / mission contest flow.
     * Does not toggle user pause; broadcasts OnSimulationClockStateChanged when changed.
     * @param bPaused True to freeze the clock until contest resolves.
     */
    UFUNCTION(BlueprintCallable, Category = "Time|Strategic Pause")
    void SetStrategicClockPaused(bool bPaused);

    /**
     * Returns whether salvage-contest strategic pause is active.
     * Call to gate UI or systems that should wait for contest resolution.
     * @return True when strategic pause was set by SetStrategicClockPaused.
     */
    UFUNCTION(BlueprintPure, Category = "Time|Strategic Pause")
    bool IsStrategicClockPaused() const { return bStrategicClockPaused; }

    /** Fires once per simulation day boundary with the 1-based day number. */
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnDayPassed OnDayPassed;

    /** Fires when time scale or pause state changes — bind UI speed/pause buttons here. */
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnSimulationClockStateChanged OnSimulationClockStateChanged;

private:
    FDateTime CurrentGameDate;
    FDateTime PreviousTickGameDate;

    float TimeScale = 1.0f;
    bool bIsPaused = true;
    bool bStrategicClockPaused = false;

    FTimerHandle RealTimeTimer;

    int32 LastBroadcastSimulationDay = -1;

    /** Max simulated seconds processed per real-time tick (prevents single-frame meltdown at extreme time scale). */
    static constexpr float MaxSimulationSecondsPerTick = 600.f;

    /** Real-time timer callback; accumulates scaled seconds and steps simulation when not halted. */
    void RealTimeTick();
    /** Applies one chunk of simulated seconds: advances date, updates vehicles, may broadcast OnDayPassed. */
    void ProcessSimulationStep(float StepSeconds);
    /** Notifies listeners of current TimeScale, bIsPaused, and bStrategicClockPaused. */
    void BroadcastClockStateChanged();
};
# Missions & AI

## Mission types

| Type | Who flies | Typical purpose |
|------|-----------|-----------------|
| Recon | Scout, Transport, Support | Discover sites, survey nodes, patrol spokes |
| Offensive | Gunship, Heavy | Attack enemy base (after start day) |
| Salvage | Transport, Support, Scout | Recover wreck resources |
| Defensive | Combat types with range | Patrol toward inbound threat entry lanes |
| Interception | Gunship, Heavy | Engage tracked radar contact |
| Base Expansion | Any with range | Race to claim a site and guard CC construction |

All missions use **live movement** — vehicles fly on the logical map in real time. There is no abstract day-countdown mission path.

## Launching missions

**Do not use** `LaunchMissionFromBase` (removed).

| API | Use |
|-----|-----|
| `UMissionManagerSubsystem::StartMission` | Canonical launch (vehicles, soldiers, type, faction, optional scheduled hour) |
| `ScheduleVehicleMissionsForBase` | AI/player staggered daily scheduling |
| `GatherIdleVehiclesAtBase` | Vehicles available to fly today |
| `LaunchInterceptionAtContact` | Intercept a radar contact by ID |
| `TryLaunchInterceptionAtContactAuto` | Pick best contact + vehicle at base |
| `UBaseManagerSubsystem::StartBaseExpansion` | Base expansion mission |

Mission completion: **`OnMissionCompleted`** on `UMissionManagerSubsystem` (not on event dispatcher).

## Vehicle crew

Every departing vehicle needs **at least one soldier** aboard:

- Crew from soldiers **stationed at the origin base** who are not on another mission or vehicle
- Missions **max-fill** to `SoldierCapacity` when soldiers are available
- **Deferred** missions assign crew at **launch hour**, not when queued
- Reactive interception uses the same rules; launch aborts with `[CREW]` if no soldiers

On **vehicle destruction**, aboard soldiers roll `VehicleCrashDeathChance` for KIA; survivors become **MIA** at the wreck.

| API | Purpose |
|-----|---------|
| `UStrategyVehicle::HasMinimumCrew` / `GetCrewCount` | Blueprint checks |
| `TryAssignMissionCrew` | Assign before/at launch |
| `GatherMissionReadySoldiersAtBase` | Eligible roster at base |
| `ProcessCrewOnVehicleDestruction` | Wreck KIA/MIA |

## Base expansion (vehicle-guarded)

New bases are **not** built instantly. **`TryBuildBaseOnSite` was removed.**

1. `StartBaseExpansion(Faction, Site, OriginBase, Vehicle, BaseName)` orders the mission
2. First vehicle to reach a valid site calls **`TryClaimExpansionSite`** — pays CC cost once, starts construction
3. Guard must stay **on-station** (`IsExpansionBaseGuarded`) for CC build days to advance
4. Guard destroyed or leaving early → **`CancelExpansionConstruction`** — site reopens, **no refund**
5. CC complete → guard returns home

AI runs expansion **before** daily mission scheduling and may preempt deferred Recon/Offensive/Salvage when no inbound radar threats.

Events: `OnBaseExpansionOrdered`, `OnBaseExpansionClaimed`, `OnBaseExpansionCancelled`, `OnBaseExpansionGuardComplete`.

## Daily AI loop

Per faction when AI enabled (`RunAIForFaction`):

1. Apply facility income; advance construction and production
2. Facility repairs
3. Recruit soldiers (`TryRecruit`)
4. Start research (`TryResearch` — uses `ResearchDatabaseAsset` on campaign)
5. Buy/equip gear (`TryBuyAndEquip`)
6. Build facilities and vehicles
7. **Base expansion** (`TryStartAIExpansion`)
8. Schedule staggered missions for idle vehicles
9. Optional verbose base dump

Manual tick: **`Debug_RunAI`** on campaign or AI subsystem. There is no **`PerformDailyBuildOrder`** (removed).

## Mission scheduling priority (combat vehicles)

1. **Interception** — interceptable radar contact in range
2. **Defensive** — inbound threats near home base
3. **Offensive** — enemy base in range after start day
4. **Recon** — fallback for non-combat types

## Recon target priority

1. Patrol to **radar entry lane** (inbound threat first-detection point)
2. Survey undiscovered site already known to faction
3. Spoke-and-wheel patrol from base
4. Random patrol within range

## Combat

Gunship/Heavy engage when:

- Different faction
- Equipped weapon
- Offensive rating ≥ `MinOffenseToEngage`

Combat vehicles may engage **inbound threats in transit** when `bEngageInboundThreatsWhileInTransit` is on.

Destroyed vehicles create **salvage wrecks** when salvage is enabled.

## Base attack

Offensive missions fly to an enemy Command Center. On arrival the game logs **`[BASE ATTACK EVENT]`**. **`HandleBaseAttackArrival`** does not apply strategic damage yet.

## Reactive interception

Passive radar **inbound** contacts queue reactive gunship launches (`TryReactiveInterception`) when faction AI and `bAIReactiveInterceptionEnabled` are on and the base has crew.

Player/designer intercept: radar widget or `TryLaunchInterceptionAtContactAuto` (see [Radar & intel](radar-and-intel.md)).

## Production (facilities)

All queued production lives on **UStrategyFacility** — not a separate production manager.

| Output | Facility | Started by |
|--------|----------|------------|
| Soldiers | Living Quarters | AI `TryRecruit` / facility queue |
| Vehicles | Hangar | AI `TryBuildVehicle` |
| Items | Workshop | AI purchase path (instant) or facility queue |
| Research | Laboratory | `UResearchManagerSubsystem::StartResearch` |
| New facility | Self | `StartConstruction` |

Instant procurement: **`UEngineeringManagerSubsystem::PurchaseItem`**, `PurchaseAndEquipVehicleWeapon`, `PurchaseAmmoForVehicle`.

## What is not implemented

- Soldier casualties on abstract mission **resolve** (`SoldiersKilled` stays 0)
- Combat POW/KIA from campaign tuning props
- Strategic base damage on offensive arrival
- Per-faction research completion tracking (unlock stubs)
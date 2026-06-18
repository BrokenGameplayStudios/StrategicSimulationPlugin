# Missions & AI

## Mission types

| Type | Who flies | Typical purpose |
|------|-----------|-----------------|
| Recon | Scout, Transport, Support | Discover sites, survey nodes, patrol spokes |
| Offensive | Gunship, Heavy | Attack enemy base (after start day) |
| Salvage | Transport, Support, Scout | Recover wreck resources |
| Defensive | Any with range | Patrol toward inbound threat entry lanes |
| Interception | Gunship, Heavy | Engage tracked radar contact |
| Base Expansion | Any with range | Race to claim a site and guard Command Center construction |

All new missions use **live movement** (vehicles fly on the map in real time).

## Base expansion (vehicle-guarded)

New bases are **not** built instantly. A faction must dispatch a **Base Expansion** mission:

1. Vehicle flies to a discovered, unused `PotentialBase` site (factions can **race** for the same site).
2. First survivor to reach the site **claims** it, pays Command Center cost, and starts construction.
3. The vehicle **guards on-station** until the Command Center is operational (live combat if challenged).
4. Guard returns to its **origin** home base when construction completes.

If the **guard is destroyed** before the Command Center finishes, construction is **cancelled**, the site reopens, and the Command Center cost is **not** refunded. Choose your fleet wisely — scouts arrive fast but gunships hold the site better.

Player API: `StartBaseExpansion` on `UBaseManagerSubsystem`. Events: `OnBaseExpansionOrdered`, `OnBaseExpansionClaimed`, `OnBaseExpansionCancelled`, `OnBaseExpansionGuardComplete`.

## Daily AI loop

Each simulation day, per faction (if AI enabled):

1. Facility construction and repairs
2. Soldier recruitment and training
3. Research and production
4. Vehicle build (scouts first, then combat)
5. Equip soldiers and vehicle weapons
6. Schedule missions for idle vehicles (staggered across 24h)
7. **Base expansion** (before routine mission scheduling) — preempts deferred Recon/Offensive/Salvage slots when no inbound threats; prefers combat vehicles

Force a tick for testing: **`Debug_RunAI`** on campaign or AI subsystem.

## Mission scheduling priority (combat vehicles)

1. **Interception** — if faction has interceptable radar contacts in range
2. **Defensive** — if inbound threats detected near home base
3. **Offensive** — if enemy base in range and start day reached
4. **Recon** — fallback for non-combat types

## Recon target priority

1. Patrol to **radar entry lane** (inbound threat first-detection point)
2. Survey undiscovered site already known to faction
3. Spoke-and-wheel patrol expanding from base
4. Random patrol within range

## Combat

Gunship/Heavy vehicles engage when:

- Different faction
- Equipped weapon
- Offensive rating ≥ `MinOffenseToEngage`

Combat vehicles may also engage **inbound threats while in transit** when that option is enabled on the campaign.

Combat ends on destruction, timeout, or range exhaustion. Destroyed vehicles create **salvage wrecks** (if salvage is enabled).

## Base attack

Offensive missions fly to an enemy Command Center. On arrival the game logs **`[BASE ATTACK EVENT]`**. Strategic base damage is **not** applied yet.

## Reactive interception

When passive radar detects an **inbound** vehicle, idle gunships at that base may launch immediately (AI or player). Player can also click contacts on the radar map widget.

## What is not implemented

- Abstract day-countdown missions (legacy path unused)
- Soldier casualties on mission resolve (always zero)
- Base attack outcome / strategic base damage
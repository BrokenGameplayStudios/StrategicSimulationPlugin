# Salvage Sites Design — Summary

**See also:** [Documentation wiki](./README.md) · [Radar & intel (next)](./design-radar-intel-patrol-summary.md)

**Produced:** June 17, 2026 (revision 5 — product decisions; PR-2 implemented)  
**Full document:** [`design-salvage-sites.md`](./design-salvage-sites.md)  
**Review:** [`design-salvage-sites-review.md`](./design-salvage-sites-review.md)

---

## Status

**Approved for PR-1–7 + PR-6b.** Revision 5 incorporates user product decisions on fog-of-war, depletion, contested salvage, and universal wreck creation.

**Follow-on:** [Radar, intel & patrol](./design-radar-intel-patrol-summary.md) (PR-8–14) extends fog-of-war with stale intel, LOS, base radar, and patrol/interception missions.

---

## Product decisions (rev 5)

| Topic | Decision |
|-------|----------|
| **Depleted wrecks** | **Disappear** from map — removed from `AllPotentialSites`, not gray markers |
| **Both factions salvage** | **Yes** — contested salvage fires `OnSalvageContestStarted`, pauses strategic clock, loads tactical scenario from both force snapshots; abort leaves wreck for other faction (XCOM-style) |
| **Wreck creation** | **All destroyed vehicles** create salvage sites; geography filter (ocean/jungle) deferred until per-pixel terrain exists |
| **Salvage fog-of-war** | **Combat-known** — engagement participants already know wreck location; sending salvage is a strategic choice (winner may return expecting retaliation) |
| **Regular site fog** | Radar discovery + blue/red dots on debug map (unchanged) |
| **Vehicles** | Spotted only by radar (unchanged) |

---

## Fog-of-war model (split)

```
PotentialBase / ResourceNode  →  hidden until friendly radar ping  →  discovery dots
SalvageSite (combat)        →  auto-known to KnownFactions       →  strategic dispatch choice
```

---

## PR plan

| PR | Title | Focus |
|----|-------|-------|
| **PR-1** | Foundation + discovery fix | `KnownFactions`, `RegisterCombatKnownSalvage`, `SiteId`, guards |
| **PR-2** | Debug HUD hardening | Inspector, visibility helpers, removed wrecks not drawn |
| **PR-3** | Event dispatcher | Site/salvage/contest events |
| **PR-4** | Site-map save/load (QA) | Serialize sites + clear missions on load |
| **PR-5** | Player map | Fog-aware icons (combat-known wrecks visible immediately) |
| **PR-6** | Salvage mission MVP | Extraction + `RemoveSalvageSite` on depletion |
| **PR-6b** | Contested salvage dispatch | Pause clock, force snapshots, tactical hook, abort/win outcomes |
| **PR-7** | AI & balance | Salvage vs return-after-win, retaliation tuning |

---

## PR-2 implemented (rev 5+)

| Feature | Detail |
|---------|--------|
| **Wreck expiry** | `SalvageWreckExpiryDays` (default 7) on initializer + campaign; auto-remove via `ProcessSalvageSiteExpiry` |
| **Inspector** | Days remaining, full salvage resources (M/Mt/Bio/Chem/Exo), MIA list, KIA crash count |
| **Crash crew** | `VehicleCrashDeathChance` dice → KIA; survivors → MIA at wreck |
| **MIA rescue** | `RescueMIAsFromWreck` (own faction salvage, PR-6 wire-up) |
| **MIA → POW** | `ProcessMIAsOnOpposingSalvage` + `OpposingSalvageMIAPOWChance` (AI dice; player = PR-6b combat) |
| **Display helpers** | `GetSiteStatusDisplayText`, `ShouldShowSalvageToFaction`, salvage discovery dots on triangles |

## Key architectural additions (rev 5)

- `KnownFactions` on `UStrategySiteDefinition` — who knows wreck without radar
- `RegisterCombatKnownSalvage()` — auto-populate `DiscoveredSites*` for combat participants
- `RemoveSalvageSite()` — depletion removes wreck entirely
- `OnSalvageContestStarted` + `PauseStrategicClock` / `ResolveSalvageContest` — contested dual-faction salvage
- `FSalvageContestForceSnapshot` — vehicle/soldier stats for tactical layer

---

## Review history

| Round | Issues | Result |
|-------|--------|--------|
| Rev 1–4 | 29 | All closed (engineering review) |
| Rev 5 | Product decisions | Incorporated directly |
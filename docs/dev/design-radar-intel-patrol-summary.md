# Radar, Intel & Patrol — Summary

**See also:** [Documentation wiki](./README.md) · [Full design](./design-radar-intel-patrol.md) · [Salvage sites summary](./design-salvage-sites-summary.md)

**Produced:** June 2026 · **Status:** PR-12 shipped; PR-13 next

---

## Goal

Upgrade the strategic layer from **permanent perfect intel** to **fog-of-war with stale snapshots**, **LOS-aware radar** (mountain zones), **base passive scanning**, **enemy contact tracks**, and **patrol/guard + interception** missions — increasing vehicular combat and salvage contests.

---

## Product decisions

| Topic | Decision |
|-------|----------|
| **Site location** | Stays known after first sighting |
| **Site details** | Resources, base-built state are **stale** until revisited |
| **Mountains (v1)** | Circular/rect **blocker zones** on initializer; ray LOS |
| **Base radar** | Command Center passive ping (v1); dedicated facility later |
| **Contacts** | Faction registry: last position, estimated heading, expiry |
| **Defensive missions** | Guard patrol nodes near threatened bases (fix current bug: Defensive targets enemy bases today) |
| **En-route defense** | Friendly ships engage enemies **inbound to own bases** |

---

## PR plan

| PR | Focus |
|----|-------|
| **PR-8** | Docs wiki, `bAllowDebugExecCommands`, `bRadarLOSEnabled`, `bStaleIntelEnabled` |
| **PR-9** | `UFactionIntelSubsystem`, stale UI, save schema v3 |
| **PR-10** | `URadarTerrainSubsystem`, blocker zones, LOS in all radar |
| **PR-11** | Base passive radar tick |
| **PR-12** | `FRadarContact`, `OnRadarContactUpdated`, universal wreck destroy |
| **PR-13** | Patrol/Guard missions + AI |
| **PR-14** | Interception scheduling + in-transit engagement |

---

## Salvage integration

- Stale wreck tooltips (PR-9)
- More combat → more wrecks → more salvage contests (PR-12–14)
- Prior gaps folded in: Exec shipping guard (PR-8), `ensure` on salvage base build (PR-8)

---

## Out of scope

- Pixel geography map / zone authoring tool
- Tactical combat map load
- Full Continue Game save (intel extends site-map save only)
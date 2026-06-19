# Radar, Intel & Patrol — Implementation Summary

**Last updated:** June 2026  
**User guide:** [Radar & intel](../radar-and-intel.md) · [Missions & AI](../missions-and-ai.md)

Full PR-8–14 spec: [`design-radar-intel-patrol.md`](./design-radar-intel-patrol.md) (archive).

---

## Shipped behavior

| Feature | Implementation |
|---------|----------------|
| Stale intel | `UFactionIntelSubsystem` — `GetDisplayResources`, `GetDisplayHasBase`, save schema v3 |
| Radar LOS | `URadarTerrainSubsystem` + `RadarBlockerZones` on initializer |
| Vehicle radar | `UStrategyVehicle::PerformRadarPing` — sites, vehicles, bases |
| Base passive radar | `URadarContactSubsystem::TickBaseRadar` — `FRadarContact` registry |
| First-detection point | Entry position on passive ring (backtracked) |
| Contact expiry | `RadarContactExpiryHours` |
| Defensive / recon patrol | `UExplorationSubsystem` — entry lanes, spokes, hot spokes |
| Interception missions | `LaunchInterceptionAtContact`, reactive queue |
| En-route engagement | `bEngageInboundThreatsWhileInTransit` |
| Player radar widget | `UStrategyRadarContactMapWidget` — spectate mode, designer dispatch APIs |
| Enemy alert | `OnOpposingFactionRadarAlert` |

## Spectate / AI vs AI (2026)

- `bAllowPlayerClickToIntercept` default **false** while AI sim runs
- `bShowOpposingFactionContacts` default **true** — both factions drawn
- Designer APIs: `GetHoveredContactId`, `TryInterceptContactByIdForFaction`, etc.

## Removed / not exposed

- `HasKnownSiteLocation`, `GetSiteIntelSnapshot` — use display helpers on intel subsystem
- Dedicated radar facility type — CC passive radar only in v1

## PR map (historical)

| PR | Focus | Status |
|----|-------|--------|
| PR-8 | Wiki, feature flags, exec guard | Done |
| PR-9 | Intel subsystem + stale UI | Done |
| PR-10 | Terrain LOS | Done |
| PR-11 | Base passive radar | Done |
| PR-12 | Contact registry + wreck destroy | Done |
| PR-13 | Patrol / defensive targeting fix | Done |
| PR-14 | Interception + in-transit combat | Done |

## Out of scope (still)

- Pixel geography / zone authoring tool
- Tactical combat map load
- Full Continue Game save
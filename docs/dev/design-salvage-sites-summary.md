# Salvage Sites — Implementation Summary

**Last updated:** June 2026  
**User guide:** [Salvage](../salvage.md) · [Missions & AI](../missions-and-ai.md) · [Tuning](../tuning.md)

The full PR-1–7 design spec remains in [`design-salvage-sites.md`](./design-salvage-sites.md) (archive). This page describes **what is in the codebase today**.

---

## Shipped behavior

| Feature | Implementation |
|---------|----------------|
| Wreck on destroy | `CreateSalvageSite` — all combat destroys (when `bSalvageSitesEnabled`) |
| Combat-known | `KnownFactions` + `RegisterCombatKnownSalvage` — no radar to dispatch |
| Depletion | Resources hit zero → `RemoveSalvageSite` — site removed from map |
| Expiry | `SalvageWreckExpiryDays` → `ProcessSalvageSiteExpiry` |
| Salvage missions | Transport/Support/Scout; on-station extraction window |
| Crew at wreck | `ProcessCrewOnVehicleDestruction` — KIA dice + MIA at site |
| MIA rescue | `RescueMIAsFromWreck` (own faction salvage) |
| MIA → POW | `ProcessMIAsOnOpposingSalvage` (AI dice) |
| Contested salvage | `BeginSalvageContest` → pause clock → `ResolveSalvageContest` |
| Fog UI | `UStrategySalvageMapWidget`, `BuildSalvageMapMarkers`, stale tooltips |
| QA save | Site map + intel schema v3 — not full campaign |

## Discovery API (current)

- **Canonical:** `AddDiscoveredSite(Faction, UStrategySiteDefinition*, EDiscoveryReason)`
- **Removed:** `AddDiscoveredSiteAtLocation`, location-based overload, `DiscoveringFaction` field

Visibility uses `DiscoveredSitesHuman` / `DiscoveredSitesEnemy` and salvage `KnownFactions`.

## Base expansion interaction

New bases use **vehicle-guarded expansion** — not `TryBuildBaseOnSite` (removed). Salvage and expansion can contest the same map space; expansion uses live combat at sites, not salvage-style contest UI.

## Events

`OnSalvageSiteCreated`, `OnSalvageSiteRemoved`, `OnSalvageContestStarted` on `UStrategyEventDispatcher`.

## Out of scope (still)

- Tactical salvage battle map inside plugin
- Pixel geography filter for wreck placement
- Full Continue Game save

## PR map (historical)

| PR | Focus | Status |
|----|-------|--------|
| PR-1 | Foundation, `KnownFactions`, guards | Done |
| PR-2 | Debug HUD salvage display | Done |
| PR-3 | Event dispatcher | Done |
| PR-4 | Site-map QA save | Done |
| PR-5 | Player fog map widget | Done |
| PR-6 | Salvage mission MVP | Done |
| PR-6b | Contested salvage hook | Done |
| PR-7 | AI salvage scoring | Done |
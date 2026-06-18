## Design Document Review: Salvage Sites System (Revision 3 — Final)

### Summary

**Approved for PR-1–7 implementation.** Revision 4 addresses the final minor issue (stale mission state on load). All review rounds closed.

---

### Issue 1: PR-4 `LoadCampaign` recipe does not clear stale mission/vehicle runtime state
- **Severity**: minor
- **Section**: Persistence — Post-`LoadCampaign` runtime state / `LoadCampaign` steps (ordered)
- **Description**: The post-load state table specifies **Active missions → ✗ None** and **Parked / in-flight vehicles → ✗ None** after PR-4 load. The recommended QA workflow is same-session **play → `SaveCampaign` → `LoadCampaign` → inspect map** (§Persistence, §Recommended dev workflow). The explicit load recipe clears sites and discovery lists and may call `ResetAllBases()`, but it does **not** clear `UMissionManagerSubsystem::ActiveMissions` or in-flight/docked vehicle references. Verified: no `ActiveMissions.Empty()` or mission-reset API exists in the codebase today; `ResetSimulation()` also does not clear missions. After a same-session load, stale missions from the pre-save play session can remain, contradicting the post-load table and potentially affecting `CollectSitesTargetedByActiveMissions` during site QA.
- **Suggestion**: Add to PR-4 `LoadCampaign` step 2 (or a new step 2b): clear `ActiveMissions` (destroy/cancel each `UMissionGroup`, dock or destroy orphaned vehicles, log `[SAVE] Cleared N stale missions from pre-load session`). Alternatively, revise the post-load table row to *"Stale missions may persist from pre-load session — site-map QA only; not restored from save"* and add a PR-4 DoD note that mission count is not asserted. Prefer explicit clear to match the table.
- **Status**: addressed
- **Response**: Added `LoadCampaign` step 3: `UMissionManagerSubsystem::ClearRuntimeMissionStateForSiteMapLoad()` — detaches vehicles/soldiers, empties `ActiveMissions`, logs `[SAVE] Cleared N stale mission(s)`. Renumbered subsequent steps; updated sequence diagram, API table, Key Decisions, PR-4 tasks/DoD (`ActiveMissions.Num() == 0`, `CollectSitesTargetedByActiveMissions` empty after load). Documented that `ResetSimulation()` also omits mission clear.

---

### Strengths (revision 3)

- **All 9 re-review issues closed:** PR-4 scope is honest (site-map QA/dev tooling, not Continue Game); `BuiltOnSite` linkage correctly moved out of scope; post-load state table and dev workflow are explicit.
- **`SiteId` end-to-end:** Renamed from salvage-only ID; assignment table covers `GenerateInitialSites`, `CreateSalvageSite`, `DeserializeAllSites`; PR-1 and PR-4 DoD verify stable GUID round-trip including Command Center `bHasBeenUsed` sites.
- **Load recipe is implementable:** Six ordered steps; explicitly forbids `ResetSimulation()`; documents why; optional `ResetAllBases()` with warning for same-session misuse.
- **Discovery fix is complete on paper:** Pointer overload canonical; location overload deprecated with `DeprecatedFunction` meta, SiteType-mismatch warning, and PR-1 grep DoD.
- **PR dependency hygiene:** `CanSalvageSite` PR-1/PR-6 split documented; `FindSiteAtLocation` default change tasked in PR-1; feature-flag guard uses verified `GetGameInstance()->GetSubsystem<UStrategyCampaignSubsystem>()` pattern.
- **PR plan remains shippable:** Seven PRs with Definition of Done; persistence before player map; salvage missions deferred with credible PR-6 sequence diagram and acceptance checklist.
- **Field-level accuracy:** `DiscoveringFaction` documented as legacy/unused; changelog and cross-links between design, summary, and review files support handoff.

---

## Revision History

| Date | Reviewer action |
|------|-----------------|
| 2026-06-17 | **Rev 1:** 19 issues filed. Verdict: needs revision. |
| 2026-06-17 | **Rev 2:** All 19 rev-1 issues addressed in `design-salvage-sites.md`. |
| 2026-06-17 | **Rev 2 re-review:** 9 issues filed (4 major, 3 minor, 2 nit). Verdict: approve with minor revisions. |
| 2026-06-17 | **Rev 3:** All 9 re-review issues addressed in `design-salvage-sites.md`. |
| 2026-06-17 | **Rev 3 final re-review:** 9/9 rev-2 issues verified addressed. **1 new minor issue** (stale mission state on load). Verdict: **approved for PR-1–7**; resolve open issue in PR-4. |
| 2026-06-17 | **Rev 4:** Stale mission issue addressed. Verdict: **fully approved** — all review issues closed. |

---

## Revision Summary (Round 4)

| Date | Action |
|------|--------|
| 2026-06-17 | **Rev 4 complete.** Final minor issue (stale `ActiveMissions` on same-session `LoadCampaign`) set to **addressed**. Design doc revision 4 adds `ClearRuntimeMissionStateForSiteMapLoad` as explicit load step 3. **All review issues across rounds 1–4 are closed.** |

---

## Prior Issues — Resolution Status (do not re-open)

| Round | Issues | Status |
|-------|--------|--------|
| Rev 1 | 19 issues (discovery, persistence, PR plan, API contracts, etc.) | All **addressed** in rev 2 |
| Rev 2 re-review | Issues 1–9 (scope table, load framing, `SiteId`, load recipe, deprecated overload, `CanSalvageSite` split, `FindSiteAtLocation`, accessor snippet, `DiscoveringFaction`) | All **addressed** in rev 3 |
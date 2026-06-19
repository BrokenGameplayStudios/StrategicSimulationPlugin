# Salvage

## Wreck creation

When a vehicle is destroyed in combat, **`CreateSalvageSite`** places a **SalvageSite** at the wreck with resources seeded from the vehicle's build cost.

- **Combat-known:** engagement participants are added to `KnownFactions` and discovery lists immediately — no radar required
- Other factions discover via radar, recon, or visiting
- Wrecks expire after **`SalvageWreckExpiryDays`** (`ProcessSalvageSiteExpiry`)
- Depleted wrecks are **removed** from the map (`ESalvageSiteState::Removed`)

Requires **`bSalvageSitesEnabled`** on campaign.

## Crew at wrecks

On destruction, **`ProcessCrewOnVehicleDestruction`** runs:

- `VehicleCrashDeathChance` may mark soldiers **KIA**
- Survivors become **MIA** at the wreck (`MIASoldiers` on site)
- Vehicles cannot launch without ≥1 soldier (empty wrecks avoided)

| API | Purpose |
|-----|---------|
| `RescueMIAsFromWreck` | Owning faction salvage returns MIAs to base |
| `ProcessMIAsOnOpposingSalvage` | Opposing salvage: AI dice for MIA → POW |

## Fog of war

Players only see wrecks their faction knows:

- Combat-known (fought there)
- Discovered via radar / recon
- Stale intel may show outdated resources until refreshed

**UStrategySalvageMapWidget** or **`BuildSalvageMapMarkers`** for the player layer. Tooltips via **`FormatSalvageTooltipText`** append *"Intel stale"* when appropriate.

## Salvage missions

Transport, Support, and Scout vehicles fly to the wreck, loiter **`SalvageOnStationHours`**, and extract hourly at **`SalvageEfficiencyMultiplier`**. Gunship/Heavy cannot salvage.

Disable scheduling with **`bSalvageMissionsEnabled`**. Cap concurrent fleets with **`MaxActiveSalvageMissionsPerFaction`**.

## AI salvage

**`EvaluateAISalvageScheduling`** scores wrecks by value and distance. Enemy wrecks score higher. Tune thresholds on initializer/campaign (see [Tuning](tuning.md)).

## Contested salvage

If Human and Enemy both have active salvage missions at the same wreck:

1. **`BeginSalvageContest`** → **`OnSalvageContestStarted`** with `FSalvageContestForceSnapshot` per faction
2. Strategic clock **pauses** (`PauseStrategicClock`)
3. Your tactical layer resolves combat (external to plugin)
4. **`ResolveSalvageContest(ESalvageContestOutcome)`** — winner continues, loser **`AbortSalvageMission`**, clock resumes

Outcomes: `FactionAWins`, `FactionBWins`, `FactionAAborts`, `FactionBAborts`, `MutualRetreat`.

## Discovery API

Sites are registered with **`AddDiscoveredSite(Faction, UStrategySiteDefinition*, Reason)`**.

**`AddDiscoveredSiteAtLocation` was removed** — always pass the site pointer.

## Save / load (QA)

| API | Effect |
|-----|--------|
| `SaveCampaign(SlotIndex)` | Slots `SaveSlot01`…`SaveSlot10` |
| `LoadCampaign(SlotIndex)` | Restores sites + intel only |
| `GetAllSaveMetadata()` | Scan slots 1–10 |

**Saved:** site map, discovery flags, resource pools, salvage state, intel snapshots (schema v3).  
**Not saved:** bases, facilities, vehicles, soldiers, missions, rosters.

After load there are **no bases** — not runnable until a fresh **`StartSimulation`** in a new session. Use load to inspect the debug map, not to continue a campaign.

Workflow: play → save → load → inspect debug map. For a new playable session: **`ResetSimulation`** then **`StartSimulation`**.
# Salvage

## Wreck creation

When a vehicle is destroyed (combat or future crash paths), a **SalvageSite** is placed at the wreck location with resources derived from the vehicle. Participants in the fight know the wreck immediately (**combat-known**); other factions must discover it via radar or visit.

Wrecks expire after **`SalvageWreckExpiryDays`** if not salvaged.

## Fog of war

Players only see wrecks their faction knows:

- Discovered via radar / recon
- Combat-known (fought at the wreck)
- Intel snapshot may show **stale** resources until refreshed

Use **UStrategySalvageMapWidget** or **`BuildSalvageMapMarkers`** for the player map layer. Requires `bSalvageSitesEnabled` and `bSitesPersistenceEnabled`.

## Salvage missions

Transport, Support, and Scout vehicles fly to the wreck, loiter for **`SalvageOnStationHours`**, and extract resources hourly into the faction pool. Gunship/Heavy cannot salvage.

Disable new salvage with **`bSalvageMissionsEnabled`**.

## AI salvage

AI scores wrecks by resource value and distance. Enemy wrecks score higher. Tune caps and thresholds on the initializer (see [Tuning](tuning.md)).

## Contested salvage

If Human and Enemy both have active salvage missions at the same wreck:

1. **`OnSalvageContestStarted`** fires with force snapshots
2. Strategic clock **pauses**
3. Your tactical layer resolves combat
4. Call **`ResolveSalvageContest(Outcome)`** — winner continues, loser returns home, clock resumes

## Save / load (QA)

**`SaveCampaign`** / **`LoadCampaign`** persist the site map (nodes, wrecks, discovery, intel). This is **not** a playable Continue Game — bases, vehicles, and missions are not restored.

Workflow: play → save → load → inspect debug map. For a new playable session use **`ResetSimulation`** then **`StartSimulation`**, not load-then-start.
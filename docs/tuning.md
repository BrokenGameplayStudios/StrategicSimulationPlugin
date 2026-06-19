# Tuning guide

Gameplay knobs are split between **AStrategyGameInitializer** (level defaults, copied on `BeginPlay`) and **UStrategyCampaignSubsystem** (runtime source of truth). Edit the initializer for level design; edit campaign in PIE to experiment.

**Name mapping:** `MaxFactionBases` on the initializer → `MaxAIBases` on campaign and `UAIControllerSubsystem::MaxBases`.

## Map generation

| Property | Default | Where | Effect |
|----------|---------|-------|--------|
| Number Of Strategic Sites | 25 | Both | Potential base / resource nodes |
| Minimum Distance Between Sites | 350 | Both | Minimum spacing |
| Logical Map Width / Height | 1920 × 1080 | Both | Mission and UI coordinates |
| Map Border Padding | 100 | Both | Keep sites off edges |
| Min Distance Between Factions | 700 | Both | Starting base separation |
| Max Faction Bases / MaxAIBases | 4 | Init / Campaign | Expansion cap per faction |

Generated sites receive **mineable resources** for facility extraction.

## AI simulation

| Property | Default | Where | Effect |
|----------|---------|-------|--------|
| Start With Human AI | true | Init | Daily AI for Human |
| Start With Enemy AI | true | Init | Daily AI for Enemy |
| Stagger Mission Launches | true | Both | Spread departures across 24h game day |
| Offensive Missions Start Day | 5 | Both | First day Gunship/Heavy may attack enemy bases |
| Min Offense To Engage | 10 | Both | Minimum offensive rating for vehicular combat |

Runtime: `UAIControllerSubsystem::SetAIEnabled`, `IsSimulatingHumanAI`, `IsSimulatingEnemyAI`.

## Starting resources

**HumanStartingStockpile** and **EnemyStartingStockpile** on the initializer (Money, Metals, Biologicals, Chemicals, Exotic Material, Research Points).

## Salvage

| Property | Default | Where | Effect |
|----------|---------|-------|--------|
| bSalvageSitesEnabled | true | Campaign | Create wrecks on vehicle destruction |
| bSalvageMissionsEnabled | true | Campaign | Schedule salvage missions |
| bSitesPersistenceEnabled | true | Campaign | Include sites in QA save |
| SalvageWreckExpiryDays | 7 | Both | Days before unclaimed wrecks removed |
| SalvageOnStationHours | 4.0 | Campaign | On-station extraction window |
| SalvageEfficiencyMultiplier | 4.0 | Both | Hourly extraction rate scale |
| MaxActiveSalvageMissionsPerFaction | 2 | Both | Concurrent salvage fleets |
| MinSalvageScoreThreshold | 15.0 | Both | AI distance/value gate |
| SalvageDeclineAfterWinChance | — | Both | AI skips wreck after winning combat there |
| LoserSalvageScoreMultiplier | — | Both | AI bias to enemy wrecks |
| VehicleCrashDeathChance | 0.25 | Both | KIA vs MIA on wreck (survivors = MIA) |
| OpposingSalvageMIAPOWChance | — | Both | AI opposing salvage: MIA → POW dice |

## Expansion

| Property | Default | Where | Effect |
|----------|---------|-------|--------|
| MaxActiveExpansionMissionsPerFaction | 1 | Both | Concurrent guard missions per faction |
| bBaseExpansionRequiresVehicleGuard | true | Both | Require vehicle mission (disable for testing only) |

Forward-base Command Centers respect **`BuildTimeDays`** on the facility definition. **Starting** Command Centers at game start are still instant.

## Radar & intel

| Property | Default | Where | Effect |
|----------|---------|-------|--------|
| bBasePassiveRadarEnabled | true | Campaign | CC passive threat tracking |
| BaseRadarRangePixels | 512 | Both | Passive radar reach |
| BaseRadarPingIntervalHours | 1.0 | Both | Time between base sweeps |
| RadarContactExpiryHours | 6.0 | Campaign | Contact fades without refresh |
| bAIReactiveInterceptionEnabled | true | Campaign | AI gunships on inbound contacts |
| bNotifyPlayerOfEnemyRadarContacts | true | Campaign | Toast when enemy detects your vehicles |
| bShowEnemyRadarContactsOnDebugMap | true | Campaign | Magenta diamonds on debug HUD |
| bRadarLOSEnabled | true | Both | Mountains block radar |
| bStaleIntelEnabled | true | Both | UI uses last-known site snapshots |
| bEngageInboundThreatsWhileInTransit | true | Both | Combat vehicles engage inbound threats en route |
| RadarBlockerZones | — | Init | Mountain LOS zones (circles/rects) |

## POW / casualties

| Property | Where | Status |
|----------|-------|--------|
| VehicleCrashDeathChance | Both | **Live** — wreck KIA/MIA |
| OpposingSalvageMIAPOWChance | Both | **Live** — AI opposing salvage |
| POWCaptureChanceOnVictory, KIAChanceOnVictory, EnemyKIAChanceOnDefeat, etc. | Campaign | **Defined but not used** in combat resolution yet |

Containment/Autopsy facilities can be built; per-base POW storage and daily processing are **scaffolding only**. Live POW path today: salvage `CaptureAsPOW`, faction `GetPOWRoster`.

## Radar contact map widget

| Property | Default | Effect |
|----------|---------|--------|
| bShowOpposingFactionContacts | true | Draw Human (cyan) and Enemy (magenta) contacts |
| bAllowPlayerClickToIntercept | false | Left-click dispatch; off during AI spectate |
| ViewerFaction | Human | Whose contacts trigger discovery toasts |

## Debug

| Property | Default | Where | Effect |
|----------|---------|-------|--------|
| bVerboseLogging | false | Both | Extra subsystem logs |
| bVerboseFacilityLogging | false | Campaign | Per-facility day ticks |
| bAllowDebugExecCommands | true | Init → Campaign | Debug HUD console commands |
| Show Unlock Messages | true | Init | `[UNLOCK]` console output |
| bShowFacilityTicks | false | Init | Facility daily simulation logs |

See [Debug tools](debug-tools.md) for HUD commands and log filters.
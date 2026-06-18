# Tuning guide

Most gameplay knobs live on **AStrategyGameInitializer** and are copied to **UStrategyCampaignSubsystem** when the level starts. You can also edit campaign properties at runtime in the editor for testing.

## Map generation

| Property | Default | Effect |
|----------|---------|--------|
| Number Of Strategic Sites | 25 | Potential base / resource nodes placed on the map |
| Minimum Distance Between Sites | 350 | Minimum spacing between nodes |
| Logical Map Width / Height | 1920 × 1080 | Coordinate space for missions and UI |
| Map Border Padding | 100 | Keep sites away from map edges |
| Min Distance Between Factions | 700 | Separation of Human and Enemy starting bases |
| Max Faction Bases | 4 | Expansion cap per faction |

Generated sites receive **mineable resources** (metals, chemicals, etc.) for facility extraction.

## AI simulation

| Property | Default | Effect |
|----------|---------|--------|
| Start With Human AI | true | Daily AI for Human faction |
| Start With Enemy AI | true | Daily AI for Enemy faction |
| Stagger Mission Launches | true | Spread departures across the 24h game day |
| Offensive Missions Start Day | 5 | First day Gunship/Heavy may attack enemy bases |
| Min Offense To Engage | 10 | Minimum offensive rating to start vehicular combat |

## Starting resources

Set **HumanStartingStockpile** and **EnemyStartingStockpile** (Money, Metals, Biologicals, Chemicals, Exotic Material, Research Points).

## Salvage

| Property | Default | Effect |
|----------|---------|--------|
| bSalvageSitesEnabled | true | Create wrecks when vehicles are destroyed |
| bSalvageMissionsEnabled | true | AI/player can schedule salvage missions |
| SalvageWreckExpiryDays | 7 | Days before unclaimed wrecks are removed |
| SalvageOnStationHours | 4.0 | On-station extraction window |
| SalvageEfficiencyMultiplier | 4.0 | Hourly extraction rate scale |
| MaxActiveSalvageMissionsPerFaction | 2 | Concurrent salvage fleets |
| MinSalvageScoreThreshold | 15.0 | AI distance/value gate for salvage |

## Radar & intel

| Property | Default | Effect |
|----------|---------|--------|
| bBasePassiveRadarEnabled | true | Command Center passive threat tracking |
| BaseRadarRangePixels | 512 | Passive radar reach |
| BaseRadarPingIntervalHours | 1.0 | Time between base radar sweeps |
| RadarContactExpiryHours | 6.0 | Contact fades if not refreshed |
| bAIReactiveInterceptionEnabled | true | AI launches gunships on inbound contacts |
| bNotifyPlayerOfEnemyRadarContacts | true | Toast when enemy detects your vehicles |
| bRadarLOSEnabled | true | Mountains block radar line of sight |
| bStaleIntelEnabled | true | UI shows last-known site data when stale |
| bEngageInboundThreatsWhileInTransit | true | Combat vehicles engage inbound threats en route |

## POW / casualties (campaign)

| Property | Default | Effect |
|----------|---------|--------|
| POWCaptureChanceOnVictory | varies | POW on combat victory |
| KIAChanceOnVictory | varies | KIA on combat victory |
| VehicleCrashDeathChance | 0.25 | Crash death vs MIA on vehicle destruction |

## Debug

| Property | Default | Effect |
|----------|---------|--------|
| bVerboseLogging | false | Extra subsystem logs |
| bAllowDebugExecCommands | true on initializer | Debug HUD console commands |
| Show Unlock Messages | true | `[UNLOCK]` console output |

See [Debug tools](debug-tools.md) for HUD commands and log filters.
# Glossary

| Term | Meaning |
|------|---------|
| **Logical map** | 2D coordinate space (default 1920×1080) for vehicle positions and UI — not the 3D level |
| **Command Center** | Starting facility; passive radar when operational |
| **Site** | Map node: potential base, resource node, salvage wreck, or POI |
| **Catalog** | Database array listing all definitions of one type (Items, Facilities, …) |
| **Production slot** | Concurrent queue job on a facility (train, build, research, fabricate) |
| **Capacity** | Stationed units a facility supports (soldiers in barracks, vehicles in hangar) |
| **Strategy tech** | `UStrategyTechDefinition` — item-tech node unlocked by research |
| **Combat-known** | Faction knows a wreck because they fought there — no radar needed |
| **Discovered site** | Faction has the site on their map (`DiscoveredSitesHuman/Enemy`) |
| **Stale intel** | UI shows last-known site resources/base state until refreshed |
| **Live movement** | Vehicle moves on the map in real time during a mission |
| **On-station** | Vehicle loitering at target (recon survey, salvage, expansion guard) |
| **Radar contact** | Passive radar track: position, heading, inbound flag, expiry |
| **Entry point** | Where a contact first entered radar range — intercept/patrol target |
| **Inbound threat** | Enemy vehicle moving toward friendly bases |
| **Reactive interception** | Gunship launch on inbound contact (needs crew at base) |
| **Mission-ready soldier** | At origin base; not KIA/MIA/POW; not on mission or vehicle |
| **Base expansion** | Vehicle mission to claim a site and guard CC construction |
| **Expansion guard** | Vehicle on-station until CC operational; loss cancels build |
| **Spoke patrol** | Recon pattern along rotating compass spokes from base |
| **Hot spoke** | Spoke direction prioritized after inbound threat |
| **Contested salvage** | Two factions salvaging same wreck — clock pauses for tactical resolve |
| **Strategic clock pause** | Time freeze during salvage contest — separate from user pause |
| **Range budget** | Max round-trip distance a vehicle can fly on one mission |
| **Schema version** | Save compatibility: v2 sites, v3 + intel snapshots |
| **Spectate mode** | AI sim on, radar click-intercept off, both factions' contacts visible |
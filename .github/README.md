# AzerothCore — Ashbringer Fork


[![Contributor Covenant](https://img.shields.io/badge/Contributor%20Covenant-2.1-4baaaa.svg)](CODE_OF_CONDUCT.md)
[![CodeFactor](https://www.codefactor.io/repository/github/azerothcore/azerothcore-wotlk/badge)](https://www.codefactor.io/repository/github/azerothcore/azerothcore-wotlk)
[![Discord](https://img.shields.io/discord/217589275766685707?logo=discord&logoColor=white)](https://discord.gg/gkt4y2x "Our community hub on Discord")
=======
Custom AzerothCore WotLK 3.3.5a build powering the **Ashbringer** private server.
Based on AzerothCore `bcraft-loc` branch.


---

## Core Modifications

These are direct edits to AzerothCore source files. If you are merging upstream changes,
these files need to be checked for conflicts.

---

### 1. Naxx40 — Difficulty Downscale Fix
**File:** `src/server/game/DataStores/DBCStores.cpp`
**Function:** `GetDownscaledMapDifficultyData` (~line 803)

**Problem:** When difficulty `2` (RAID_DIFFICULTY_10MAN_HEROIC) is not found in DBC for
map 533 (Naxxramas), the function silently downgrades to difficulty `0`, preventing Naxx40
from loading correctly.

**Fix:** Added a special case at the top of the function — if `mapId == 533` and
`difficulty == RAID_DIFFICULTY_10MAN_HEROIC`, return the 40-player difficulty entry directly.

---

### 2. Naxx40 — Script Double Registration Fix
**File:** `src/server/scripts/Northrend/northrend_script_loader.cpp`
**Lines:** ~62 and ~228

**Problem:** Base AzerothCore registers `AddSC_instance_naxxramas()` for map 533.
`mod-vanilla-naxxramas` also registers the same script, causing both to become invalid
and the instance to fail.

**Fix:** Commented out the base AC registration calls on lines 62 and 228 so only
`mod-vanilla-naxxramas` owns the Naxx script.

---

### 3. Ragnaros — AutoBalance Tuning
**File:** `src/server/scripts/EasternKingdoms/Molten_Core/boss_ragnaros.cpp`
*(or relevant autobalance override file)*

**Problem:** AutoBalance default scaling made Ragnaros trivial or unplayably hard
depending on group size.

**Fix:** Custom HP/damage scaling adjustments to keep Ragnaros engaging for small
groups while still requiring meaningful coordination. See inline comments in the file.

---

### 4. Item Affixes — Spell Whitelist / Blacklist Edits
**File:** Related to `mod-item-affixes` spell configuration

**Problem:** Default affix spell pool included spells that were either broken,
too powerful, or incompatible with vanilla-era progression.

**Fix:** Edited spell eligibility list to exclude problematic spell IDs and ensure
affixes feel appropriate for the progression phase. See inline comments.

---

## Required SQL (Characters DB)

Run after fresh install or migration:

```sql
-- mod-xp-dynamic tables
source modules/mod-xp-dynamic/data/sql/db_characters/base/account_tables.sql

-- Naxx40 map/difficulty entries
INSERT INTO mapdifficulty_dbc (ID, MapID, Difficulty, Message_Lang_enUS, Message_Lang_Mask, RaidDuration, MaxPlayers, Difficultystring)
VALUES ([id], 533, 2, 'You must be level 60 and in a raid group to enter.', 16712190, 604800, 40, 'RAID_DIFFICULTY_40PLAYER');

INSERT INTO map_dbc (ID, ExpansionID) VALUES (533, 0);
```

---

## Modules

All modules live in `Z:\ac\modules\` (Windows build machine) or equivalent on Linux.

| Module | Purpose |
|--------|---------|
| `mod-xp-dynamic` | Dynamic XP/Gold/Rep rates, Skill XP rolls, Alt Progression Bonus, ARAC class fixes |
| `mod-001v1-arena` | 1v1 arena support |
| `mod-01-progression-system` | Chromie-style era gating (phase progression) |
| `mod-02-progression` | Progression system support layer |
| `mod-03-individual-progression` | Per-player individual progression tracking and dungeon access |
| `mod-ah-bot-plus` | Auction house bot to keep AH populated on low-pop server |
| `mod-autobalance` | Dynamic creature scaling based on group size |
| `mod-autofish` | Auto fishing bot / utility |
| `mod-boss-announcer` | Announces boss kills server-wide |
| `mod-breaking-news-override` | Custom server news/broadcast override |
| `mod-guildhouse` | Player guild housing system |
| `mod-instance-reset` | Custom instance lockout/reset controls |
| `mod-instance-tools` | Instance management GM tools |
| `mod-item-affixes` | Random item affixes — adds roguelike modifier rolls to loot |
| `mod-multibot-bridge` | Bridge for managing multiple playerbot accounts |
| `mod-mythic-plus` | Mythic+ style dungeon difficulty system |
| `mod-nemesis-system` | Player nemesis / rivalry tracking system |
| `mod-npc-enchanter` | Custom NPC enchanter for convenience |
| `mod-npc-gambler` | Gambling NPC |
| `mod-player-bot-level-brackets` | Forces playerbots to stay within level brackets |
| `mod-playerbots` | ARAC playerbot system (any race any class AI bots) |
| `mod-playerbots-characters` | LLM-powered bot characters via OpenRouter API |
| `mod-profession-experience` | Bonus XP for profession leveling |
| `mod-quest-loot-party` | Quest loot shared to party members |
| `mod-reagent-bank-account` | Account-wide reagent bank |
| `mod-recruit-friend` | Recruit-a-friend XP bonus system |
| `mod-solocraft` | Solo dungeon difficulty scaling |
| `mod-solo-lfg` | LFG system for solo players / small groups |
| `mod-transmog` | Transmogrification system |
| `mod-travelers-recall` | Extra hearthstone / recall system |
| `mod-vanilla-naxxramas` | Naxx40 — restores 40-player Naxxramas for vanilla era progression |

---

## Notes for Maintainers

- **mod-playerbots-characters** uses OpenRouter as LLM backend. Config: `PBC.BaseUrl = https://openrouter.ai/api/v1` (do NOT append `/chat/completions` — the module adds that itself). Model: `deepseek/deepseek-v4-flash`.
- **mod-vanilla-naxxramas** requires the two core edits above (DBCStores and script loader) to function correctly. Without them Naxx40 will either not load or crash on entry.
- **mod-item-affixes** spell list was manually curated — do not overwrite with upstream defaults without reviewing the affix spell edits documented above.
- **mod-autobalance** Ragnaros tuning — see core edit #3 above.
- **Client-side:** Players need a custom `patch-n.mpq` that adds difficulty 2 entries to client-side `MapDifficulty.dbc` for map 533. Without it the skull difficulty icon does not display on the instance portal even if the server is configured correctly.

---

## Server Info

- **Server name:** Ashbringer
- **Expansion:** WotLK 3.3.5a (Chromie progression — vanilla phase at launch)
- **Build machine:** Windows, Z:\ac
- **Server path:** S:\apps\WoWServAsh
- **Characters DB:** `azc_characters_ashbringer`
- **World DB:** `azc_world_ashbringer`
- **Dedicated server hardware:** Firebat A6 Mini PC (Ryzen 7 7735HS, 16GB LPDDR5) — headless, NSSM services
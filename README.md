# Ashbringer — AzerothCore Fork

A WotLK 3.3.5a AzerothCore fork built on top of [Playerbots](https://github.com/liyunfan1223/mod-playerbots) and
~50 community modules, with a handful of custom systems layered on top. This README tracks what's actually
different from a stock AzerothCore+modules checkout — not a full module inventory, just the things we built,
forked, or meaningfully changed.

## What we've changed

### Item Affixes — class-skill & talent affixes

[`mod-item-affixes`](modules/mod-item-affixes/) adds a Diablo-style reroll system to gear, but the piece specific
to this fork is that affix rolls aren't limited to stats: an option can also be a **class-skill affix** (a
`SpellMod` bonus to one of the player's own abilities, e.g. "+10% Fireball damage") or a **talent affix** (passive
bonus ranks in a talent from the player's own tree). Spec is auto-detected from the player's dominant talent tree,
role from class, and main stat from class, so it works without any addon UI on a standard server — the addon
selectors (type/spec/role/main-stat pickers) exist for servers with custom classes or non-standard talent point
totals, and are off by default here.

See [`modules/mod-item-affixes/conf/mod_item_affixes.conf.dist`](modules/mod-item-affixes/conf/mod_item_affixes.conf.dist)
for the full tuning surface — quality-tier slot/option/reroll counts, stat budgets, the class-affix roll chance,
crit rolls, and Imprints (extractable class-affix runes).

### Individual Progression — pets, on top of Grimfeather's fork

[`mod-individual-progression`](modules/mod-individual-progression/) is tracked from
[Grimfeather's fork](https://github.com/Grimfeather/mod-individual-progression) rather than the original
ZhengPeiRu21 upstream, specifically for its pet trainer work: full Hunter and Warlock pet trainer content
(`data/sql/world/base/hunter_pet_trainers.sql`, `warlock_pet_trainers.sql`) and pet-related playerbot fixes
(playerbot pet nerfs, Warlock pet stat corrections at progression level caps) that the base module doesn't have.
Everything else about the progression-gating system (item/spell/zone unlocks tied to progression phase) is
inherited as-is from that fork.

### Our own additions

Built from scratch for this fork, not adapted from an existing module:

- **[`mod-custom-races`](modules/mod-custom-races/)** — adds Goblin (Horde, race 9 — already a real client race
  slot, just NPC-only in stock 3.3.5a) and High Elf (Alliance, race 22 — a new race ID reskinning Blood Elf's
  model/skeleton) as playable races. Additive only: every stock race, class, and item stays untouched. See the
  module's own README for the full status of what's done per race, what's deliberately out of scope (e.g. Goblin's
  own racial ability, which would need new spell content rather than reused data), and the client MPQ packaging
  process.
- **Azeroth Vocations** (`src/server/game/Vocations/` + [`mod-azeroth-vocations`](modules/mod-azeroth-vocations/))
  — an in-game class-switching system: a Class Master gossip flow lets a character archive their current class
  and either restore a previously-archived one or start a new class fresh at level 1, gated by a level-based
  slot curve (extra slots purchasable with gold). The gossip/command layer lives in the module; the actual
  archive/restore logic (`CoreVocationService`) is compiled into core. Both the module's gossip flow and the core
  service default to **disabled** — see
  [`modules/mod-azeroth-vocations/conf/mod_azeroth_vocations.conf.dist`](modules/mod-azeroth-vocations/conf/mod_azeroth_vocations.conf.dist).

## Everything else

The remaining ~50 modules under [`modules/`](modules/) are used as published upstream, unmodified beyond normal
version bumps. See each module's own README for what it does.

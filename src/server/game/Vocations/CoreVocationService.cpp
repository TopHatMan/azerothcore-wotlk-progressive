#include "CoreVocationService.h"

#include "Bag.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "SpellMgr.h"
#include "StringFormat.h"
#include "World.h"

#include <set>

namespace
{
// A spell counts as belonging to a specific class only if every one of its
// SkillLineAbility.dbc entries restricts ClassMask to particular class(es).
// Professions, First Aid, Cooking, languages, etc. have ClassMask = 0
// (learnable by any class) and must never be archived/wiped on a class
// switch -- they aren't part of what makes a class a class. A spell with no
// SkillLineAbility entry at all (can't happen for anything actually in a
// player's spellbook via normal means, but conservatively) is treated the
// same way: never touched, since there's no data to attribute it to a class.
bool IsClassSpecificSpell(uint32 spellId)
{
    auto bounds = sSpellMgr->GetSkillLineAbilityMapBounds(spellId);
    if (bounds.first == bounds.second)
        return false;

    for (auto itr = bounds.first; itr != bounds.second; ++itr)
        if (itr->second->ClassMask != 0)
            return true;

    return false;
}

// Where a starting-outfit item goes among the 19 equipment slots, or
// EQUIPMENT_SLOT_END if it isn't equipment at all (a starting reagent, food,
// etc.) -- those are not the Class Armory's concern, see design doc §4.2.
// Deliberately simpler than Player::FindEquipSlot: a brand new class profile
// starts with nothing else equipped, so there is no swap/dual-wield/Titan
// Grip conflict to resolve -- every inventory type has exactly one obvious
// slot to try.
uint8 GetOutfitEquipSlot(ItemTemplate const* itemTemplate)
{
    switch (itemTemplate->InventoryType)
    {
        case INVTYPE_HEAD:          return EQUIPMENT_SLOT_HEAD;
        case INVTYPE_NECK:          return EQUIPMENT_SLOT_NECK;
        case INVTYPE_SHOULDERS:     return EQUIPMENT_SLOT_SHOULDERS;
        case INVTYPE_BODY:          return EQUIPMENT_SLOT_BODY;
        case INVTYPE_CHEST:
        case INVTYPE_ROBE:          return EQUIPMENT_SLOT_CHEST;
        case INVTYPE_WAIST:         return EQUIPMENT_SLOT_WAIST;
        case INVTYPE_LEGS:          return EQUIPMENT_SLOT_LEGS;
        case INVTYPE_FEET:          return EQUIPMENT_SLOT_FEET;
        case INVTYPE_WRISTS:        return EQUIPMENT_SLOT_WRISTS;
        case INVTYPE_HANDS:         return EQUIPMENT_SLOT_HANDS;
        case INVTYPE_FINGER:        return EQUIPMENT_SLOT_FINGER1;
        case INVTYPE_TRINKET:       return EQUIPMENT_SLOT_TRINKET1;
        case INVTYPE_CLOAK:         return EQUIPMENT_SLOT_BACK;
        case INVTYPE_WEAPON:
        case INVTYPE_WEAPONMAINHAND:
        case INVTYPE_2HWEAPON:      return EQUIPMENT_SLOT_MAINHAND;
        case INVTYPE_SHIELD:
        case INVTYPE_WEAPONOFFHAND:
        case INVTYPE_HOLDABLE:      return EQUIPMENT_SLOT_OFFHAND;
        case INVTYPE_RANGED:
        case INVTYPE_RANGEDRIGHT:
        case INVTYPE_THROWN:
        case INVTYPE_RELIC:         return EQUIPMENT_SLOT_RANGED;
        case INVTYPE_TABARD:        return EQUIPMENT_SLOT_TABARD;
        default:                    return EQUIPMENT_SLOT_END; // not equippable
    }
}
} // namespace

CoreVocationService* CoreVocationService::instance()
{
    static CoreVocationService instance;
    return &instance;
}

bool CoreVocationService::IsServiceEnabled() const
{
    return _serviceEnabled;
}

void CoreVocationService::LoadConfig()
{
    _serviceEnabled = sConfigMgr->GetOption<bool>("AzerothVocations.Core.Enable", false);
    // See core-vocation-service-design.md §5a. Independent kill switch: with
    // this off (default), SwitchClass behaves exactly as before (forced
    // relog). Untested in production as of this writing -- keep off until
    // it's been exercised.
    _liveSwitchEnabled = sConfigMgr->GetOption<bool>("AzerothVocations.Core.LiveSwitchEnabled", false);

    for (uint8 classId = 1; classId < _classEnabled.size(); ++classId)
    {
        if (classId == 10) // unused class id, no ChrClasses.dbc entry (see SharedDefines.h)
            continue;

        _classEnabled[classId] = sConfigMgr->GetOption<bool>(
            Acore::StringFormat("AzerothVocations.Core.ClassEnabled.{}", classId), classId != CLASS_DEATH_KNIGHT);
    }
}

std::vector<ClassProfileSummary> CoreVocationService::GetClassProfiles(uint32 characterGuid)
{
    std::vector<ClassProfileSummary> out;
    QueryResult result = CharacterDatabase.Query(
        "SELECT class, level, xp, isActive FROM av_core_class_profile WHERE characterGuid = {}",
        characterGuid);
    if (!result)
        return out;

    do
    {
        Field* fields = result->Fetch();
        ClassProfileSummary summary;
        summary.classId = fields[0].Get<uint8>();
        summary.level = fields[1].Get<uint8>();
        summary.xp = fields[2].Get<uint32>();
        summary.isActive = fields[3].Get<uint8>() != 0;
        out.push_back(summary);
    } while (result->NextRow());

    return out;
}

bool CoreVocationService::HasClassProfile(uint32 characterGuid, uint8 classId)
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT 1 FROM av_core_class_profile WHERE characterGuid = {} AND class = {}",
        characterGuid, classId);
    return result.get() != nullptr;
}

std::vector<ArmoryItemSummary> CoreVocationService::GetArmorySummary(uint32 characterGuid, uint8 classId)
{
    std::vector<ArmoryItemSummary> out;
    QueryResult result = CharacterDatabase.Query(
        "SELECT a.equipSlot, a.itemGuid, i.itemEntry FROM av_core_class_armory_item a "
        "INNER JOIN item_instance i ON i.guid = a.itemGuid "
        "WHERE a.characterGuid = {} AND a.class = {}",
        characterGuid, classId);
    if (!result)
        return out;

    do
    {
        Field* fields = result->Fetch();
        ArmoryItemSummary summary;
        summary.equipSlot = fields[0].Get<uint8>();
        summary.itemGuid = fields[1].Get<uint32>();
        summary.itemEntry = fields[2].Get<uint32>();
        out.push_back(summary);
    } while (result->NextRow());

    return out;
}

ClassSwitchResult CoreVocationService::CanSwitchClass(Player* player, uint8 targetClass)
{
    if (!IsServiceEnabled())
        return ClassSwitchResult::ERR_SERVICE_DISABLED;

    if (targetClass >= _classEnabled.size() || !_classEnabled[targetClass])
        return ClassSwitchResult::ERR_TARGET_CLASS_DISABLED;

    if (player->getClass() == targetClass)
        return ClassSwitchResult::ERR_ALREADY_ACTIVE_CLASS;

    if (!player->IsAlive())
        return ClassSwitchResult::ERR_PLAYER_DEAD;

    if (player->IsInCombat())
        return ClassSwitchResult::ERR_PLAYER_IN_COMBAT;

    if (player->GetGroup())
        return ClassSwitchResult::ERR_PLAYER_GROUPED;

    if (player->GetTransport())
        return ClassSwitchResult::ERR_PLAYER_ON_TRANSPORT;

    if (player->IsFlying())
        return ClassSwitchResult::ERR_PLAYER_FLYING;

    if (Map* map = player->GetMap())
    {
        if (map->IsDungeon() || map->IsBattlegroundOrArena())
            return ClassSwitchResult::ERR_PLAYER_IN_INSTANCE;
    }

    // Unsafe inventory state: an open trade session touching this player.
    if (player->GetTradeData())
        return ClassSwitchResult::ERR_UNSAFE_INVENTORY_STATE;

    return ClassSwitchResult::SUCCESS;
}

ClassProfileSummary CoreVocationService::CreateLevelOneClassProfile(Player* player, uint8 classId)
{
    uint32 const characterGuid = player->GetGUID().GetCounter();

    ClassProfileSummary summary;
    summary.classId = classId;
    summary.level = 1;
    summary.xp = 0;
    summary.isActive = false;

    if (HasClassProfile(characterGuid, classId))
    {
        // Idempotent: return the existing summary rather than recreating it.
        for (ClassProfileSummary const& existing : GetClassProfiles(characterGuid))
        {
            if (existing.classId == classId)
                return existing;
        }
    }

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    // ON DUPLICATE KEY UPDATE class = class (no-op) rather than a bare INSERT:
    // the HasClassProfile() check above is a classic check-then-act gap (e.g.
    // a double-click on the gossip confirm button before the first request's
    // transaction commits) -- match the defensive pattern every other INSERT
    // in this file already uses instead of risking a 1062 duplicate-key error.
    trans->Append(
        "INSERT INTO av_core_class_profile (characterGuid, class, level, xp, isActive) "
        "VALUES ({}, {}, 1, 0, 0) ON DUPLICATE KEY UPDATE class = class",
        characterGuid, classId);

    // Seed the fresh spellbook/action bars from the same source a brand new
    // character of this race+class would get at creation. Talents and
    // glyphs stay empty -- a real level-1 character has no talent points
    // and no glyph slots yet either.
    uint32 const raceMask = 1u << (player->getRace() - 1);
    uint32 const classMask = 1u << (classId - 1);

    // The REAL starting spellbook (Charge, Shoot Bow, etc.) is not stored in
    // any table -- it comes from the same skill-line auto-learn mechanism
    // Player::LearnDefaultSkills()/learnSkillRewardedSpells() use at real
    // character creation: for every skill this race+class starts with
    // (playercreateinfo_skills), any SkillLineAbility.dbc entry for that
    // skill line with AcquireMethod == LEARNED_ON_SKILL_LEARN is granted
    // immediately, independent of skill value. playercreateinfo_spell_custom
    // (queried below too) is a separate, genuinely sparse table for
    // server-specific ADDITIONS on top of that -- it was never meant to be
    // the primary source, which is why a fresh class profile seeded from it
    // alone came out with an empty spellbook.
    std::set<uint32> seededSpellIds;
    std::set<uint32> seededSkillIds;
    if (QueryResult skillResult = WorldDatabase.Query(
            "SELECT skill, classMask FROM playercreateinfo_skills "
            "WHERE (raceMask = 0 OR raceMask & {}) AND (classMask = 0 OR classMask & {})",
            raceMask, classMask))
    {
        do
        {
            Field* fields = skillResult->Fetch();
            uint32 const skillId = fields[0].Get<uint32>();
            if (fields[1].Get<uint32>() != 0)
                seededSkillIds.insert(skillId);

            for (SkillLineAbilityEntry const* ability : GetSkillLineAbilitiesBySkillLine(skillId))
            {
                if (ability->AcquireMethod != SKILL_LINE_ABILITY_LEARNED_ON_SKILL_LEARN)
                    continue;
                if (ability->RaceMask && !(ability->RaceMask & raceMask))
                    continue;
                if (ability->ClassMask && !(ability->ClassMask & classMask))
                    continue;
                if (!sSpellMgr->GetSpellInfo(ability->Spell))
                    continue;

                seededSpellIds.insert(ability->Spell);
            }
        } while (skillResult->NextRow());
    }

    for (uint32 skillId : seededSkillIds)
        trans->Append(
            "INSERT INTO av_core_class_skill (characterGuid, class, skill, value, max) VALUES ({}, {}, {}, 1, 1) "
            "ON DUPLICATE KEY UPDATE value = 1, max = 1",
            characterGuid, classId, skillId);

    if (QueryResult spellResult = WorldDatabase.Query(
            "SELECT Spell FROM playercreateinfo_spell_custom "
            "WHERE (racemask = 0 OR racemask & {}) AND (classmask = 0 OR classmask & {})",
            raceMask, classMask))
    {
        do
        {
            seededSpellIds.insert(spellResult->Fetch()[0].Get<uint32>());
        } while (spellResult->NextRow());
    }

    for (uint32 spellId : seededSpellIds)
        trans->Append(
            "INSERT INTO av_core_class_spell (characterGuid, class, spell, specMask) VALUES ({}, {}, {}, 1) "
            "ON DUPLICATE KEY UPDATE specMask = 1",
            characterGuid, classId, spellId);

    if (QueryResult actionResult = WorldDatabase.Query(
            "SELECT button, action, type FROM playercreateinfo_action WHERE race = {} AND class = {}",
            player->getRace(), classId))
    {
        do
        {
            Field* fields = actionResult->Fetch();
            uint32 const action = fields[1].Get<uint32>();
            uint8 const type = fields[2].Get<uint8>();
            trans->Append(
                "INSERT INTO av_core_class_action (characterGuid, class, spec, button, action, type) "
                "VALUES ({}, {}, 0, {}, {}, {}) ON DUPLICATE KEY UPDATE action = {}, type = {}",
                characterGuid, classId, fields[0].Get<uint8>(), action, type, action, type);
        } while (actionResult->NextRow());
    }

    // Seed starting equipment from the same CharStartOutfit.dbc entry a brand
    // new character of this race+class+gender gets at creation (mirrors the
    // "attempt equip" half of Player::StoreNewItemInBestSlots), parked
    // straight into the Class Armory rather than equipped live -- this class
    // isn't active yet. Non-equipment outfit items (starting reagents, food,
    // etc.) are skipped: the Armory only ever holds the 19 equipment slots,
    // same as every other class's parked gear (see design doc §4.2).
    if (CharStartOutfitEntry const* outfit = GetCharStartOutfitEntry(player->getRace(), classId, player->getGender()))
    {
        for (int32 const itemId : outfit->ItemId)
        {
            if (itemId <= 0)
                continue;

            ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(uint32(itemId));
            if (!itemTemplate)
                continue;

            uint8 const equipSlot = GetOutfitEquipSlot(itemTemplate);
            if (equipSlot >= EQUIPMENT_SLOT_END)
                continue;

            Item* newItem = Item::CreateItem(uint32(itemId), 1, player);
            if (!newItem)
                continue;

            newItem->SaveToDB(trans);
            trans->Append(
                "INSERT INTO av_core_class_armory_item (characterGuid, class, equipSlot, itemGuid) VALUES ({}, {}, {}, {}) "
                "ON DUPLICATE KEY UPDATE itemGuid = VALUES(itemGuid)",
                characterGuid, classId, equipSlot, newItem->GetGUID().GetCounter());
            delete newItem;
        }
    }

    // DirectCommitTransaction, not the async CommitTransaction: the seeded
    // spell/action/armory rows this just wrote need to already be durably
    // committed the instant this function returns, because SwitchClass()'s
    // very next step (when this is called from HandleConfirmSwitch right
    // before SwitchClass) reads them straight back with a synchronous
    // CharacterDatabase.Query(). CommitTransaction() only enqueues the write
    // to a background worker and returns immediately -- that follow-up read
    // was racing a commit that usually hadn't happened yet, which is why a
    // freshly-seeded class's starting gear and spells never actually made it
    // onto the character.
    CharacterDatabase.DirectCommitTransaction(trans);

    if (!HasClassProfile(characterGuid, classId))
    {
        LOG_ERROR("server.vocations", "Failed to create class profile for character {} and class {}.", characterGuid, classId);
        return {};
    }

    return summary;
}

ClassSwitchResult CoreVocationService::SwitchClass(Player* player, uint8 targetClass, std::string const& initiatedBy)
{
    ClassSwitchResult const guardResult = CanSwitchClass(player, targetClass);
    if (guardResult != ClassSwitchResult::SUCCESS)
    {
        LogSwitch(player->GetGUID().GetCounter(), player->getClass(), targetClass, guardResult, initiatedBy);
        return guardResult;
    }

    uint32 const characterGuid = player->GetGUID().GetCounter();
    uint8 const currentClass = player->getClass();

    if (!HasClassProfile(characterGuid, targetClass))
    {
        ClassProfileSummary const createdProfile = CreateLevelOneClassProfile(player, targetClass);
        if (!createdProfile.classId || !HasClassProfile(characterGuid, targetClass))
        {
            LogSwitch(characterGuid, currentClass, targetClass,
                ClassSwitchResult::ERR_TARGET_PROFILE_CREATE_FAILED, initiatedBy);
            return ClassSwitchResult::ERR_TARGET_PROFILE_CREATE_FAILED;
        }
    }

    ClassProfileSummary targetProfile;
    for (ClassProfileSummary const& profile : GetClassProfiles(characterGuid))
    {
        if (profile.classId == targetClass)
        {
            targetProfile = profile;
            break;
        }
    }

    if (!targetProfile.classId)
    {
        LogSwitch(characterGuid, currentClass, targetClass,
            ClassSwitchResult::ERR_TARGET_PROFILE_CREATE_FAILED, initiatedBy);
        return ClassSwitchResult::ERR_TARGET_PROFILE_CREATE_FAILED;
    }

    // Phase A: flush the live player's current state to DB now, committed
    // immediately, so the archive step below reads fresh rows rather than
    // whatever was last saved before this session (or an uncommitted write
    // this same transaction hasn't sent yet -- a plain SELECT cannot see
    // writes queued on a transaction that hasn't committed).
    player->SaveToDB(false, false);

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    // --- Archive currentClass out of the live tables -----------------------
    trans->Append(
        "INSERT INTO av_core_class_profile (characterGuid, class, level, xp, isActive) "
        "VALUES ({}, {}, {}, {}, 0) "
        "ON DUPLICATE KEY UPDATE level = {}, xp = {}, isActive = 0, updatedAt = NOW()",
        characterGuid, currentClass, player->GetLevel(), player->GetUInt32Value(PLAYER_XP),
        player->GetLevel(), player->GetUInt32Value(PLAYER_XP));

    // Only spells actually restricted to a class (SkillLineAbility.dbc
    // ClassMask != 0) get archived and wiped here. Professions, First Aid,
    // Cooking, languages, etc. have ClassMask = 0 -- they aren't part of what
    // makes a class a class, and archiving them under whichever class you
    // happened to be would strand them there: restoring only ever pulls
    // av_core_class_spell rows for the class you're switching TO, so a
    // universal spell archived under class A is simply gone the moment you
    // switch to class B instead of back to A. Leaving them alone in
    // character_spell means they just stay learned across every class,
    // which is how a real character's professions work anyway.
    std::vector<uint32> archivedSpellIds;
    if (QueryResult spellRows = CharacterDatabase.Query(
            "SELECT spell, specMask FROM character_spell WHERE guid = {}", characterGuid))
    {
        do
        {
            Field* fields = spellRows->Fetch();
            uint32 const spellId = fields[0].Get<uint32>();
            if (!IsClassSpecificSpell(spellId))
                continue;

            trans->Append(
                "INSERT INTO av_core_class_spell (characterGuid, class, spell, specMask) VALUES ({}, {}, {}, {}) "
                "ON DUPLICATE KEY UPDATE specMask = {}",
                characterGuid, currentClass, spellId, fields[1].Get<uint8>(), fields[1].Get<uint8>());
            archivedSpellIds.push_back(spellId);
        } while (spellRows->NextRow());
    }
    for (uint32 spellId : archivedSpellIds)
        trans->Append("DELETE FROM character_spell WHERE guid = {} AND spell = {}", characterGuid, spellId);

    if (QueryResult talentRows = CharacterDatabase.Query(
            "SELECT spell, specMask FROM character_talent WHERE guid = {}", characterGuid))
    {
        do
        {
            Field* fields = talentRows->Fetch();
            trans->Append(
                "INSERT INTO av_core_class_talent (characterGuid, class, spell, specMask) VALUES ({}, {}, {}, {}) "
                "ON DUPLICATE KEY UPDATE specMask = {}",
                characterGuid, currentClass, fields[0].Get<uint32>(), fields[1].Get<uint8>(), fields[1].Get<uint8>());
        } while (talentRows->NextRow());
    }
    trans->Append("DELETE FROM character_talent WHERE guid = {}", characterGuid);

    if (QueryResult glyphRows = CharacterDatabase.Query(
            "SELECT talentGroup, glyph1, glyph2, glyph3, glyph4, glyph5, glyph6 FROM character_glyphs WHERE guid = {}",
            characterGuid))
    {
        do
        {
            Field* fields = glyphRows->Fetch();
            trans->Append(
                "INSERT INTO av_core_class_glyph (characterGuid, class, talentGroup, glyph1, glyph2, glyph3, glyph4, glyph5, glyph6) "
                "VALUES ({}, {}, {}, {}, {}, {}, {}, {}, {}) "
                "ON DUPLICATE KEY UPDATE glyph1 = {}, glyph2 = {}, glyph3 = {}, glyph4 = {}, glyph5 = {}, glyph6 = {}",
                characterGuid, currentClass,
                fields[0].Get<uint8>(), fields[1].Get<uint16>(), fields[2].Get<uint16>(), fields[3].Get<uint16>(),
                fields[4].Get<uint16>(), fields[5].Get<uint16>(), fields[6].Get<uint16>(),
                fields[1].Get<uint16>(), fields[2].Get<uint16>(), fields[3].Get<uint16>(),
                fields[4].Get<uint16>(), fields[5].Get<uint16>(), fields[6].Get<uint16>());
        } while (glyphRows->NextRow());
    }
    trans->Append("DELETE FROM character_glyphs WHERE guid = {}", characterGuid);

    if (QueryResult actionRows = CharacterDatabase.Query(
            "SELECT spec, button, action, type FROM character_action WHERE guid = {}", characterGuid))
    {
        do
        {
            Field* fields = actionRows->Fetch();
            trans->Append(
                "INSERT INTO av_core_class_action (characterGuid, class, spec, button, action, type) "
                "VALUES ({}, {}, {}, {}, {}, {}) "
                "ON DUPLICATE KEY UPDATE action = {}, type = {}",
                characterGuid, currentClass, fields[0].Get<uint8>(), fields[1].Get<uint8>(),
                fields[2].Get<uint32>(), fields[3].Get<uint8>(),
                fields[2].Get<uint32>(), fields[3].Get<uint8>());
        } while (actionRows->NextRow());
    }
    trans->Append("DELETE FROM character_action WHERE guid = {}", characterGuid);

    // Archive and remove the current class's skill lines as well. Skill lines
    // are what make the client create the class spellbook tabs (Arcane,
    // Holy, Beast Mastery, etc.). Leaving them in character_skills makes an
    // old tab survive a switch, while failing to restore the target lines
    // leaves the new class without its tab until the player levels and learns
    // another spell from that discipline.
    uint32 const currentRaceMask = 1u << (player->getRace() - 1);
    uint32 const currentClassMask = 1u << (currentClass - 1);
    if (QueryResult skillDefinitions = WorldDatabase.Query(
            "SELECT skill FROM playercreateinfo_skills "
            "WHERE (raceMask = 0 OR raceMask & {}) AND classMask != 0 AND classMask & {}",
            currentRaceMask, currentClassMask))
    {
        do
        {
            uint16 const skillId = skillDefinitions->Fetch()[0].Get<uint16>();
            if (QueryResult skillRows = CharacterDatabase.Query(
                    "SELECT value, max FROM character_skills WHERE guid = {} AND skill = {}",
                    characterGuid, skillId))
            {
                Field* fields = skillRows->Fetch();
                trans->Append(
                    "INSERT INTO av_core_class_skill (characterGuid, class, skill, value, max) "
                    "VALUES ({}, {}, {}, {}, {}) "
                    "ON DUPLICATE KEY UPDATE value = {}, max = {}",
                    characterGuid, currentClass, skillId,
                    fields[0].Get<uint16>(), fields[1].Get<uint16>(),
                    fields[0].Get<uint16>(), fields[1].Get<uint16>());
                trans->Append(
                    "DELETE FROM character_skills WHERE guid = {} AND skill = {}",
                    characterGuid, skillId);
            }
        } while (skillDefinitions->NextRow());
    }

    // Per design doc 4a: auras are not part of a class's saved identity and are
    // never restored on switch-back. Clear unconditionally rather than trying
    // to enumerate which specific auras are class-unsafe.
    trans->Append("DELETE FROM character_aura WHERE guid = {}", characterGuid);

    // --- Park currentClass's equipped gear into the Armory ------------------
    // The player is still fully online here (the forced logout happens after
    // this whole transaction commits), so each of these items is a live,
    // equipped Item object in the player's in-memory inventory -- not just a
    // character_inventory row. Deleting that row directly, as an earlier
    // version of this code did, leaves the live Item object still flagged
    // "in world" with no owner slot, which later trips Object::~Object's
    // "deleted but still in world!!" assert. Use the same
    // detach-then-persist pattern AuctionHouseHandler.cpp uses when an item
    // leaves a player's inventory for other cold storage (the auction
    // house): MoveItemFromInventory() first to properly unequip/detach the
    // live object and notify the client, then DeleteFromInventoryDB()/
    // SaveToDB() to persist that on the DB side.
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item)
            continue;

        // ON DUPLICATE KEY UPDATE, not a bare INSERT: this is the one place in
        // the whole switch where getting it wrong causes silent item loss, not
        // just a logged error. A stale row can already exist here (e.g. a
        // previous switch away from this class that itself hit an error
        // partway through and left an unparked leftover) -- without the
        // update clause, a 1062 on this statement is just skipped by the
        // transaction executor while MoveItemFromInventory() below still runs
        // unconditionally, detaching the item from the player with nothing
        // left pointing at where it went.
        uint32 const itemGuid = item->GetGUID().GetCounter();
        trans->Append(
            "INSERT INTO av_core_class_armory_item (characterGuid, class, equipSlot, itemGuid) VALUES ({}, {}, {}, {}) "
            "ON DUPLICATE KEY UPDATE itemGuid = VALUES(itemGuid)",
            characterGuid, currentClass, slot, itemGuid);

        player->MoveItemFromInventory(INVENTORY_SLOT_BAG_0, slot, true);
        item->DeleteFromInventoryDB(trans);
        item->SaveToDB(trans);
    }

    // --- Restore targetClass into the live tables ---------------------------
    if (QueryResult spellRows = CharacterDatabase.Query(
            "SELECT spell, specMask FROM av_core_class_spell WHERE characterGuid = {} AND class = {}",
            characterGuid, targetClass))
    {
        do
        {
            Field* fields = spellRows->Fetch();
            trans->Append(
                "INSERT INTO character_spell (guid, spell, specMask) VALUES ({}, {}, {})",
                characterGuid, fields[0].Get<uint32>(), fields[1].Get<uint8>());
        } while (spellRows->NextRow());
    }
    trans->Append("DELETE FROM av_core_class_spell WHERE characterGuid = {} AND class = {}", characterGuid, targetClass);

    if (QueryResult skillRows = CharacterDatabase.Query(
            "SELECT skill, value, max FROM av_core_class_skill WHERE characterGuid = {} AND class = {}",
            characterGuid, targetClass))
    {
        do
        {
            Field* fields = skillRows->Fetch();
            trans->Append(
                "REPLACE INTO character_skills (guid, skill, value, max) VALUES ({}, {}, {}, {})",
                characterGuid, fields[0].Get<uint16>(), fields[1].Get<uint16>(), fields[2].Get<uint16>());
        } while (skillRows->NextRow());
    }
    trans->Append("DELETE FROM av_core_class_skill WHERE characterGuid = {} AND class = {}", characterGuid, targetClass);

    if (QueryResult talentRows = CharacterDatabase.Query(
            "SELECT spell, specMask FROM av_core_class_talent WHERE characterGuid = {} AND class = {}",
            characterGuid, targetClass))
    {
        do
        {
            Field* fields = talentRows->Fetch();
            trans->Append(
                "INSERT INTO character_talent (guid, spell, specMask) VALUES ({}, {}, {})",
                characterGuid, fields[0].Get<uint32>(), fields[1].Get<uint8>());
        } while (talentRows->NextRow());
    }
    trans->Append("DELETE FROM av_core_class_talent WHERE characterGuid = {} AND class = {}", characterGuid, targetClass);

    if (QueryResult glyphRows = CharacterDatabase.Query(
            "SELECT talentGroup, glyph1, glyph2, glyph3, glyph4, glyph5, glyph6 FROM av_core_class_glyph "
            "WHERE characterGuid = {} AND class = {}",
            characterGuid, targetClass))
    {
        do
        {
            Field* fields = glyphRows->Fetch();
            trans->Append(
                "INSERT INTO character_glyphs (guid, talentGroup, glyph1, glyph2, glyph3, glyph4, glyph5, glyph6) "
                "VALUES ({}, {}, {}, {}, {}, {}, {}, {})",
                characterGuid, fields[0].Get<uint8>(), fields[1].Get<uint16>(), fields[2].Get<uint16>(),
                fields[3].Get<uint16>(), fields[4].Get<uint16>(), fields[5].Get<uint16>(), fields[6].Get<uint16>());
        } while (glyphRows->NextRow());
    }
    trans->Append("DELETE FROM av_core_class_glyph WHERE characterGuid = {} AND class = {}", characterGuid, targetClass);

    if (QueryResult actionRows = CharacterDatabase.Query(
            "SELECT spec, button, action, type FROM av_core_class_action WHERE characterGuid = {} AND class = {}",
            characterGuid, targetClass))
    {
        do
        {
            Field* fields = actionRows->Fetch();
            trans->Append(
                "INSERT INTO character_action (guid, spec, button, action, type) VALUES ({}, {}, {}, {}, {})",
                characterGuid, fields[0].Get<uint8>(), fields[1].Get<uint8>(), fields[2].Get<uint32>(), fields[3].Get<uint8>());
        } while (actionRows->NextRow());
    }
    trans->Append("DELETE FROM av_core_class_action WHERE characterGuid = {} AND class = {}", characterGuid, targetClass);

    // --- Restore targetClass's parked equipment, where still valid ----------
    if (QueryResult armoryRows = CharacterDatabase.Query(
            "SELECT a.equipSlot, a.itemGuid, i.itemEntry FROM av_core_class_armory_item a "
            "INNER JOIN item_instance i ON i.guid = a.itemGuid "
            "WHERE a.characterGuid = {} AND a.class = {}",
            characterGuid, targetClass))
    {
        do
        {
            Field* fields = armoryRows->Fetch();
            uint8 const equipSlot = fields[0].Get<uint8>();
            uint32 const itemGuid = fields[1].Get<uint32>();
            uint32 const itemEntry = fields[2].Get<uint32>();

            ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemEntry);
            bool const canEquip = itemTemplate &&
                (itemTemplate->AllowableClass & (1 << (targetClass - 1))) != 0;

            if (canEquip)
            {
                trans->Append(
                    "INSERT INTO character_inventory (guid, bag, slot, item) VALUES ({}, {}, {}, {})",
                    characterGuid, uint32(INVENTORY_SLOT_BAG_0), equipSlot, itemGuid);
                trans->Append(
                    "DELETE FROM av_core_class_armory_item WHERE characterGuid = {} AND class = {} AND equipSlot = {}",
                    characterGuid, targetClass, equipSlot);
            }
            // else: leave it parked. Not an error, not a partial failure --
            // see design doc 4.3.
        } while (armoryRows->NextRow());
    }

    // --- Flip the switch itself ----------------------------------------------
    trans->Append(
        "UPDATE characters SET class = {}, level = {}, xp = {} WHERE guid = {}",
        targetClass, targetProfile.level, targetProfile.xp, characterGuid);
    trans->Append(
        "UPDATE av_core_class_profile SET isActive = 0, updatedAt = NOW() WHERE characterGuid = {} AND isActive = 1",
        characterGuid);
    trans->Append(
        "UPDATE av_core_class_profile SET isActive = 1, updatedAt = NOW() WHERE characterGuid = {} AND class = {}",
        characterGuid, targetClass);

    // DirectCommitTransaction, not the async CommitTransaction -- same reason
    // as CreateLevelOneClassProfile above. When AzerothVocations.Core.
    // LiveSwitchEnabled is on, LiveApplySwitch() runs immediately after this
    // returns and synchronously re-reads character_spell/character_talent/
    // character_inventory to re-apply everything to the live player. Those
    // reads were racing this transaction's own async commit -- the spells and
    // gear this transaction had just restored into those tables often weren't
    // durably written yet, which is what actually produced the "still shows
    // the old class with no spells/gear" symptom, independent of whichever
    // class was involved.
    CharacterDatabase.DirectCommitTransaction(trans);

    LogSwitch(characterGuid, currentClass, targetClass, ClassSwitchResult::SUCCESS, initiatedBy);

    if (_liveSwitchEnabled)
    {
        // See design doc §5a. Steps 1-13 above are unchanged either way; this
        // only replaces what used to happen next.
        LiveApplySwitch(player, currentClass, targetClass, targetProfile);
    }
    else
    {
        // No live class hot-swap: disconnect without a second save. The player
        // object in memory is still the OLD class; saving it now would overwrite
        // everything the transaction above just wrote. Next login runs
        // Player::LoadFromDB() fresh against the now-updated tables.
        if (WorldSession* session = player->GetSession())
            session->LogoutPlayer(false);
    }

    return ClassSwitchResult::SUCCESS;
}

void CoreVocationService::LiveApplySwitch(Player* player, uint8 oldClass, uint8 targetClass, ClassProfileSummary const& targetProfile)
{
    uint32 const characterGuid = player->GetGUID().GetCounter();

    // a. Interrupt casting; dismiss pet/totems/summons -- exact sequence
    // Player::ActivateSpec uses before touching anything else (Player.cpp
    // ~15495-15511).
    if (player->IsNonMeleeSpellCast(false))
        player->InterruptNonMeleeSpells(false);

    if (Pet* pet = player->GetPet())
        player->RemovePet(pet, PET_SAVE_NOT_IN_SLOT);

    player->ClearAllReactives();
    player->UnsummonAllTotems();
    player->RemoveAllControlled();

    // b. Tell the client to clear its action bars (ActivateSpec: SendActionButtons(2)).
    player->SendActionButtons(2);

    // c. Auras never survive a switch either way -- design doc §4a.
    player->RemoveAllAuras();

    // d. Unlearn every currently-known CLASS-SPECIFIC spell (see
    // IsClassSpecificSpell) -- professions, First Aid, etc. were never
    // archived or deleted from the DB by SwitchClass() above, so stripping
    // them here too would just desync the live spellbook from what's
    // actually still on the character. Snapshot ids first: removeSpell
    // mutates the map we'd otherwise be iterating.
    {
        std::vector<uint32> oldSpellIds;
        for (auto const& [spellId, spell] : player->GetSpellMap())
            if (spell->State != PLAYERSPELL_REMOVED && IsClassSpecificSpell(spellId))
                oldSpellIds.push_back(spellId);
        for (uint32 spellId : oldSpellIds)
            player->removeSpell(spellId, SPEC_MASK_ALL, false);
    }

    // e. Unlearn every currently-known talent the same way.
    {
        std::vector<uint32> oldTalentIds;
        for (auto const& [spellId, talent] : player->GetTalentMap())
            if (talent->State != PLAYERSPELL_REMOVED)
                oldTalentIds.push_back(spellId);
        for (uint32 spellId : oldTalentIds)
            player->_removeTalent(spellId, SPEC_MASK_ALL);
    }

    // Replace the outgoing class skill lines in memory too. The database
    // transaction already archived and removed them, but a live switch does
    // not pass through Player::LoadFromDB(), so leaving the old skill fields
    // here keeps stale spellbook disciplines (for example Arcane after
    // leaving Mage) visible to the client.
    uint32 const raceMask = 1u << (player->getRace() - 1);
    uint32 const oldClassMask = 1u << (oldClass - 1);
    if (QueryResult skillDefinitions = WorldDatabase.Query(
            "SELECT skill FROM playercreateinfo_skills "
            "WHERE (raceMask = 0 OR raceMask & {}) AND classMask != 0 AND classMask & {}",
            raceMask, oldClassMask))
    {
        do
            player->SetSkill(skillDefinitions->Fetch()[0].Get<uint16>(), 0, 0, 0);
        while (skillDefinitions->NextRow());
    }

    // f. Clear every glyph in the active spec (ActivateSpec's glyph-removal loop).
    for (uint8 slot = 0; slot < MAX_GLYPH_SLOT_INDEX; ++slot)
    {
        if (uint32 glyphId = player->GetGlyph(slot))
        {
            if (GlyphPropertiesEntry const* glyphEntry = sGlyphPropertiesStore.LookupEntry(glyphId))
                player->RemoveAurasDueToSpell(glyphEntry->SpellId);
            player->SetGlyph(slot, 0, true);
        }
    }

    // g. Unequip every currently-equipped item. The transaction already
    // deleted these rows from character_inventory (parked into the Armory);
    // this is only the live in-memory/visual side of that same fact -- the
    // item_instance row is untouched, exactly like the relog path leaves it.
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
        {
            player->RemoveItem(INVENTORY_SLOT_BAG_0, slot, true);

            // Same queue-detach SetState(..., ITEM_REMOVED) itself uses
            // (Item.cpp:722) before a deleted item's pointer would otherwise
            // dangle in the queue.
            item->RemoveFromUpdateQueueOf(player);
            delete item;
        }
    }

    // h. Class byte itself. No dedicated setter exists in core today (see
    // design doc §5a) -- plain field write, same replication every other
    // UpdateField change already uses.
    player->SetByteValue(UNIT_FIELD_BYTES_0, 1, targetClass);

    // Live switches do not pass through Player::LoadFromDB(), where default
    // skill lines are normally restored. Restore the target class's exact
    // skill rows from the committed character database so the client can
    // build its spell tabs immediately, including custom-race profiles that
    // are not represented by PlayerInfo's normal race/class matrix.
    uint32 const targetClassMask = 1u << (targetClass - 1);
    if (QueryResult skillDefinitions = WorldDatabase.Query(
            "SELECT skill FROM playercreateinfo_skills "
            "WHERE (raceMask = 0 OR raceMask & {}) AND classMask != 0 AND classMask & {}",
            raceMask, targetClassMask))
    {
        do
        {
            uint16 const skillId = skillDefinitions->Fetch()[0].Get<uint16>();
            if (QueryResult skillRows = CharacterDatabase.Query(
                    "SELECT value, max FROM character_skills WHERE guid = {} AND skill = {}",
                    characterGuid, skillId))
            {
                Field* fields = skillRows->Fetch();
                player->SetSkill(skillId, 0, fields[0].Get<uint16>(), fields[1].Get<uint16>());
            }
        } while (skillDefinitions->NextRow());
    }

    // i. Power type for the new class.
    ChrClassesEntry const* classEntry = sChrClassesStore.LookupEntry(targetClass);
    if (classEntry)
        player->setPowerType(Powers(classEntry->powerType));

    // j. Level/xp for the new class. GiveLevel reads getClass() live (already
    // updated by step h), so it recalculates health/power/stats for the new
    // class the same way every normal level-up does. GiveLevel resets XP as
    // part of a normal level-up, so the archived value is re-applied right
    // after, matching what the transaction already wrote to `characters`.
    player->GiveLevel(targetProfile.level);
    player->SetUInt32Value(PLAYER_XP, targetProfile.xp);

    // k. Relearn the new class's spells/talents/glyphs from the rows the
    // transaction just restored into the live tables -- re-reading them
    // (rather than re-deriving from av_core_class_* again) guarantees this
    // matches exactly what was actually committed.
    if (QueryResult spellRows = CharacterDatabase.Query(
            "SELECT spell, specMask FROM character_spell WHERE guid = {}", characterGuid))
    {
        do
        {
            Field* fields = spellRows->Fetch();
            player->addSpell(fields[0].Get<uint32>(), fields[1].Get<uint8>(), true, false);
        } while (spellRows->NextRow());
    }

    uint32 spentTalents = 0;
    if (QueryResult talentRows = CharacterDatabase.Query(
            "SELECT spell, specMask FROM character_talent WHERE guid = {}", characterGuid))
    {
        do
        {
            Field* fields = talentRows->Fetch();
            uint32 const spellId = fields[0].Get<uint32>();
            player->addTalent(spellId, fields[1].Get<uint8>(), 0);
            if (TalentSpellPos const* talentPos = GetTalentSpellPos(spellId))
                spentTalents += talentPos->rank + 1;
        } while (talentRows->NextRow());
    }
    player->SetUsedTalentCount(spentTalents);
    player->InitTalentForLevel();

    if (QueryResult glyphRows = CharacterDatabase.Query(
            "SELECT talentGroup, glyph1, glyph2, glyph3, glyph4, glyph5, glyph6 FROM character_glyphs "
            "WHERE guid = {} AND talentGroup = {}",
            characterGuid, player->GetActiveSpec()))
    {
        Field* fields = glyphRows->Fetch();
        for (uint8 slot = 0; slot < MAX_GLYPH_SLOT_INDEX; ++slot)
        {
            uint32 const glyphId = fields[1 + slot].Get<uint16>();
            if (!glyphId)
                continue;

            if (GlyphPropertiesEntry const* glyphEntry = sGlyphPropertiesStore.LookupEntry(glyphId))
                player->CastSpell(player, glyphEntry->SpellId, TriggerCastFlags(TRIGGERED_FULL_MASK & ~(TRIGGERED_IGNORE_SHAPESHIFT | TRIGGERED_IGNORE_CASTER_AURASTATE)));
            player->SetGlyph(slot, glyphId, true);
        }
    }

    // l. Re-equip whatever the transaction's Armory-restore step (per 4.3)
    // actually placed back into character_inventory for this class -- reread
    // rather than re-deriving validity, so this can never disagree with what
    // was actually committed.
    if (QueryResult equipRows = CharacterDatabase.Query(
            "SELECT slot, item FROM character_inventory WHERE guid = {} AND bag = {} AND slot < {}",
            characterGuid, uint32(INVENTORY_SLOT_BAG_0), uint32(EQUIPMENT_SLOT_END)))
    {
        do
        {
            Field* invFields = equipRows->Fetch();
            uint8 const slot = invFields[0].Get<uint8>();
            uint32 const itemGuid = invFields[1].Get<uint32>();

            if (QueryResult itemRow = CharacterDatabase.Query(
                    "SELECT creatorGuid, giftCreatorGuid, count, duration, charges, flags, enchantments, "
                    "randomPropertyId, durability, playedTime, text, itemEntry FROM item_instance WHERE guid = {}",
                    itemGuid))
            {
                Field* itemFields = itemRow->Fetch();
                uint32 const itemEntry = itemFields[11].Get<uint32>();
                if (ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemEntry))
                {
                    Item* item = NewItemOrBag(itemTemplate);
                    if (item->LoadFromDB(itemGuid, player->GetGUID(), itemFields, itemEntry))
                    {
                        // Item's constructor defaults uState to ITEM_NEW;
                        // LoadFromDB never corrects it (confirmed by reading
                        // both -- neither touches uState). Left alone, the
                        // next save would REPLACE INTO this row instead of
                        // UPDATE it (Item.cpp's SaveToDB branches on exactly
                        // this flag) even though the row already exists.
                        // Same fix real login-time loading already applies
                        // (PlayerStorage.cpp:6053) -- this item was just
                        // read from an existing row, it is not new.
                        item->SetState(ITEM_UNCHANGED, player);
                        player->EquipItem(uint16(INVENTORY_SLOT_BAG_0 << 8 | slot), item, true);
                    }
                    else
                        delete item;
                }
            }
        } while (equipRows->NextRow());
    }

    // m. Recompute every derived stat (armor/health/mana/etc.) now that
    // class, level, spells, talents, and equipment are all in their final
    // live state -- same call InitStatsForLevel(true) makes on a normal
    // level-up or spec reset.
    player->InitStatsForLevel(true);

    // n. Reload and resend action bars: identical async pattern
    // Player::ActivateSpec uses (Player.cpp ~15637-15651), reusing the same
    // public Player::LoadActions(result) it calls back into (Player.cpp
    // ~15699), which itself does the DB-row-apply + SendActionButtons(1).
    {
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_ACTIONS_SPEC);
        stmt->SetData(0, characterGuid);
        stmt->SetData(1, player->GetActiveSpec());
        WorldSession* session = player->GetSession();
        session->GetQueryProcessor().AddCallback(CharacterDatabase.AsyncQuery(stmt)
            .WithPreparedCallback([session](PreparedQueryResult result)
        {
            if (Player* thisPlayer = session->GetPlayer())
                thisPlayer->LoadActions(result);
        }));
    }
}

void CoreVocationService::LogSwitch(uint32 characterGuid, uint8 fromClass, uint8 toClass, ClassSwitchResult result, std::string const& initiatedBy)
{
    CharacterDatabase.Execute(
        "INSERT INTO av_core_class_switch_log (characterGuid, fromClass, toClass, result, initiatedBy) "
        "VALUES ({}, {}, {}, {}, '{}')",
        characterGuid, fromClass, toClass, uint8(result), initiatedBy);
}

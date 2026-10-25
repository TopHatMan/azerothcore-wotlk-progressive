/*
 * Core Vocation Service. See modules/mod-azeroth-vocations/docs/design/
 * core-vocation-service-design.md for the full design this implements.
 *
 * Ownership: this is the ONLY code permitted to write to av_core_* tables,
 * and the only code that changes characters.class / character_spell /
 * character_talent / character_glyphs / character_action / character_inventory
 * for the purpose of a class switch. mod-azeroth-vocations (and nothing else)
 * calls this interface; it must never reach past it into any of those tables.
 */

#ifndef CORE_VOCATION_SERVICE_H
#define CORE_VOCATION_SERVICE_H

#include "Define.h"
#include <array>
#include <string>
#include <vector>

class Player;

enum class ClassSwitchResult : uint8
{
    SUCCESS                           = 0,
    ERR_SERVICE_DISABLED              = 1,
    ERR_TARGET_CLASS_DISABLED         = 2,
    ERR_PLAYER_DEAD                   = 3,
    ERR_PLAYER_IN_COMBAT              = 4,
    ERR_PLAYER_GROUPED                = 5,
    ERR_PLAYER_ON_TRANSPORT           = 6,
    ERR_PLAYER_FLYING                 = 7,
    ERR_PLAYER_IN_INSTANCE            = 8,
    ERR_UNSAFE_INVENTORY_STATE        = 9,
    ERR_TARGET_PROFILE_CREATE_FAILED  = 10,
    ERR_DB_TRANSACTION_FAILED         = 11,
    ERR_ALREADY_ACTIVE_CLASS          = 12,
};

struct ClassProfileSummary
{
    uint8 classId = 0;
    uint8 level = 1;
    uint32 xp = 0;
    bool isActive = false;
};

struct ArmoryItemSummary
{
    uint8 equipSlot     = 0;
    uint32 itemGuid     = 0;
    uint32 itemEntry    = 0;
};

class CoreVocationService
{
public:
    static CoreVocationService* instance();

    // --- Read-only queries --------------------------------------------------
    std::vector<ClassProfileSummary> GetClassProfiles(uint32 characterGuid);
    bool HasClassProfile(uint32 characterGuid, uint8 classId);
    std::vector<ArmoryItemSummary> GetArmorySummary(uint32 characterGuid, uint8 classId);

    // --- Guarded mutation ----------------------------------------------------
    ClassSwitchResult CanSwitchClass(Player* player, uint8 targetClass);
    ClassProfileSummary CreateLevelOneClassProfile(Player* player, uint8 classId);
    ClassSwitchResult SwitchClass(Player* player, uint8 targetClass, std::string const& initiatedBy = "player");

    // --- Config gate (module reads this indirectly via CanSwitchClass) --------
    bool IsServiceEnabled() const;

    // Caches AzerothVocations.Core.Enable / AzerothVocations.Core.ClassEnabled.<N>
    // into _serviceEnabled / _classEnabled. Called once at startup and again on
    // every .reload config -- see src/server/scripts/World/vocation_service_config.cpp.
    // Guarded checks (IsServiceEnabled/CanSwitchClass) read the cache, never
    // sConfigMgr directly, per .agents/docs/cpp-guidelines.md's config rule.
    void LoadConfig();

private:
    CoreVocationService() = default;

    void LogSwitch(uint32 characterGuid, uint8 fromClass, uint8 toClass, ClassSwitchResult result, std::string const& initiatedBy);

    // Step 14 (live) of SwitchClass -- see core-vocation-service-design.md §5a.
    // Called after the transaction has committed, in place of the forced
    // relog, only when _liveSwitchEnabled. Applies the already-persisted
    // archive/restore result onto the connected Player live: spells, talents,
    // glyphs, equipment, action bars, power type, level/xp, class byte.
    void LiveApplySwitch(Player* player, uint8 oldClass, uint8 targetClass, ClassProfileSummary const& targetProfile);

    bool _serviceEnabled = false;
    bool _liveSwitchEnabled = false;
    // Index 0 unused (no class 0); matches CLASS_* ids 1..MAX_CLASSES-1 directly.
    std::array<bool, 12> _classEnabled = { false, true, true, true, true, true, false, true, true, true, false, true };
};

#define sCoreVocationService CoreVocationService::instance()

#endif // CORE_VOCATION_SERVICE_H

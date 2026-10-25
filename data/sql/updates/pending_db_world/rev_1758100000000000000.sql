-- Custom race starter-data bootstrap.
--
-- High Elf (12) follows Human (1) for the Alliance classes and Goblin (9)
-- follows Orc (2) for the Horde classes.  The extra ARAC classes use an
-- existing race that already has that class's native starter data:
-- Night Elf (4) supplies Druid (11).
--
-- Do not use race + 11 for Goblin: Orc is race 2 and Goblin is race 9.

-- Copy the complete action bars for the same class/race mapping.  This is
-- separate from CharStartOutfit.dbc: action bars are database data, while the
-- visible level-one equipment comes from CharStartOutfit.dbc.
DELETE FROM `playercreateinfo_action` WHERE `race` IN (9, 12);
INSERT INTO `playercreateinfo_action` (`race`, `class`, `button`, `action`, `type`)
SELECT 12, `class`, `button`, `action`, `type`
FROM `playercreateinfo_action`
WHERE `race` = 1 AND `class` IN (1, 2, 4, 5, 6, 8, 9)
UNION ALL
SELECT 12, `class`, `button`, `action`, `type`
FROM `playercreateinfo_action`
WHERE `race` = 2 AND `class` IN (3, 7)
UNION ALL
SELECT 12, `class`, `button`, `action`, `type`
FROM `playercreateinfo_action`
WHERE `race` = 4 AND `class` = 11
UNION ALL
SELECT 9, `class`, `button`, `action`, `type`
FROM `playercreateinfo_action`
WHERE `race` = 2 AND `class` IN (1, 3, 4, 6, 7, 8, 9)
UNION ALL
SELECT 9, `class`, `button`, `action`, `type`
FROM `playercreateinfo_action`
WHERE `race` = 1 AND `class` IN (2, 5)
UNION ALL
SELECT 9, `class`, `button`, `action`, `type`
FROM `playercreateinfo_action`
WHERE `race` = 4 AND `class` = 11;

-- Copy any optional custom starter spells if the installation has populated
-- the source table.  The stock fork normally keeps this table empty; class
-- spells and racial skills are loaded from playercreateinfo_skills/DBC data.
DELETE FROM `playercreateinfo_spell_custom` WHERE `racemask` IN (256, 2048);
INSERT INTO `playercreateinfo_spell_custom` (`racemask`, `classmask`, `Spell`, `Note`)
SELECT 2048, `classmask`, `Spell`, `Note`
FROM `playercreateinfo_spell_custom`
WHERE `racemask` = 1
UNION ALL
SELECT 256, `classmask`, `Spell`, `Note`
FROM `playercreateinfo_spell_custom`
WHERE `racemask` = 2;

-- playercreateinfo_item is intentionally not populated by this migration.
-- In this fork the base table contains no Human/Orc starter-kit rows (only a
-- placeholder DK row), so copying it cannot create equipment.  New-character
-- equipment is read from CharStartOutfit.dbc; that DBC must contain valid
-- rows for race 12 and race 9 in the server's active DBC directory.

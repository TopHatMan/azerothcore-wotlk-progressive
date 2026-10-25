-- Goblin Paladin: keep the server bootstrap idempotent for databases that
-- already received mod_custom_races.sql before its missing action row was
-- corrected.
DELETE FROM `playercreateinfo_action`
WHERE `race` = 9 AND `class` = 2 AND `button` = 0;

INSERT INTO `playercreateinfo_action` (`race`, `class`, `button`, `action`, `type`)
VALUES (9, 2, 0, 6603, 0);

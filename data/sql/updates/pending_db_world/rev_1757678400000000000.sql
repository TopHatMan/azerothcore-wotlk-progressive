-- High Elf (race 22) server-side completion.
-- Reuses Blood Elf racial stats and Human-compatible Alliance equipment.

DELETE FROM `chrraces_dbc` WHERE `ID` = 22;
INSERT INTO `chrraces_dbc`
  (`ID`, `Flags`, `FactionID`, `ExplorationSoundID`, `MaleDisplayId`, `FemaleDisplayId`,
   `ClientPrefix`, `BaseLanguage`, `CreatureType`, `ResSicknessSpellID`, `SplashSoundID`,
   `ClientFilestring`, `CinematicSequenceID`, `Alliance`, `Name_Lang_enUS`)
VALUES
  (22, 12, 1, 4140, 15476, 15475, 'He', 7, 7, 15007, 1096, 'HighElf', 81, 0, 'High Elf');

DELETE FROM `player_race_stats` WHERE `Race` = 22;
INSERT INTO `player_race_stats` (`Race`, `Strength`, `Agility`, `Stamina`, `Intellect`, `Spirit`) VALUES
(22, -3, 2, 0, 3, -2);

DELETE FROM `playercreateinfo_action` WHERE `race` = 22 AND `class` = 1 AND `button` = 0;
INSERT INTO `playercreateinfo_action` (`race`, `class`, `button`, `action`, `type`) VALUES
(22, 1, 0, 6603, 0);

-- Race 22 uses bit 21. Add it to existing Human-wearable Alliance items,
-- while leaving the established all-races sentinel masks unchanged.
UPDATE `item_template`
SET `allowablerace` = `allowablerace` | 2097152
WHERE (`allowablerace` & 1) != 0
  AND `allowablerace` NOT IN (-1, 2047, 4095, 8191, 16383, 32767, 65535,
                             131071, 262143, 524287, 1048575, 2097151,
                             4194303);

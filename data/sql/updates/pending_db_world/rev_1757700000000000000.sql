-- Consolidate the High Elf implementation on client/server race slot 12.
-- The earlier experimental migration used race 22, but the active client
-- patch replaces the working custom slot 12 from mod-worgoblin. Leaving both
-- definitions present makes the client and server disagree about the model.
DELETE FROM `chrraces_dbc` WHERE `ID` = 22;

DELETE FROM `chrraces_dbc` WHERE `ID` = 12;
INSERT INTO `chrraces_dbc`
  (`ID`, `Flags`, `FactionID`, `ExplorationSoundID`, `MaleDisplayId`, `FemaleDisplayId`,
   `ClientPrefix`, `BaseLanguage`, `CreatureType`, `ResSicknessSpellID`, `SplashSoundID`,
   `ClientFilestring`, `CinematicSequenceID`, `Alliance`, `Name_Lang_enUS`)
VALUES
  (12, 12, 1, 4140, 40076, 40075, 'He', 7, 7, 15007, 1096, 'HighElf', 81, 0, 'High Elf');

DELETE FROM `player_race_stats` WHERE `Race` = 22;
DELETE FROM `playercreateinfo` WHERE `race` = 22;
DELETE FROM `playercreateinfo_action` WHERE `race` = 22;

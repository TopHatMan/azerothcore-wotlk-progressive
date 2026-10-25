-- Re-key custom CharStartOutfit records away from native DBC record IDs.
--
-- The earlier custom-race overlay used IDs that may also identify native
-- CharStartOutfit records in an unmodified server DBC. DBC SQL overlays are
-- keyed by ID, so those rows could replace Human/Dwarf/etc. starter outfits.
-- Preserve the custom rows, move them to an unused range, and remove only
-- the old custom-race rows so native races can use their binary DBC records.

DROP TEMPORARY TABLE IF EXISTS `av_custom_charstartoutfit_rekey`;
CREATE TEMPORARY TABLE `av_custom_charstartoutfit_rekey` AS
SELECT *
FROM `charstartoutfit_dbc`
WHERE `RaceID` IN (9, 12)
  AND `ID` IN (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
               127, 128, 129, 130, 131, 132,
               223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233,
               234, 235, 236, 237, 238, 239, 240, 241, 242,
               2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
               2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019, 2020,
               2021, 2022, 2023, 2024, 2025, 2026, 2027, 2028, 2029, 2030,
               2031, 2032, 2033, 2034, 2035, 2036, 2037, 2038, 2039, 2040);

UPDATE `av_custom_charstartoutfit_rekey`
SET `ID` = CASE `ID`
    WHEN 1 THEN 2001 WHEN 2 THEN 2002 WHEN 3 THEN 2003 WHEN 4 THEN 2004
    WHEN 5 THEN 2005 WHEN 6 THEN 2006 WHEN 7 THEN 2007 WHEN 8 THEN 2008
    WHEN 9 THEN 2009 WHEN 10 THEN 2010 WHEN 11 THEN 2011 WHEN 12 THEN 2012
    WHEN 13 THEN 2013 WHEN 14 THEN 2014 WHEN 127 THEN 2015 WHEN 128 THEN 2016
    WHEN 129 THEN 2017 WHEN 130 THEN 2018 WHEN 131 THEN 2019 WHEN 132 THEN 2020
    WHEN 223 THEN 2021 WHEN 224 THEN 2022 WHEN 225 THEN 2023 WHEN 226 THEN 2024
    WHEN 227 THEN 2025 WHEN 228 THEN 2026 WHEN 229 THEN 2027 WHEN 230 THEN 2028
    WHEN 231 THEN 2029 WHEN 232 THEN 2030 WHEN 233 THEN 2031 WHEN 234 THEN 2032
    WHEN 235 THEN 2033 WHEN 236 THEN 2034 WHEN 237 THEN 2035 WHEN 238 THEN 2036
    WHEN 239 THEN 2037 WHEN 240 THEN 2038 WHEN 241 THEN 2039 WHEN 242 THEN 2040
    ELSE `ID`
END;

DELETE FROM `charstartoutfit_dbc`
WHERE `ID` IN (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
               127, 128, 129, 130, 131, 132,
               223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233,
               234, 235, 236, 237, 238, 239, 240, 241, 242,
               2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
               2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019, 2020,
               2021, 2022, 2023, 2024, 2025, 2026, 2027, 2028, 2029, 2030,
               2031, 2032, 2033, 2034, 2035, 2036, 2037, 2038, 2039, 2040);

INSERT INTO `charstartoutfit_dbc`
SELECT * FROM `av_custom_charstartoutfit_rekey`;

DROP TEMPORARY TABLE `av_custom_charstartoutfit_rekey`;

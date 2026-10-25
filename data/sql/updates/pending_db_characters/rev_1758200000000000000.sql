-- Stores class-specific skill lines for mod-azeroth-vocations.
-- Languages and other class-independent skills remain in character_skills.
CREATE TABLE IF NOT EXISTS `av_core_class_skill` (
    `characterGuid` INT UNSIGNED NOT NULL,
    `class` TINYINT UNSIGNED NOT NULL,
    `skill` SMALLINT UNSIGNED NOT NULL,
    `value` SMALLINT UNSIGNED NOT NULL DEFAULT 1,
    `max` SMALLINT UNSIGNED NOT NULL DEFAULT 1,
    PRIMARY KEY (`characterGuid`, `class`, `skill`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

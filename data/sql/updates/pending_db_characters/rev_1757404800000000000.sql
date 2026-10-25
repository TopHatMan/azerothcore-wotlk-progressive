-- Core Vocation Service tables (mod-azeroth-vocations project).
-- See modules/mod-azeroth-vocations/docs/design/core-vocation-service-design.md
-- for the full design this schema implements. Additive only: no column is added
-- to any existing characters-DB table. A character that never uses this system
-- has zero rows in any table below, ever.

CREATE TABLE IF NOT EXISTS `av_core_class_profile` (
  `characterGuid` INT UNSIGNED NOT NULL,
  `class` TINYINT UNSIGNED NOT NULL,
  `level` TINYINT UNSIGNED NOT NULL DEFAULT 1,
  `xp` INT UNSIGNED NOT NULL DEFAULT 0,
  `isActive` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `createdAt` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `updatedAt` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`characterGuid`, `class`),
  KEY `idx_active` (`characterGuid`, `isActive`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `av_core_class_spell` (
  `characterGuid` INT UNSIGNED NOT NULL,
  `class` TINYINT UNSIGNED NOT NULL,
  `spell` INT UNSIGNED NOT NULL,
  `specMask` TINYINT UNSIGNED NOT NULL DEFAULT 1,
  PRIMARY KEY (`characterGuid`, `class`, `spell`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `av_core_class_talent` (
  `characterGuid` INT UNSIGNED NOT NULL,
  `class` TINYINT UNSIGNED NOT NULL,
  `spell` INT UNSIGNED NOT NULL,
  `specMask` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`characterGuid`, `class`, `spell`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `av_core_class_glyph` (
  `characterGuid` INT UNSIGNED NOT NULL,
  `class` TINYINT UNSIGNED NOT NULL,
  `talentGroup` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `glyph1` SMALLINT UNSIGNED DEFAULT 0,
  `glyph2` SMALLINT UNSIGNED DEFAULT 0,
  `glyph3` SMALLINT UNSIGNED DEFAULT 0,
  `glyph4` SMALLINT UNSIGNED DEFAULT 0,
  `glyph5` SMALLINT UNSIGNED DEFAULT 0,
  `glyph6` SMALLINT UNSIGNED DEFAULT 0,
  PRIMARY KEY (`characterGuid`, `class`, `talentGroup`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `av_core_class_action` (
  `characterGuid` INT UNSIGNED NOT NULL,
  `class` TINYINT UNSIGNED NOT NULL,
  `spec` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `button` TINYINT UNSIGNED NOT NULL,
  `action` INT UNSIGNED NOT NULL DEFAULT 0,
  `type` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`characterGuid`, `class`, `spec`, `button`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `av_core_class_armory_item` (
  `characterGuid` INT UNSIGNED NOT NULL,
  `class` TINYINT UNSIGNED NOT NULL,
  `equipSlot` TINYINT UNSIGNED NOT NULL,
  `itemGuid` INT UNSIGNED NOT NULL,
  `storedAt` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`characterGuid`, `class`, `equipSlot`),
  UNIQUE KEY `idx_item` (`itemGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `av_core_class_switch_log` (
  `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  `characterGuid` INT UNSIGNED NOT NULL,
  `fromClass` TINYINT UNSIGNED NOT NULL,
  `toClass` TINYINT UNSIGNED NOT NULL,
  `result` TINYINT UNSIGNED NOT NULL,
  `reason` VARCHAR(255) NULL,
  `initiatedBy` VARCHAR(64) NOT NULL,
  `createdAt` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  KEY `idx_char` (`characterGuid`, `createdAt`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

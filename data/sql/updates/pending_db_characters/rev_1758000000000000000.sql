-- mod-xp-dynamic: SVL character state only.
-- Do not run tables-forxp.sql or tablesforxp2.sql for SVL testing.

CREATE TABLE IF NOT EXISTS `svl_character` (
    `guid` INT UNSIGNED NOT NULL,
    `enabled` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `primary_one` SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    `primary_two` SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='mod-xp-dynamic SVL character state';

CREATE TABLE IF NOT EXISTS `svl_completed_dungeon` (
    `guid` INT UNSIGNED NOT NULL,
    `dungeon` VARCHAR(24) NOT NULL,
    `completed_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (`guid`, `dungeon`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='mod-xp-dynamic SVL dungeon credits';

CREATE TABLE IF NOT EXISTS `svl_gate_credit` (
    `guid` INT UNSIGNED NOT NULL,
    `credit` VARCHAR(32) NOT NULL,
    PRIMARY KEY (`guid`, `credit`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='mod-xp-dynamic SVL non-dungeon credits';

CREATE TABLE IF NOT EXISTS `svl_modes` (
    `guid` INT UNSIGNED NOT NULL,
    `mode` VARCHAR(8) NOT NULL,
    PRIMARY KEY (`guid`, `mode`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='mod-xp-dynamic permanent challenge modes';

CREATE TABLE IF NOT EXISTS `svl_fatigue` (
    `guid` INT UNSIGNED NOT NULL,
    `active_seconds` INT UNSIGNED NOT NULL DEFAULT 0,
    `last_rest_at` TIMESTAMP NULL DEFAULT NULL,
    PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='mod-xp-dynamic SRV active-time fatigue state';

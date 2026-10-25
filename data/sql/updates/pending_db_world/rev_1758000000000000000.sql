-- mod-xp-dynamic: command help entries for SVL.
-- The actual commands are registered by the compiled worldserver module.

DELETE FROM `command` WHERE `name` IN ('svl', 'svl on', 'svl dark', 'svl srv', 'svl tru', 'svl status', 'svl off');
INSERT INTO `command` (`name`, `security`, `help`) VALUES
('svl', 0, 'SVL challenge commands: .svl on, .svl dark, .svl srv, .svl tru, .svl status.'),
('svl on', 0, 'Syntax: .svl on — Permanently enable SVL progression at level 1.'),
('svl dark', 0, 'Syntax: .svl dark — Permanently enable DARK SVL at level 1.'),
('svl srv', 0, 'Syntax: .svl srv — Permanently enable SRV SVL at level 1.'),
('svl tru', 0, 'Syntax: .svl tru — Permanently enable all SVL challenge modes at level 1.'),
('svl status', 0, 'Syntax: .svl status — Show SVL modes, current gate, and missing keys.'),
('svl off', 0, 'Syntax: .svl off — SVL modes are permanent and cannot be disabled by players.');

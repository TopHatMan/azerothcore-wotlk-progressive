-- High Elf race 12: server-side display IDs must exist in the server's
-- CreatureDisplayInfo.dbc. The client patch uses custom client display IDs
-- 40076/40075, but the server must retain the stock Blood Elf display IDs
-- 15476/15475 for collision and player login initialization.
UPDATE `chrraces_dbc`
SET `MaleDisplayId` = 15476,
    `FemaleDisplayId` = 15475
WHERE `ID` = 12;

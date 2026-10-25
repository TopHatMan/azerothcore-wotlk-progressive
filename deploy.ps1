# Pushes a build straight to the mini server over SSH (replaces zipping staging\ by hand).
#
#   Binaries + .conf.dist files -> C:\Apps\WoWServAsh   (the instance that runs)
#   data\ + module data/SQL     -> C:\Apps\WowServ      (WoWServAsh's SourceDirectory)
#
# Live *.conf files are never touched; only the .dist defaults are refreshed.
# By default only files changed since the last successful deploy are sent.
#
#   .\deploy.ps1                 # incremental: binaries + anything changed since last deploy
#   .\deploy.ps1 -Since 2026-10-01
#   .\deploy.ps1 -Full           # everything (data\ is ~800 MB)
#   .\deploy.ps1 -NoBinaries     # SQL/data/conf only
param(
    [switch]$Full,
    [datetime]$Since,
    [switch]$NoBinaries,
    [string]$RemoteHost = "rev2k@192.168.1.93",
    [string]$KeyFile    = "$env:USERPROFILE\.ssh\brang_mini_ed25519"
)

$ErrorActionPreference = "Stop"

$acRoot      = "Z:\ac"
$modulesRoot = "Z:\ac\modules"
$buildBin    = "Z:\ac\build\bin"
$runRoot     = "WoWServAsh"   # under C:\Apps on the server
$srcRoot     = "WowServ"
$stampFile   = "$env:LOCALAPPDATA\ac-deploy\last-deploy.txt"
$sshArgs     = @("-i", $KeyFile, "-o", "IdentitiesOnly=yes", "-o", "BatchMode=yes")

# Modules never sent at all (conf, data, scripts). mod-item-affixes has hand-edited
# update .ps1 scripts on the server that a copy would overwrite.
$excludeModules = @("mod-item-affixes")

# Files hand-edited on the server that must never be overwritten (paths under C:\Apps).
$protectedFiles = @(
    # Commented out on the server on purpose; re-enabling it re-runs a broken update.
    "WowServ\data\sql\updates\db_world\2026_08_08_09.sql",
    # Server copy uses custom High Elf display IDs (40076/40075) and different Goblin skill rows.
    "WowServ\modules\mod-custom-races\data\sql\db-world\mod_custom_races.sql"
)

# ---- 1. Work out the change window ----
if ($Full) {
    $cutoff = [datetime]::MinValue
} elseif ($Since) {
    $cutoff = $Since
} elseif (Test-Path $stampFile) {
    $cutoff = [datetime]::Parse((Get-Content $stampFile -Raw).Trim())
} else {
    throw "No previous deploy recorded. Run with -Since <date> or -Full the first time."
}
$deployStart = Get-Date
Write-Host "Sending files changed since $cutoff" -ForegroundColor Cyan

# ---- 2. Refuse to overwrite a running server ----
if (-not $NoBinaries) {
    $running = & ssh @sshArgs $RemoteHost "powershell -NoProfile -Command `"(Get-Process worldserver,authserver -ErrorAction SilentlyContinue | Where-Object { `$_.Path -like 'C:\Apps\$runRoot\*' }).Name`""
    if ($running) {
        throw "Stop $($running -join ', ') in C:\Apps\$runRoot first (or use -NoBinaries)."
    }
}

# ---- 3. Collect files into a temp tree shaped like C:\Apps ----
$pkg = Join-Path $env:TEMP "ac-deploy-pkg"
Remove-Item -Recurse -Force $pkg -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $pkg | Out-Null
$count = 0

function Add-File([string]$source, [string]$destRelative, [switch]$Always) {
    $item = Get-Item $source
    if (-not $Always -and $item.LastWriteTime -le $cutoff) { return }
    if ($protectedFiles -contains $destRelative) {
        Write-Host "  skipping protected $destRelative" -ForegroundColor Yellow
        return
    }
    $dest = Join-Path $pkg $destRelative
    New-Item -ItemType Directory -Force (Split-Path $dest) | Out-Null
    Copy-Item $item.FullName $dest -Force
    $script:count++
}

function Add-Tree([string]$sourceDir, [string]$destRelative) {
    if (-not (Test-Path $sourceDir)) { return }
    Get-ChildItem $sourceDir -Recurse -File | Where-Object {
        $_.FullName -notmatch '\\\.git\\' -and $_.LastWriteTime -gt $cutoff
    } | ForEach-Object {
        Add-File $_.FullName (Join-Path $destRelative $_.FullName.Substring($sourceDir.Length).TrimStart('\'))
    }
}

# Binaries -> WoWServAsh (always sent unless -NoBinaries)
if (-not $NoBinaries) {
    foreach ($bin in "worldserver.exe", "worldserver.pdb", "authserver.exe", "authserver.pdb") {
        $path = Join-Path $buildBin $bin
        if (-not (Test-Path $path)) { throw "$path not found - build first (see compile.txt)." }
        Add-File $path "$runRoot\$bin" -Always
    }
}

# Core + module .conf.dist -> WoWServAsh\configs
Add-File "$acRoot\src\server\apps\worldserver\worldserver.conf.dist" "$runRoot\configs\worldserver.conf.dist"
Add-File "$acRoot\src\server\apps\authserver\authserver.conf.dist"   "$runRoot\configs\authserver.conf.dist"
Get-ChildItem $modulesRoot -Directory | Where-Object { $excludeModules -notcontains $_.Name } | ForEach-Object {
    $confDir = Join-Path $_.FullName "conf"
    if (Test-Path $confDir) {
        Get-ChildItem $confDir -File -Filter "*.dist" | ForEach-Object {
            Add-File $_.FullName "$runRoot\configs\modules\$($_.Name)"
        }
    }
}

# Core data\ -> WowServ\data
Add-Tree "$acRoot\data" "$srcRoot\data"

# Module folders except src/conf -> WowServ\modules (mod-azeroth-eras keeps src: its patch SQL lives there)
Get-ChildItem $modulesRoot -Directory | Where-Object { $excludeModules -notcontains $_.Name } | ForEach-Object {
    $modName = $_.Name
    $excluded = if ($modName -eq "mod-azeroth-eras") { @("conf") } else { @("src", "conf") }
    Get-ChildItem $_.FullName -Directory | Where-Object {
        $excluded -notcontains $_.Name -and $_.Name -ne ".git"
    } | ForEach-Object {
        Add-Tree $_.FullName "$srcRoot\modules\$modName\$($_.Name)"
    }
}

if ($count -eq 0) {
    Write-Host "Nothing changed since $cutoff." -ForegroundColor Yellow
    return
}

Write-Host "Packaging $count file(s):" -ForegroundColor Green
Get-ChildItem $pkg -Recurse -File | ForEach-Object { "  " + $_.FullName.Substring($pkg.Length + 1) }

# ---- 4. Ship and extract ----
$tarFile = Join-Path $env:TEMP "ac-deploy.tar"
Remove-Item $tarFile -Force -ErrorAction SilentlyContinue
& "$env:SystemRoot\System32\tar.exe" -cf $tarFile -C $pkg .
if ($LASTEXITCODE -ne 0) { throw "tar failed" }

& scp @sshArgs $tarFile "${RemoteHost}:C:/Apps/ac-deploy.tar"
if ($LASTEXITCODE -ne 0) { throw "scp failed" }

# Keep the previous binaries as *.prev so a bad build can be rolled back by renaming.
$remote = "cd /d C:\Apps"
if (-not $NoBinaries) {
    foreach ($bin in "worldserver.exe", "worldserver.pdb", "authserver.exe", "authserver.pdb") {
        $remote += " && (if exist $runRoot\$bin copy /Y $runRoot\$bin $runRoot\$bin.prev >nul)"
    }
}
$remote += " && tar -xf ac-deploy.tar && del ac-deploy.tar"
& ssh @sshArgs $RemoteHost $remote
if ($LASTEXITCODE -ne 0) { throw "Remote extract failed" }

New-Item -ItemType Directory -Force (Split-Path $stampFile) | Out-Null
Set-Content $stampFile $deployStart.ToString("o")
Remove-Item -Recurse -Force $pkg, $tarFile -ErrorAction SilentlyContinue
Write-Host "`nDeployed $count file(s). Start worldserver from C:\Apps\$runRoot to test." -ForegroundColor Green

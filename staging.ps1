$modulesRoot  = "Z:\ac\modules"
$buildDir     = "Z:\ac\build"
$acRoot       = "Z:\ac"
$staging      = "Z:\ac\staging"

# Clean and recreate staging
Remove-Item -Recurse -Force $staging -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force "$staging\bin"          | Out-Null
New-Item -ItemType Directory -Force "$staging\conf\modules" | Out-Null
New-Item -ItemType Directory -Force "$staging\sql"          | Out-Null
New-Item -ItemType Directory -Force "$staging\data"         | Out-Null
New-Item -ItemType Directory -Force "$staging\modules"      | Out-Null

# „Ÿ„Ÿ 1. Binaries from build „Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ
Write-Host "Copying binaries..." -ForegroundColor Green
Copy-Item "$buildDir\bin\worldserver.exe" "$staging\bin\" -Force
Copy-Item "$buildDir\bin\worldserver.pdb" "$staging\bin\" -Force
Copy-Item "$buildDir\bin\authserver.exe"  "$staging\bin\" -Force
Copy-Item "$buildDir\bin\authserver.pdb"  "$staging\bin\" -Force

# „Ÿ„Ÿ 2. Main conf files „Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ
Write-Host "Copying main conf files..." -ForegroundColor Green
Copy-Item "$acRoot\src\server\apps\worldserver\worldserver.conf.dist" "$staging\conf\" -Force
Copy-Item "$acRoot\src\server\apps\authserver\authserver.conf.dist"   "$staging\conf\" -Force
Copy-Item "$acRoot\src\tools\dbimport\dbimport.conf.dist"             "$staging\conf\" -Force

# „Ÿ„Ÿ 3. Module conf files ¨ conf\modules „Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ
Write-Host "Copying module confs..." -ForegroundColor Cyan
Get-ChildItem $modulesRoot -Directory | ForEach-Object {
    $confSrc = Join-Path $_.FullName "conf"
    if (Test-Path $confSrc) {
        Write-Host "  [$($_.Name)] conf" -ForegroundColor Cyan
        Copy-Item -Path "$confSrc\*" -Destination "$staging\conf\modules" -Recurse -Force
    }
}

# „Ÿ„Ÿ 4. Module subfolders excluding src and conf „Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ
$excludedFolders = @("src", "conf")
Write-Host "Copying module assets (excluding src and conf)..." -ForegroundColor Magenta
Get-ChildItem $modulesRoot -Directory | ForEach-Object {
    $modName = $_.Name
    $modRoot = $_.FullName

    Get-ChildItem $modRoot -Directory | Where-Object { 
        $excludedFolders -notcontains $_.Name 
    } | ForEach-Object {
        $folderName = $_.Name
        $dst = "$staging\modules\$modName\$folderName"
        New-Item -ItemType Directory -Force $dst | Out-Null
        Write-Host "  [$modName\$folderName]" -ForegroundColor Magenta
        Copy-Item -Path "$($_.FullName)\*" -Destination $dst -Recurse -Force
    }
}

# „Ÿ„Ÿ 5. Root AC data folder „Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ
Write-Host "Copying root data folder..." -ForegroundColor Green
if (Test-Path "$acRoot\data") {
    Copy-Item -Path "$acRoot\data\*" -Destination "$staging\data" -Recurse -Force
}

Write-Host "`nDone! Staging at $staging" -ForegroundColor Green
Write-Host "bin\     - worldserver.exe + authserver.exe" -ForegroundColor White
Write-Host "conf\    - main confs + modules\"            -ForegroundColor White
Write-Host "modules\ - all module subfolders except src and conf" -ForegroundColor White
Write-Host "data\    - all data files"                   -ForegroundColor White
$startTime = Get-Date

if (-Not (Test-Path -Path .\x64\Release\)) {
    New-Item -ItemType Directory -Path .\x64\Release\
}

windres .\psb_bg\psb_bg.rc -O coff -o .\psb_bg\psb_bg.res
g++ .\psb_bg\main.cpp .\RwEverything\RwEverything.hpp .\RwEverything\RwEverything.cpp .\RwEverything\driver.c .\RwEverything\driver.h .\psb_bg\psb_bg.res -Os -o .\x64\Release\psb_bg.exe

$endTime = Get-Date
$duration = $endTime - $startTime
Write-Host "Build elapsed time: $($duration.TotalSeconds) seconds"
Write-Host ""
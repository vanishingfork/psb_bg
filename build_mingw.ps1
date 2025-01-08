$startTime = Get-Date

windres .\psb_bg\psb_bg.rc -O coff -o .\psb_bg\psb_bg.res
g++ .\psb_bg\main.cpp .\RwEverything\RwEverything.hpp .\RwEverything\RwEverything.cpp .\RwEverything\driver.cpp .\RwEverything\driver.hpp .\psb_bg\psb_bg.res -o .\x64\Release\psb_bg.exe

$endTime = Get-Date
$duration = $endTime - $startTime
Write-Host "Build elapsed time: $($duration.TotalSeconds) seconds"
Write-Host ""
.\x64\Release\psb_bg.exe
# Start the game with the mod and load a save slot (Title > Load > Load > slot N > OK), slots 1-8.
# The Load screen remembers its cursor, so the script reads it (RVA 0x1261734)
# and moves to the slot. Prints the spoken lines from the run.
param([int]$Slot = 1, [int]$Wait = 24)
$ctl = "$PSScriptRoot\gamectl.py"
$mem = "$PSScriptRoot\..\re\memdiff.py"
Get-Process Game -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep 2
Start-Process -FilePath "s:\steam\steamapps\common\STEINS;GATE\SteinsGateAccess\SGALaunch.exe"
Start-Sleep $Wait
foreach ($k in 'enter', 'down', 'down', 'enter', 'down', 'enter') {
    python $ctl keys $k wait:1800 2>$null | Out-Null
}
python "$PSScriptRoot\slot.py" $Slot confirm | Out-Null
Start-Sleep 4
Get-Content "$env:LOCALAPPDATA\SteinsGateAccess\game\SteinsGateAccess\spoken.log" | Select-Object -Skip 8

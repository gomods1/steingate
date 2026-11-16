# Builds the mod and lays out dist\ as it is installed in the game folder:
#   dist\SteinsGateAccess\SGALaunch.exe          (x86) starts the game with the mod
#   dist\dinput8.dll                             (x86) the mod, a DirectInput8 proxy in the game folder
#   dist\SteinsGateAccess\SpeechHost.exe + prism.dll (x64) speech
#   dist\SteinsGateAccess\data\..., LICENSES\...
# and build\x86\dinput8.map (the mod's map, for crash offsets).
# dist\ is emptied first, so it holds exactly one build (tools\package.py zips it).
# PRISM may come from the environment (the release workflow downloads Prism and sets it);
# Visual Studio is found with vswhere when the Community 2022 path is missing.
param([switch]$Install)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$vcvars = 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat'
if (-not (Test-Path $vcvars)) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    $vcvars = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find 'VC\Auxiliary\Build\vcvarsall.bat' | Select-Object -First 1
    if (-not $vcvars) { throw "Visual Studio with the C++ tools not found" }
}
$prism = if ($env:PRISM) { $env:PRISM } else { 'C:\Users\Nitropc\code\libs\prism\prism-windows-x64' }
$obj = "$root\build"
$out = "$root\dist\SteinsGateAccess"
if (Test-Path "$root\dist") { Remove-Item "$root\dist" -Recurse -Force }
New-Item -ItemType Directory -Force "$obj\x86", "$obj\x64", "$obj\launcher", "$out\data" | Out-Null
$flags = '/nologo /O2 /MT /std:c++17 /utf-8 /W3 /DUNICODE /D_UNICODE'

$mh = "$root\third_party\minhook"
$modSrc = ((Get-ChildItem "$root\src\mod\*.cpp") + (Get-Item "$mh\src\buffer.c", "$mh\src\hook.c", "$mh\src\trampoline.c", "$mh\src\hde\hde32.c") | ForEach-Object { '"' + $_.FullName + '"' }) -join ' '
cmd /c "`"$vcvars`" x86 >nul && cl $flags /LD /EHa /I`"$mh\include`" /Fo`"$obj\x86\\`" /Fe`"$obj\x86\dinput8.dll`" $modSrc /link /DEF:`"$root\src\mod\dinput8.def`" /MAP:`"$obj\x86\dinput8.map`" dxguid.lib user32.lib"
if ($LASTEXITCODE) { throw "mod build failed" }

cmd /c "`"$vcvars`" x86 >nul && cl $flags /EHsc /Fo`"$obj\launcher\\`" /Fe`"$obj\launcher\SGALaunch.exe`" `"$root\src\launcher\main.cpp`" /link /SUBSYSTEM:WINDOWS user32.lib shell32.lib ole32.lib"
if ($LASTEXITCODE) { throw "launcher build failed" }

cmd /c "`"$vcvars`" x64 >nul && cl $flags /EHsc /I`"$prism\include`" /Fo`"$obj\x64\\`" /Fe`"$obj\x64\SpeechHost.exe`" `"$root\src\speechhost\main.cpp`" /link /SUBSYSTEM:WINDOWS `"$prism\dynamic\release\lib\prism.lib`" shell32.lib delayimp.lib /DELAYLOAD:prism.dll"
if ($LASTEXITCODE) { throw "speech host build failed" }

Copy-Item "$obj\x86\dinput8.dll" "$root\dist\" -Force
Copy-Item "$obj\launcher\SGALaunch.exe", "$obj\x64\SpeechHost.exe" $out -Force
Copy-Item "$prism\dynamic\release\bin\prism.dll" $out -Force
Copy-Item "$root\data\*" "$out\data\" -Recurse -Force
New-Item -ItemType Directory -Force "$out\LICENSES" | Out-Null
Copy-Item "$prism\LICENSES\*" "$out\LICENSES\" -Recurse -Force
Copy-Item "$prism\NOTICE" "$out\LICENSES\prism-NOTICE" -Force
Copy-Item "$root\third_party\sc3tools\LICENSE" "$out\LICENSES\sc3tools-charset-LICENSE" -Force
Copy-Item "$root\third_party\minhook\LICENSE.txt" "$out\LICENSES\minhook-LICENSE.txt" -Force
if (Test-Path "$root\README.md") { Copy-Item "$root\README.md" "$out\README.md" -Force }
Write-Host "built into $out"

if ($Install) {
    Get-Process Game, SpeechHost -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep 1
    $target = Join-Path 's:\steam\steamapps\common\STEINS;GATE' 'SteinsGateAccess'
    New-Item -ItemType Directory -Force -Path $target | Out-Null
    Copy-Item -Path "$out\*" -Destination $target -Recurse -Force
    Copy-Item -LiteralPath "$root\dist\dinput8.dll" -Destination (Split-Path $target) -Force
    Remove-Item -LiteralPath (Join-Path $target "SteinsGateAccess.dll") -ErrorAction SilentlyContinue
    # A stale SGALaunch.exe (Oct 24 build, no junction) once survived an install
    # and silently started the game without the mod: verify every binary.
    $checks = @(
        @("$out\SGALaunch.exe", (Join-Path $target "SGALaunch.exe")),
        @("$out\SpeechHost.exe", (Join-Path $target "SpeechHost.exe")),
        @("$root\dist\dinput8.dll", (Join-Path (Split-Path $target) "dinput8.dll")))
    foreach ($c in $checks) {
        if ((Get-FileHash -LiteralPath $c[0]).Hash -ne (Get-FileHash -LiteralPath $c[1]).Hash) {
            throw "install mismatch: $($c[1]) differs from $($c[0])"
        }
    }
    Write-Host "installed into $target"
}

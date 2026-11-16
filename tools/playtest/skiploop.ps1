# Fast-forward the story: hold Control in one-minute chunks; when the last
# spoken line has not changed after a chunk, the game is waiting: at a Phone
# Trigger wait the phone must be opened and the new mail read (phone.py
# readmail), elsewhere Enter continues. Stops early when the phone shows
# interactive-mail links (reply words) so they can be tested.
param([int]$Minutes = 10)
$ctl = "$PSScriptRoot\gamectl.py"
$phone = "$PSScriptRoot\phone.py"
$log = "$env:LOCALAPPDATA\SteinsGateAccess\game\SteinsGateAccess\spoken.log"
$sga = "$env:LOCALAPPDATA\SteinsGateAccess\game\SteinsGateAccess\sga.log"
$prev = ''
function LinksSeen { Select-String -Path $sga -Pattern '^\[2 (8[89]\d|9\d\d|1\d\d\d),.*\{' -Quiet }
for ($i = 1; $i -le $Minutes; $i++) {
    python $ctl keys hold:ctrl:60000 wait:500 2>$null | Out-Null
    $last = Get-Content $log | Select-Object -Last 1
    "chunk ${i}: $last"
    if (LinksSeen) { "interactive mail with links seen"; break }
    if ($last -eq $prev) {
        "no progress: opening the phone to read mail"
        python $phone readmail 2>$null
        Start-Sleep 4
        if (LinksSeen) { "interactive mail with links seen"; break }
        python $phone close 2>$null | Out-Null
        python $ctl keys enter wait:1500 2>$null | Out-Null
    }
    $prev = $last
}

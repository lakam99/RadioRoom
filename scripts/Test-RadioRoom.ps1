param([string]$Build="build",[string]$Executable,[string]$Plugin)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if(!$Executable){$Executable=Join-Path $root "$Build/RadioRoomStandalone_artefacts/Release/RadioRoom.exe"}
if(!$Plugin){$Plugin=Join-Path $root "$Build/RadioRoom_artefacts/Release/VST3/RadioRoom.vst3"}
$tests=Join-Path $root 'tests'
New-Item -ItemType Directory -Force $tests,(Join-Path $tests 'output') | Out-Null
function Invoke-Checked([string[]]$Arguments,[int]$Timeout=60000){
    $quoted=$Arguments | ForEach-Object {'"'+$_+'"'}
    $p=Start-Process -FilePath $Executable -ArgumentList $quoted -WorkingDirectory $root -WindowStyle Hidden -PassThru
    if(!$p.WaitForExit($Timeout)){throw "RadioRoom test helper timed out (PID $($p.Id)); inspect it before retrying."}
    if($p.ExitCode -ne 0){throw "RadioRoom returned $($p.ExitCode). See reports in $tests."}
}
Invoke-Checked @('--test',(Join-Path $tests 'dsp-report.json'))
Invoke-Checked @('--updater-test',(Join-Path $tests 'updater-report.json'))
Invoke-Checked @('--host',([IO.Path]::GetFullPath($Plugin)),'--report',(Join-Path $tests 'host-report.json'))
for($skin=0;$skin -lt 3;$skin++){Invoke-Checked @('--snapshot',(Join-Path $tests "output/RadioRoom-skin-$skin.png"),'--skin',"$skin")}
Get-Content (Join-Path $tests 'dsp-report.json')
Get-Content (Join-Path $tests 'host-report.json')

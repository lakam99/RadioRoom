param([string]$InstallRoot = (Join-Path $env:LOCALAPPDATA 'Programs/RadioRoom'), [switch]$NoRegistration, [switch]$NoShortcuts)
. (Join-Path $PSScriptRoot 'Release-Common.ps1')
$mutex=New-Object Threading.Mutex($false,'Local\RadioRoom-Installer')
$locked=$false
try {
try {$locked=$mutex.WaitOne(0)} catch [Threading.AbandonedMutexException] {$locked=$true}
if (-not $locked) {throw 'Another RadioRoom installation is in progress.'}
$InstallRoot = [IO.Path]::GetFullPath($InstallRoot).TrimEnd('\','/')
Assert-OwnedDirectory $InstallRoot '.radioroom-install.json'
$info = Get-Content -LiteralPath (Join-Path $InstallRoot '.radioroom-install.json') -Raw | ConvertFrom-Json
if ($info.installRoot -ine $InstallRoot -or [IO.Path]::GetFileName($InstallRoot) -ine 'RadioRoom') { throw 'Installation path does not match its ownership marker.' }
$vst = Assert-ChildPath (Join-Path $info.vst3Root 'RadioRoom.vst3') $info.vst3Root
Assert-OwnedDirectory $vst '.radioroom-owner.json'
Assert-Unlocked (Join-Path $InstallRoot 'RadioRoom.exe')
Assert-Unlocked (Join-Path $vst 'Contents/x86_64-win/RadioRoom.vst3')
$targets = @(@($vst,$info.vst3Root,'.radioroom-owner.json'), @($InstallRoot,(Split-Path -Parent $InstallRoot),'.radioroom-install.json'))
$vstBackupRoot=Join-Path (Split-Path -Parent $info.vst3Root) 'RadioRoom-Backups'
foreach ($pair in @(@($info.previousApp,(Split-Path -Parent $InstallRoot),'RadioRoom.backup-','.radioroom-install.json'),@($info.previousVst,$vstBackupRoot,'RadioRoom.vst3.backup-','.radioroom-owner.json'))) {
    if ($pair[0] -and (Test-Path -LiteralPath $pair[0])) {
        $candidate = Assert-ChildPath $pair[0] $pair[1]
        if (-not [IO.Path]::GetFileName($candidate).StartsWith($pair[2],[StringComparison]::Ordinal)) { throw 'Unexpected backup path.' }
        $targets += ,@($candidate,$pair[1],$pair[3])
    }
}
foreach ($pair in $targets) { if (Test-Path -LiteralPath $pair[0]) { $null=Assert-ChildPath $pair[0] $pair[1]; Assert-OwnedDirectory $pair[0] $pair[2] } }
if (-not $NoShortcuts) {
    $link = Join-Path ([Environment]::GetFolderPath('Programs')) 'RadioRoom.lnk'
    if (Test-Path -LiteralPath $link) {
        $shell = New-Object -ComObject WScript.Shell
        if ($shell.CreateShortcut($link).TargetPath -ieq (Join-Path $InstallRoot 'RadioRoom.exe')) { Remove-Item -LiteralPath $link -Force }
    }
}
if (-not $NoRegistration) {
    $key='HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\RadioRoom'
    if ((Get-ItemProperty -LiteralPath $key -Name InstallLocation -ErrorAction SilentlyContinue).InstallLocation -ieq $InstallRoot) { Remove-Item -LiteralPath $key -Recurse }
}
foreach ($pair in $targets) { if (Test-Path -LiteralPath $pair[0]) { $null=Assert-ChildPath $pair[0] $pair[1]; Remove-Item -LiteralPath $pair[0] -Recurse -Force } }
Write-Output 'RadioRoom removed. User settings and recordings were preserved.'
} finally {if ($locked) {$mutex.ReleaseMutex()};$mutex.Dispose()}

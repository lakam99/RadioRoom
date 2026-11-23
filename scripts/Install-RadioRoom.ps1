param(
    [string]$PackageRoot = $PSScriptRoot,
    [string]$InstallRoot = (Join-Path $env:LOCALAPPDATA 'Programs/RadioRoom'),
    [string]$Vst3Root = (Join-Path $env:LOCALAPPDATA 'Programs/Common/VST3'),
    [int]$WaitForProcess = 0,
    [switch]$Relaunch,
    [switch]$NoShortcuts,
    [switch]$NoRegistration,
    [string]$Report
)
. (Join-Path $PSScriptRoot 'Release-Common.ps1')
$mutex = New-Object Threading.Mutex($false, 'Local\RadioRoom-Installer')
$locked = $false
$appSwitched = $false; $vstSwitched = $false
$appMoved = $false; $vstMoved = $false
$stageApp = $null; $stageVst = $null
try {
    try { $locked = $mutex.WaitOne(0) } catch [Threading.AbandonedMutexException] { $locked=$true }
    if (-not $locked) { throw 'Another RadioRoom installation is in progress.' }
    $manifest = Get-VerifiedPackage $PackageRoot
    $InstallRoot = [IO.Path]::GetFullPath($InstallRoot).TrimEnd('\','/')
    $Vst3Root = [IO.Path]::GetFullPath($Vst3Root).TrimEnd('\','/')
    if ([IO.Path]::GetFileName($InstallRoot) -ine 'RadioRoom') { throw 'The application directory must be named RadioRoom.' }
    $appParent = Split-Path -Parent $InstallRoot
    if ($Vst3Root -eq [IO.Path]::GetPathRoot($Vst3Root).TrimEnd('\','/')) { throw 'Choose a VST3 directory rather than a drive root.' }
    $vstBackupRoot = Join-Path (Split-Path -Parent $Vst3Root) 'RadioRoom-Backups'
    if ($vstBackupRoot -ieq $Vst3Root) { throw 'The VST3 directory cannot be the backup directory.' }
    if ((Test-Path -LiteralPath $vstBackupRoot) -and ((Get-Item -LiteralPath $vstBackupRoot -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'The backup directory must not be a filesystem link.' }
    $vstTarget = Assert-ChildPath (Join-Path $Vst3Root 'RadioRoom.vst3') $Vst3Root
    if ($vstTarget.StartsWith($InstallRoot + '\', [StringComparison]::OrdinalIgnoreCase) -or $InstallRoot.StartsWith($vstTarget + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Application and VST3 directories must be separate.' }
    $null = Assert-ChildPath $InstallRoot $appParent
    Assert-Unlocked (Join-Path $InstallRoot 'RadioRoom.exe')
    Assert-Unlocked (Join-Path $vstTarget 'Contents/x86_64-win/RadioRoom.vst3')
    # The private workstation prototype predates ownership metadata. Recognise
    # that exact pair of binaries, never adopt a directory merely by its name.
    if((Test-Path -LiteralPath $InstallRoot) -and !(Test-Path -LiteralPath (Join-Path $InstallRoot '.radioroom-install.json'))){
        if(!(Test-Path -LiteralPath $vstTarget) -or (Test-Path -LiteralPath (Join-Path $vstTarget '.radioroom-owner.json'))){throw 'Legacy installation is incomplete.'}
        foreach($dir in @($InstallRoot,$vstTarget)){
            if((Get-Item -LiteralPath $dir -Force).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Legacy directories cannot be links.'}
            foreach($item in Get-ChildItem -LiteralPath $dir -Recurse -Force){if($item.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Legacy directories cannot contain links.'}}
        }
        foreach($item in Get-ChildItem -LiteralPath $InstallRoot -Force){if($item.Name -notin @('RadioRoom.exe','README.md','LICENSE.txt','JUCE-LICENSE.md') -or $item.PSIsContainer){throw 'Unrecognised files in legacy application directory.'}}
        if((Get-FileHash -LiteralPath (Join-Path $InstallRoot 'RadioRoom.exe')).Hash -ne '8C117BC4FEF312CBE37316F14D0D4805810809CDACDE6CE7DD2B94408C66EA23' -or (Get-FileHash -LiteralPath (Join-Path $vstTarget 'Contents/x86_64-win/RadioRoom.vst3')).Hash -ne '849BCFEE250818E5E48E64C210E381F7B2A8F9183A4D1DB41B7930808D9FB5E0'){throw 'Unrecognised legacy RadioRoom binaries.'}
        @{product=$script:RadioRoomProduct;repository=$script:RadioRoomRepository;version='0.1.0';installRoot=$InstallRoot;vst3Root=$Vst3Root}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $InstallRoot '.radioroom-install.json') -Encoding UTF8
        @{product=$script:RadioRoomProduct;repository=$script:RadioRoomRepository;version='0.1.0'}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $vstTarget '.radioroom-owner.json') -Encoding UTF8
    }
    Assert-OwnedDirectory $InstallRoot '.radioroom-install.json'
    Assert-OwnedDirectory $vstTarget '.radioroom-owner.json'
    if ($WaitForProcess -gt 0) {
        $parent = Get-Process -Id $WaitForProcess -ErrorAction SilentlyContinue
        if ($parent) {
            if ([IO.Path]::GetFullPath($parent.Path) -ine (Join-Path $InstallRoot 'RadioRoom.exe')) { throw 'Refusing to wait for an unrelated process.' }
            if (-not $parent.WaitForExit(120000)) { throw 'RadioRoom did not close; update was not installed.' }
        }
    }
    Assert-Unlocked (Join-Path $InstallRoot 'RadioRoom.exe')
    Assert-Unlocked (Join-Path $vstTarget 'Contents/x86_64-win/RadioRoom.vst3')
    $old = $null
    if (Test-Path -LiteralPath (Join-Path $InstallRoot '.radioroom-install.json')) { $old = Get-Content -LiteralPath (Join-Path $InstallRoot '.radioroom-install.json') -Raw | ConvertFrom-Json }
    if ($old -and (Get-StableVersion $old.version) -gt (Get-StableVersion $manifest.version)) { throw 'Refusing to downgrade an existing installation.' }
    $id = [guid]::NewGuid().ToString('N')
    $stageApp = Assert-ChildPath ($InstallRoot + '.stage-' + $id) $appParent
    $stageVst = Assert-ChildPath ($vstTarget + '.stage-' + $id) $Vst3Root
    $backupApp = Assert-ChildPath ($InstallRoot + '.backup-' + $id) $appParent
    $backupVst = Assert-ChildPath (Join-Path $vstBackupRoot ('RadioRoom.vst3.backup-' + $id)) $vstBackupRoot
    $null = New-Item -ItemType Directory -Path $appParent,$Vst3Root -Force
    Copy-Item -LiteralPath (Join-Path $PackageRoot 'Standalone') -Destination $stageApp -Recurse
    Copy-Item -LiteralPath (Join-Path $PackageRoot 'VST3/RadioRoom.vst3') -Destination $stageVst -Recurse
    foreach ($entry in $manifest.files.PSObject.Properties) {
        if ($entry.Name.StartsWith('Standalone/')) { $path = Join-Path $stageApp $entry.Name.Substring(11) }
        else { $path = Join-Path $stageVst $entry.Name.Substring(20) }
        if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ine $entry.Value) { throw 'Staged payload verification failed.' }
    }
    $info = [ordered]@{ product=$script:RadioRoomProduct; repository=$script:RadioRoomRepository; version=$manifest.version; installRoot=$InstallRoot; vst3Root=$Vst3Root; previousApp=$null; previousVst=$null }
    if (Test-Path -LiteralPath $InstallRoot) { Move-Item -LiteralPath $InstallRoot -Destination $backupApp; $appMoved=$true; $info.previousApp=$backupApp }
    if (Test-Path -LiteralPath $vstTarget) { $null=New-Item -ItemType Directory -Path $vstBackupRoot -Force; Move-Item -LiteralPath $vstTarget -Destination $backupVst; $vstMoved=$true; $info.previousVst=$backupVst }
    $info | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $stageApp '.radioroom-install.json') -Encoding UTF8
    @{ product=$script:RadioRoomProduct; repository=$script:RadioRoomRepository; version=$manifest.version } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $stageVst '.radioroom-owner.json') -Encoding UTF8
    Move-Item -LiteralPath $stageApp -Destination $InstallRoot; $appSwitched=$true
    Move-Item -LiteralPath $stageVst -Destination $vstTarget; $vstSwitched=$true
    if (-not $NoShortcuts) {
        $shell = New-Object -ComObject WScript.Shell
        $shortcut = $shell.CreateShortcut((Join-Path ([Environment]::GetFolderPath('Programs')) 'RadioRoom.lnk'))
        $shortcut.TargetPath = Join-Path $InstallRoot 'RadioRoom.exe'; $shortcut.WorkingDirectory=$InstallRoot; $shortcut.Save()
    }
    if (-not $NoRegistration) {
        $key = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\RadioRoom'
        $null = New-Item -Path $key -Force
        $values = @{ DisplayName='RadioRoom'; DisplayVersion=$manifest.version; Publisher='Lakam Audio'; InstallLocation=$InstallRoot; DisplayIcon=(Join-Path $InstallRoot 'RadioRoom.exe'); UninstallString=('powershell.exe -NoProfile -ExecutionPolicy Bypass -File "' + (Join-Path $InstallRoot 'Uninstall-RadioRoom.ps1') + '" -InstallRoot "' + $InstallRoot + '"') }
        foreach ($name in $values.Keys) { Set-ItemProperty -LiteralPath $key -Name $name -Value $values[$name] }
    }
    # Keep the immediately previous version. Delete only older owned backups.
    if ($old) {
        foreach ($pair in @(@($old.previousApp,$appParent,'RadioRoom.backup-','.radioroom-install.json'), @($old.previousVst,$vstBackupRoot,'RadioRoom.vst3.backup-','.radioroom-owner.json'))) {
            if ($pair[0] -and (Test-Path -LiteralPath $pair[0])) {
                try {
                    $obsolete = Assert-ChildPath $pair[0] $pair[1]
                    if ([IO.Path]::GetFileName($obsolete).StartsWith($pair[2],[StringComparison]::Ordinal)) { Assert-OwnedDirectory $obsolete $pair[3]; Remove-Item -LiteralPath $obsolete -Recurse -Force }
                } catch { Write-Warning 'An older backup is still in use and was retained.' }
            }
        }
    }
    $result = @{ passed=$true; version=$manifest.version; installRoot=$InstallRoot; vst3=(Join-Path $vstTarget 'Contents/x86_64-win/RadioRoom.vst3') }
    if ($Report) { $result | ConvertTo-Json | Set-Content -LiteralPath $Report -Encoding UTF8 }
    Write-Output ('Installed RadioRoom ' + $manifest.version)
    if ($Relaunch) { Start-Process -FilePath (Join-Path $InstallRoot 'RadioRoom.exe') -WorkingDirectory $InstallRoot -WindowStyle Hidden }
} catch {
    $failure = $_
    # Roll back only directories created by this transaction, inside checked parents.
    if ($vstSwitched) { $null=Assert-ChildPath $vstTarget $Vst3Root; Remove-Item -LiteralPath $vstTarget -Recurse -Force }
    if ($appSwitched) { $null=Assert-ChildPath $InstallRoot $appParent; Remove-Item -LiteralPath $InstallRoot -Recurse -Force }
    if ($vstMoved) { Move-Item -LiteralPath $backupVst -Destination $vstTarget }
    if ($appMoved) { Move-Item -LiteralPath $backupApp -Destination $InstallRoot }
    if ($Report) { @{passed=$false; error=$failure.Exception.Message} | ConvertTo-Json | Set-Content -LiteralPath $Report -Encoding UTF8 }
    if ($Relaunch -and (Test-Path -LiteralPath (Join-Path $InstallRoot 'RadioRoom.exe'))) { Start-Process -FilePath (Join-Path $InstallRoot 'RadioRoom.exe') -WindowStyle Hidden }
    throw $failure
} finally {
    foreach ($pair in @(@($stageApp,$appParent),@($stageVst,$Vst3Root))) {
        if ($pair[0] -and (Test-Path -LiteralPath $pair[0])) { $null=Assert-ChildPath $pair[0] $pair[1]; Remove-Item -LiteralPath $pair[0] -Recurse -Force }
    }
    if ($locked) { $mutex.ReleaseMutex() }; $mutex.Dispose()
}

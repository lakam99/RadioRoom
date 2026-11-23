param([string]$Build='build')
$ErrorActionPreference='Stop'
& cmake -S $PSScriptRoot -B (Join-Path $PSScriptRoot $Build) -G 'Visual Studio 17 2022' -A x64
if($LASTEXITCODE -ne 0){throw 'CMake configuration failed'}
& cmake --build (Join-Path $PSScriptRoot $Build) --config Release --target RadioRoom_VST3 RadioRoomStandalone --parallel 4
if($LASTEXITCODE -ne 0){throw 'CMake build failed'}

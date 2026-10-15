param(
  [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
  [ValidateSet('x64', 'x86')][string]$Architecture = 'x64',
  [switch]$Sanitize
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'MSVC build tools not found' }
$devcmd = Join-Path $installation 'Common7\Tools\VsDevCmd.bat'
$quotedDevcmd = [char]34 + $devcmd + [char]34
$envLines = & $env:ComSpec /d /c "$quotedDevcmd -no_logo -arch=$Architecture -host_arch=x64 >nul && set"
foreach ($envLine in $envLines) {
  if ($envLine -match '^([^=]+)=(.*)$') {
    [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process')
  }
}
$env:VSLANG = '1033'
$suffix = if ($Sanitize) { '-asan' } else { '' }
$buildDir = Join-Path $root "build\tests-$Architecture-$Configuration$suffix-msvc"
$options = @('--fresh', '-S', $root, '-B', $buildDir, '-G', 'Ninja',
  "-DCMAKE_BUILD_TYPE=$Configuration", '-DLOGGER_BUILD_EXAMPLE=ON')
$ninja = Join-Path $installation 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
$options += "-DCMAKE_MAKE_PROGRAM=$ninja"
if ($Sanitize) { $options += '-DLOGGER_SANITIZE=ON' }
& cmake @options
if ($LASTEXITCODE) { exit $LASTEXITCODE }
& cmake --build $buildDir
if ($LASTEXITCODE) { exit $LASTEXITCODE }
& ctest --test-dir $buildDir --output-on-failure
exit $LASTEXITCODE

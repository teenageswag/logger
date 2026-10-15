param(
  [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
  [ValidateSet('x64', 'x86')][string]$Architecture = 'x64',
  [switch]$Sanitize,
  [switch]$Benchmarks
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
$buildDir = Join-Path $root "build\tests-$Architecture-$Configuration$suffix-vs"
$platform = if ($Architecture -eq 'x86') { 'Win32' } else { 'x64' }
$options = @('-S', $root, '-B', $buildDir, '-G', 'Visual Studio 18 2026',
  '-A', $platform, '-DLOGGER_BUILD_EXAMPLE=ON')
if ($Sanitize) { $options += '-DLOGGER_SANITIZE=ON' }
if ($Benchmarks) { $options += '-DLOGGER_BUILD_BENCHMARKS=ON' }
& cmake @options
if ($LASTEXITCODE) { exit $LASTEXITCODE }
& cmake --build $buildDir --config $Configuration --parallel 4
if ($LASTEXITCODE) { exit $LASTEXITCODE }
& ctest --test-dir $buildDir -C $Configuration --output-on-failure
if ($LASTEXITCODE) { exit $LASTEXITCODE }
if ($Benchmarks) {
  Push-Location -LiteralPath $buildDir
  try {
    & (Join-Path $buildDir "$Configuration\logger_benchmark.exe") |
      Tee-Object -FilePath (Join-Path $buildDir 'benchmark.csv')
    if ($LASTEXITCODE) { exit $LASTEXITCODE }
  } finally { Pop-Location }
}
exit $LASTEXITCODE

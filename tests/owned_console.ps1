param(
  [Parameter(Mandatory)][string]$HostExe,
  [Parameter(Mandatory)][string]$Dll
)
$ErrorActionPreference = 'Stop'
$quotedDll = [char]34 + $Dll + [char]34
$child = Start-Process -FilePath $HostExe -ArgumentList @($quotedDll, 'owned') -WindowStyle Hidden -PassThru
if (-not $child.WaitForExit(15000)) {
  Stop-Process -Id $child.Id -Force
  throw 'Owned-console DLL test timed out'
}
exit $child.ExitCode

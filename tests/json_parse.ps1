param([Parameter(Mandatory)][string]$Path)
$ErrorActionPreference = 'Stop'
$value = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json -AsHashtable -DateKind String
if ($value.timestamp -ne '1969-12-31T23:59:59.999Z' -or
    $value.fields.same -ne 2 -or $value.fields.flag -isnot [bool] -or
    $value.fields.nan -ne $null -or $value.fields.unicode -ne 'Привет') {
  throw 'JSON schema/type validation failed'
}
Write-Output 'PASS independent JSON parser'

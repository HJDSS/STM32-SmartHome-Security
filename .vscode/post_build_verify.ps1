param(
    [Parameter(Mandatory = $true)]
    [string]$OutputDir,

    [Parameter(Mandatory = $false)]
    [string]$MapPath = "",

    [Parameter(Mandatory = $false)]
    [int]$MaxBinSizeKB = 512
)

$ErrorActionPreference = "Stop"

function Write-Info([string]$msg) {
    Write-Host "[VERIFY] $msg"
}

if (-not (Test-Path $OutputDir)) {
    throw "Output directory not found: $OutputDir"
}

$allFiles = Get-ChildItem -LiteralPath $OutputDir -File -ErrorAction SilentlyContinue
$binFile = $allFiles | Where-Object { $_.Extension -ieq ".bin" } | Select-Object -First 1
$hexFile = $allFiles | Where-Object { $_.Extension -ieq ".hex" } | Select-Object -First 1
$axfFile = $allFiles | Where-Object { $_.Extension -ieq ".axf" -or $_.Extension -ieq ".elf" } | Select-Object -First 1

if ($null -eq $binFile -and $null -eq $hexFile -and $null -eq $axfFile) {
    Write-Info ("OutputDir scanned files: {0}" -f ($allFiles.Count))
    throw "No binary artifacts found in $OutputDir (need at least one of .bin/.hex/.axf)."
}

Write-Info "Binary artifacts:"
if ($axfFile) { Write-Host ("  AXF: {0}" -f $axfFile.FullName) }
if ($hexFile) { Write-Host ("  HEX: {0}" -f $hexFile.FullName) }
if ($binFile) { Write-Host ("  BIN: {0}" -f $binFile.FullName) }

if ($binFile) {
    $binKB = [math]::Round($binFile.Length / 1KB, 2)
    Write-Info ("BIN size: {0} KB (limit {1} KB)" -f $binKB, $MaxBinSizeKB)
    if ($binFile.Length -gt ($MaxBinSizeKB * 1KB)) {
        throw ("BIN too large: {0} KB > {1} KB" -f $binKB, $MaxBinSizeKB)
    }
}

$filesForHash = @()
if ($axfFile) { $filesForHash += $axfFile.FullName }
if ($hexFile) { $filesForHash += $hexFile.FullName }
if ($binFile) { $filesForHash += $binFile.FullName }

Write-Info "SHA256:"
foreach ($f in $filesForHash) {
    $h = Get-FileHash -Algorithm SHA256 -Path $f
    Write-Host ("  {0}  {1}" -f $h.Hash, (Split-Path -Path $f -Leaf))
}

if ([string]::IsNullOrWhiteSpace($MapPath) -eq $false -and (Test-Path $MapPath)) {
    Write-Info "Map quick summary:"
    $mapLines = Get-Content -Path $MapPath -ErrorAction SilentlyContinue
    $keys = @("Program Size:", "Code (inc. data)", "RO-data", "RW-data", "ZI-data")
    foreach ($k in $keys) {
        $line = $mapLines | Where-Object { $_ -match [regex]::Escape($k) } | Select-Object -First 1
        if ($line) { Write-Host ("  {0}" -f $line.Trim()) }
    }
} elseif ([string]::IsNullOrWhiteSpace($MapPath) -eq $false) {
    Write-Info "Map file not found, skip: $MapPath"
}

Write-Info "Verification passed."

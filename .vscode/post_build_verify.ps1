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

function Resolve-FirstFile([string]$dir, [string[]]$patterns) {
    foreach ($pattern in $patterns) {
        $f = Get-ChildItem -Path $dir -Filter $pattern -File -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($null -ne $f) { return $f }
    }
    return $null
}

if (-not (Test-Path $OutputDir)) {
    throw "Output directory not found: $OutputDir"
}

$binFile = Resolve-FirstFile -dir $OutputDir -patterns @("*.bin")
$hexFile = Resolve-FirstFile -dir $OutputDir -patterns @("*.hex")
$axfFile = Resolve-FirstFile -dir $OutputDir -patterns @("*.axf", "*.elf")

if ($null -eq $binFile -and $null -eq $hexFile -and $null -eq $axfFile) {
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

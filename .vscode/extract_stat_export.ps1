param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,

    [Parameter(Mandatory = $false)]
    [string]$CsvPath = "",

    [Parameter(Mandatory = $false)]
    [string]$JsonlPath = ""
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $InputPath)) {
    throw "Input not found: $InputPath"
}

if ([string]::IsNullOrWhiteSpace($CsvPath)) {
    $CsvPath = [System.IO.Path]::ChangeExtension($InputPath, ".stat_export.csv")
}
if ([string]::IsNullOrWhiteSpace($JsonlPath)) {
    $JsonlPath = [System.IO.Path]::ChangeExtension($InputPath, ".stat_export.jsonl")
}

$rows = @()
Get-Content -LiteralPath $InputPath | ForEach-Object {
    $line = $_.Trim()
    if (-not $line.StartsWith("{")) { return }
    if ($line -notmatch '"tag"\s*:\s*"STAT_EXPORT"') { return }
    try {
        $obj = $line | ConvertFrom-Json
        $rows += [pscustomobject]@{
            tick        = $obj.tick
            false_alarm = $obj.false_alarm
            reconnect   = $obj.reconnect
            replay      = $obj.replay
            sd_fail     = $obj.sd_fail
            run_ex_72h  = $obj.run_ex_72h
        }
    } catch {
        # ignore malformed lines
    }
}

if ($rows.Count -eq 0) {
    throw "No STAT_EXPORT lines found in $InputPath"
}

$rows | Export-Csv -Path $CsvPath -NoTypeInformation -Encoding UTF8
$rows | ForEach-Object { $_ | ConvertTo-Json -Compress } | Set-Content -Path $JsonlPath -Encoding UTF8

Write-Host ("[STAT_EXPORT] rows={0}" -f $rows.Count)
Write-Host ("[STAT_EXPORT] csv={0}" -f $CsvPath)
Write-Host ("[STAT_EXPORT] jsonl={0}" -f $JsonlPath)


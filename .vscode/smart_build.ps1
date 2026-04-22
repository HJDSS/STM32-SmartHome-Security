param(
  [string]$WorkspaceRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path,
  [string]$Uv4Path = "D:\Keil_v5\UV4\UV4.exe",
  [string]$TargetName = "Target 1",
  [int]$MaxBinSizeKB = 512
)

$ErrorActionPreference = "Stop"

$biyeDir = Join-Path $WorkspaceRoot "biye"
$outDir  = Join-Path $biyeDir "代码\Output"
$logPath = Join-Path $outDir "build.log"
$verify  = Join-Path $WorkspaceRoot ".vscode\post_build_verify.ps1"

if (-not (Test-Path -LiteralPath $Uv4Path)) { throw "UV4.exe not found: $Uv4Path" }
if (-not (Test-Path -LiteralPath $biyeDir)) { throw "biye dir not found: $biyeDir" }

$proj = Get-ChildItem -LiteralPath $biyeDir -Recurse -Filter "*.uvprojx" -File | Select-Object -First 1
if (-not $proj) { throw "No .uvprojx found under $biyeDir" }

# Map file sits under <projDir>\Listings\*.map (Keil default)
$projParent = Split-Path -Parent $proj.FullName
$mapGuess = Get-ChildItem -LiteralPath (Join-Path $projParent "Listings") -Filter "*.map" -File -ErrorAction SilentlyContinue | Select-Object -First 1
$mapPath = if ($mapGuess) { $mapGuess.FullName } else { Join-Path $projParent "Listings\程序.map" }

New-Item -ItemType Directory -Force -Path $outDir | Out-Null

& $Uv4Path -j0 -b $proj.FullName -t $TargetName -o $logPath
$ec = $LASTEXITCODE
if ($ec -ne 0) {
  if (Test-Path -LiteralPath $logPath) { Start-Process -FilePath $logPath | Out-Null }
  exit $ec
}

& powershell -NoProfile -ExecutionPolicy Bypass -File $verify -OutputDir $outDir -MapPath $mapPath -MaxBinSizeKB $MaxBinSizeKB
exit $LASTEXITCODE


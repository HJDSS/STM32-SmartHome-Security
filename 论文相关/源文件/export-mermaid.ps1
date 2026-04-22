$ErrorActionPreference = "Stop"
[Console]::InputEncoding = [System.Text.UTF8Encoding]::new($false)
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
chcp 65001 | Out-Null

Write-Host "== Mermaid export and thesis build ==" -ForegroundColor Cyan

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$mermaidDir = Join-Path $root "mermaid"
$imagesDir = Join-Path $root "images"
$toolsDir = Join-Path $root "mmd-tools"
$mmdc = Join-Path $toolsDir "node_modules\.bin\mmdc.cmd"
$cfg = Join-Path $mermaidDir "mermaid-config.json"
$pp = Join-Path $mermaidDir "puppeteer-config.json"
$compiledDirName = ([char]0x7F16) + ([char]0x8BD1) + ([char]0x4EA7) + ([char]0x7269)
$compiledDir = Join-Path (Split-Path -Parent $root) $compiledDirName
$texFile = Get-ChildItem -Path $root -Filter "*.tex" | Select-Object -First 1
if (-not $texFile) { throw "No .tex file found in source directory." }
$texName = $texFile.Name
$texBase = $texFile.BaseName
$pdfOut = Join-Path $root ($texBase + ".pdf")
$compiledPdf = Join-Path $compiledDir ($texBase + ".pdf")

if (-not (Test-Path $mmdc)) {
  throw "Mermaid CLI not found: $mmdc. Install dependencies in mmd-tools first."
}
if (-not (Test-Path $mermaidDir)) { throw "Mermaid directory not found: $mermaidDir" }
if (-not (Test-Path $imagesDir)) { New-Item -ItemType Directory -Path $imagesDir | Out-Null }

Write-Host "1) Export Mermaid diagrams..." -ForegroundColor Yellow
$mmdFiles = Get-ChildItem $mermaidDir -Filter "*.mmd"
if ($mmdFiles.Count -eq 0) { throw "No .mmd files found in mermaid directory." }

foreach ($f in $mmdFiles) {
  $out = Join-Path $imagesDir ($f.BaseName + ".png")
  Write-Host "  - $($f.Name) -> $(Split-Path -Leaf $out)"
  & $mmdc -i $f.FullName -o $out -c $cfg -p $pp -b white
}

Write-Host "2) Compile thesis..." -ForegroundColor Yellow
Push-Location $root
try {
  xelatex -interaction=nonstopmode $texName
  if ($LASTEXITCODE -ne 0) { throw "xelatex pass 1 failed: $LASTEXITCODE" }
  biber $texBase
  if ($LASTEXITCODE -ne 0) { throw "biber failed: $LASTEXITCODE" }
  xelatex -interaction=nonstopmode $texName
  if ($LASTEXITCODE -ne 0) { throw "xelatex pass 2 failed: $LASTEXITCODE" }
  xelatex -interaction=nonstopmode $texName
  if ($LASTEXITCODE -ne 0) { throw "xelatex pass 3 failed: $LASTEXITCODE" }
}
finally {
  Pop-Location
}

if (-not (Test-Path $compiledDir)) { New-Item -ItemType Directory -Path $compiledDir | Out-Null }
Copy-Item -Path $pdfOut -Destination $compiledPdf -Force

Write-Host "3) Done: synced to $compiledDir\$($texBase).pdf" -ForegroundColor Green
Write-Host "Script completed successfully." -ForegroundColor Green

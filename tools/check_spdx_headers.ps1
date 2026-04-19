# SPDX header check for tracked source/script files.

$ErrorActionPreference = 'Stop'
$patterns = @('*.c','*.h','*.hpp','*.cpp','*.ps1','*.sh','*.cmake')
$files = foreach ($p in $patterns) { git ls-files $p }
$files = $files | Sort-Object -Unique

$missing = @()
foreach ($f in $files) {
  $raw = Get-Content -LiteralPath $f -Raw
  if ($raw -notmatch 'SPDX-License-Identifier:\s*MIT') {
    $missing += $f
  }
}

if ($missing.Count -gt 0) {
  Write-Host 'Missing SPDX headers:' -ForegroundColor Red
  $missing | ForEach-Object { Write-Host "  $_" }
  exit 1
}

Write-Host 'All checked files contain SPDX-License-Identifier: MIT' -ForegroundColor Green

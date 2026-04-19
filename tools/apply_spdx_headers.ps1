# SPDX-License-Identifier: MIT
# Apply SPDX header to tracked source/script files when missing.

$ErrorActionPreference = 'Stop'

$patterns = @('*.c','*.h','*.hpp','*.cpp','*.ps1','*.sh','*.cmake')
$files = foreach ($p in $patterns) { git ls-files $p }
$files = $files | Sort-Object -Unique

foreach ($f in $files) {
  $raw = Get-Content -LiteralPath $f -Raw
  if ($raw -match 'SPDX-License-Identifier:\s*MIT') {
    continue
  }

  $header = if ($f -match '\.(c|h|hpp|cpp)$') {
    '// SPDX-License-Identifier: MIT'
  } else {
    '# SPDX-License-Identifier: MIT'
  }

  if ($raw.StartsWith('#!')) {
    $nl = $raw.IndexOf("`n")
    if ($nl -ge 0) {
      $first = $raw.Substring(0, $nl + 1)
      $rest = $raw.Substring($nl + 1)
      $newRaw = $first + $header + "`n" + $rest
    } else {
      $newRaw = $raw + "`n" + $header + "`n"
    }
  } else {
    $newRaw = $header + "`n" + $raw
  }

  Set-Content -LiteralPath $f -Value $newRaw -NoNewline
  Write-Host "Updated: $f"
}

Write-Host 'SPDX apply pass complete.'

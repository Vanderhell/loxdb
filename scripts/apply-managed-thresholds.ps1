param(
    [string]$RecommendationsJson,
    [string]$PresetsPath = "CMakePresets.json",
    [switch]$DryRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if ($RecommendationsJson -eq "") {
    throw "RecommendationsJson is required."
}
if (-not (Test-Path $RecommendationsJson)) {
    throw "Recommendations file not found: $RecommendationsJson"
}
if (-not (Test-Path $PresetsPath)) {
    throw "Presets file not found: $PresetsPath"
}

$rec = Get-Content $RecommendationsJson -Raw | ConvertFrom-Json
$presets = Get-Content $PresetsPath -Raw | ConvertFrom-Json

if ($null -eq $presets.configurePresets) {
    throw "Invalid presets file: missing configurePresets."
}
if ($null -eq $rec.recommendations -or $rec.recommendations.Count -eq 0) {
    throw "Invalid recommendations file: missing recommendations entries."
}

function Get-RecBudget {
    param(
        [string]$OsName,
        [string]$Lane
    )
    $entry = $rec.recommendations | Where-Object { $_.os -eq $OsName -and $_.lane -eq $Lane } | Select-Object -First 1
    if ($null -eq $entry) {
        return $null
    }
    return [int][math]::Ceiling([double]$entry.recommended_budget_ms)
}

$map = @(
    @{ preset = "ci-debug-linux"; os = "linux-debug" },
    @{ preset = "ci-debug-windows"; os = "windows-debug" },
    @{ preset = "release-linux"; os = "linux-debug" },
    @{ preset = "release-windows"; os = "windows-debug" }
)

$changes = @()

foreach ($m in $map) {
    $preset = $presets.configurePresets | Where-Object { $_.name -eq $m.preset } | Select-Object -First 1
    if ($null -eq $preset) {
        continue
    }
    if ($null -eq $preset.cacheVariables) {
        $preset | Add-Member -NotePropertyName cacheVariables -NotePropertyValue (@{})
    }

    $smokeRec = Get-RecBudget -OsName $m.os -Lane "smoke"
    $longRec = Get-RecBudget -OsName $m.os -Lane "long"
    if ($null -eq $smokeRec -or $null -eq $longRec) {
        continue
    }

    $oldSmoke = [string]$preset.cacheVariables.MICRODB_MANAGED_STRESS_SMOKE_MAX_MS
    $oldLong = [string]$preset.cacheVariables.MICRODB_MANAGED_STRESS_LONG_MAX_MS
    $newSmoke = [string]$smokeRec
    $newLong = [string]$longRec

    if ($oldSmoke -ne $newSmoke) {
        $preset.cacheVariables.MICRODB_MANAGED_STRESS_SMOKE_MAX_MS = $newSmoke
    }
    if ($oldLong -ne $newLong) {
        $preset.cacheVariables.MICRODB_MANAGED_STRESS_LONG_MAX_MS = $newLong
    }

    $changes += [PSCustomObject]@{
        preset = $m.preset
        smoke_old = $oldSmoke
        smoke_new = $newSmoke
        long_old = $oldLong
        long_new = $newLong
    }
}

if ($changes.Count -eq 0) {
    Write-Output "No matching preset updates were applied."
    exit 0
}

Write-Output "Managed threshold updates:"
foreach ($c in $changes) {
    Write-Output ("- {0}: smoke {1} -> {2}, long {3} -> {4}" -f $c.preset, $c.smoke_old, $c.smoke_new, $c.long_old, $c.long_new)
}

if ($DryRun) {
    Write-Output "DryRun enabled: no file changes written."
    exit 0
}

$jsonOut = $presets | ConvertTo-Json -Depth 20
Set-Content -Path $PresetsPath -Value $jsonOut -Encoding ascii
Write-Output "Updated presets file: $PresetsPath"

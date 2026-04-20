param(
    [string]$ResultsDir = "docs/results",
    [int]$TopRuns = 20,
    [string]$OutputMarkdown = "docs/results/trend_dashboard.md"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (-not (Test-Path $ResultsDir)) {
    throw "Results directory not found: $ResultsDir"
}
if ($TopRuns -lt 1) {
    throw "TopRuns must be >= 1."
}

function Get-TimestampFromName {
    param([string]$Name)
    $m = [regex]::Match($Name, "(\d{8}_\d{6})")
    if (-not $m.Success) { return $null }
    try {
        return [DateTime]::ParseExact($m.Groups[1].Value, "yyyyMMdd_HHmmss", [System.Globalization.CultureInfo]::InvariantCulture)
    } catch {
        return $null
    }
}

function Get-SoakRows {
    param([string]$Dir)
    $rows = @()
    $files = Get-ChildItem -Path $Dir -File -Filter "soak_*.csv" -ErrorAction SilentlyContinue
    foreach ($f in $files) {
        $lines = Get-Content $f.FullName
        if ($null -eq $lines -or $lines.Count -eq 0) { continue }
        $header = "profile,ops,max_kv_put_us,max_kv_del_us,max_ts_insert_us,max_rel_insert_us,max_rel_del_us,max_compact_us,max_reopen_us,spikes_gt_1ms,spikes_gt_5ms,fail_count,slo_pass"
        $dataLine = $null
        foreach ($ln in $lines) {
            if ($ln -match '^(deterministic|balanced|stress),') {
                $dataLine = $ln
            }
        }
        if ([string]::IsNullOrWhiteSpace($dataLine)) { continue }
        $csvRows = @(@($header, $dataLine) | ConvertFrom-Csv)
        if ($null -eq $csvRows -or @($csvRows).Count -eq 0) { continue }
        $ts = Get-TimestampFromName -Name $f.Name
        foreach ($r in $csvRows) {
            $rows += [PSCustomObject]@{
                file = $f.Name
                ts = $ts
                profile = [string]$r.profile
                ops = [double]$r.ops
                max_kv_put_us = [double]$r.max_kv_put_us
                max_ts_insert_us = [double]$r.max_ts_insert_us
                max_rel_insert_us = [double]$r.max_rel_insert_us
                max_compact_us = [double]$r.max_compact_us
                max_reopen_us = [double]$r.max_reopen_us
                spikes_gt_5ms = [double]$r.spikes_gt_5ms
                slo_pass = [string]$r.slo_pass
            }
        }
    }
    return @($rows)
}

function Get-WorstcaseRows {
    param([string]$Dir)
    $rows = @()
    $files = Get-ChildItem -Path $Dir -File -Filter "worstcase_matrix_*.csv" -ErrorAction SilentlyContinue
    foreach ($f in $files) {
        $csvRows = @(Import-Csv $f.FullName)
        if ($null -eq $csvRows -or @($csvRows).Count -eq 0) { continue }
        $ts = Get-TimestampFromName -Name $f.Name
        foreach ($r in $csvRows) {
            $rows += [PSCustomObject]@{
                file = $f.Name
                ts = $ts
                profile = [string]$r.profile
                phase = [string]$r.phase
                max_kv_put_us = [double]$r.max_kv_put_us
                max_ts_insert_us = [double]$r.max_ts_insert_us
                max_rel_insert_us = [double]$r.max_rel_insert_us
                max_txn_commit_us = [double]$r.max_txn_commit_us
                max_compact_us = [double]$r.max_compact_us
                max_reopen_us = [double]$r.max_reopen_us
                spikes_gt_5ms = [double]$r.spikes_gt_5ms
                fail_count = [double]$r.fail_count
                slo_pass = [string]$r.slo_pass
            }
        }
    }
    return @($rows)
}

function Get-LatestValidationSummaries {
    param([string]$Dir, [int]$TakeN)
    $files = Get-ChildItem -Path $Dir -File -Filter "validation_summary_*.md" -ErrorAction SilentlyContinue
    return @($files | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First $TakeN)
}

$soakRows = Get-SoakRows -Dir $ResultsDir
$worstRows = Get-WorstcaseRows -Dir $ResultsDir
$latestSummaries = Get-LatestValidationSummaries -Dir $ResultsDir -TakeN $TopRuns

$md = @()
$md += "# Results Trend Dashboard"
$md += ""
$md += "- Generated UTC: $((Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ"))"
$md += "- Source directory: $ResultsDir"
$md += "- Window size: latest $TopRuns runs/files"
$md += ""

$md += "## Soak Trend"
if ($soakRows.Count -eq 0) {
    $md += "No soak CSV files found."
} else {
    $latestSoak = @(
        $soakRows |
            Sort-Object @{ Expression = { if ($_.ts -eq $null) { [DateTime]::MinValue } else { $_.ts } } } -Descending |
            Select-Object -First $TopRuns
    )
    $byProfile = $latestSoak | Group-Object profile
    $md += "| Profile | Samples | SLO Pass Rate | Max KV Put (us) | Max TS Insert (us) | Max REL Insert (us) | Max Compact (us) | Max Reopen (us) | Max Spikes >5ms |"
    $md += "|---|---:|---:|---:|---:|---:|---:|---:|---:|"
    foreach ($g in ($byProfile | Sort-Object Name)) {
        $rows = @($g.Group)
        $pass = @($rows | Where-Object { $_.slo_pass -eq "1" }).Count
        $rate = [math]::Round((100.0 * $pass) / $rows.Count, 2)
        $md += "| $($g.Name) | $($rows.Count) | $rate% | $([math]::Round(($rows | Measure-Object max_kv_put_us -Maximum).Maximum, 2)) | $([math]::Round(($rows | Measure-Object max_ts_insert_us -Maximum).Maximum, 2)) | $([math]::Round(($rows | Measure-Object max_rel_insert_us -Maximum).Maximum, 2)) | $([math]::Round(($rows | Measure-Object max_compact_us -Maximum).Maximum, 2)) | $([math]::Round(($rows | Measure-Object max_reopen_us -Maximum).Maximum, 2)) | $([math]::Round(($rows | Measure-Object spikes_gt_5ms -Maximum).Maximum, 2)) |"
    }
}
$md += ""

$md += "## Worstcase Matrix Trend"
if ($worstRows.Count -eq 0) {
    $md += "No worstcase matrix CSV files found."
} else {
    $latestWorst = @(
        $worstRows |
            Sort-Object @{ Expression = { if ($_.ts -eq $null) { [DateTime]::MinValue } else { $_.ts } } } -Descending |
            Select-Object -First ($TopRuns * 2)
    )
    $byProfilePhase = $latestWorst | Group-Object profile, phase
    $md += "| Profile | Phase | Samples | SLO Pass Rate | Max KV Put (us) | Max TS Insert (us) | Max REL Insert (us) | Max TXN Commit (us) | Max Compact (us) | Max Reopen (us) | Max Spikes >5ms |"
    $md += "|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|"
    foreach ($g in ($byProfilePhase | Sort-Object Name)) {
        $rows = @($g.Group)
        $pass = @($rows | Where-Object { $_.slo_pass -eq "1" }).Count
        $rate = [math]::Round((100.0 * $pass) / $rows.Count, 2)
        $md += "| $($rows[0].profile) | $($rows[0].phase) | $($rows.Count) | $rate% | $([math]::Round(($rows | Measure-Object max_kv_put_us -Maximum).Maximum, 2)) | $([math]::Round(($rows | Measure-Object max_ts_insert_us -Maximum).Maximum, 2)) | $([math]::Round(($rows | Measure-Object max_rel_insert_us -Maximum).Maximum, 2)) | $([math]::Round(($rows | Measure-Object max_txn_commit_us -Maximum).Maximum, 2)) | $([math]::Round(($rows | Measure-Object max_compact_us -Maximum).Maximum, 2)) | $([math]::Round(($rows | Measure-Object max_reopen_us -Maximum).Maximum, 2)) | $([math]::Round(($rows | Measure-Object spikes_gt_5ms -Maximum).Maximum, 2)) |"
    }
}
$md += ""

$md += "## Latest Validation Summaries"
if ($latestSummaries.Count -eq 0) {
    $md += "No validation summaries found."
} else {
    foreach ($f in $latestSummaries) {
        $md += "- $($f.Name) (updated UTC: $($f.LastWriteTimeUtc.ToString("yyyy-MM-ddTHH:mm:ssZ")))"
    }
}

$text = $md -join [Environment]::NewLine
Set-Content -Path $OutputMarkdown -Value $text -Encoding ascii
Write-Output "Generated trend dashboard: $OutputMarkdown"

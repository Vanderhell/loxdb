# Results Trend Dashboard

- Generated UTC: 2026-04-29T20:37:02Z
- Source directory: docs/results
- Window size: latest 20 runs/files

## Soak Trend
| Profile | Samples | SLO Pass Rate | Max KV Put (us) | Max TS Insert (us) | Max REL Insert (us) | Max Compact (us) | Max Reopen (us) | Max Spikes >5ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| balanced | 7 | 57.14% | 87226 | 84286 | 90276 | 86343 | 68667 | 1896 |
| deterministic | 7 | 57.14% | 81642 | 90309 | 90589 | 72429 | 58598 | 951 |
| stress | 6 | 100% | 91778 | 95051 | 75104 | 37801 | 80781 | 2032 |

## Worstcase Matrix Trend
| Profile | Phase | Samples | SLO Pass Rate | Max KV Put (us) | Max TS Insert (us) | Max REL Insert (us) | Max TXN Commit (us) | Max Compact (us) | Max Reopen (us) | Max Spikes >5ms |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| balanced | aged | 5 | 100% | 0 | 0 | 0 | 0 | 171104 | 0 | 0 |
| balanced | fresh | 5 | 100% | 21719 | 39668 | 41280 | 0 | 43490 | 0 | 13 |
| deterministic | aged | 10 | 50% | 0 | 0 | 0 | 0 | 132077 | 0 | 0 |
| deterministic | fresh | 10 | 50% | 24897 | 38861 | 42229 | 0 | 35139 | 0 | 24 |
| stress | aged | 5 | 100% | 0 | 0 | 0 | 0 | 246000 | 0 | 0 |
| stress | fresh | 5 | 100% | 34047 | 41692 | 47424 | 0 | 45769 | 0 | 17 |

## Latest Validation Summaries
- validation_summary_20260429_223616.md (updated UTC: 2026-04-29T20:37:00Z)
- validation_summary_20260429_222715.md (updated UTC: 2026-04-29T20:35:11Z)
- validation_summary_20260429_221046.md (updated UTC: 2026-04-29T20:18:40Z)
- validation_summary_20260429_210516.md (updated UTC: 2026-04-29T19:05:16Z)
- validation_summary_20260428_212054.md (updated UTC: 2026-04-28T19:28:44Z)
- validation_summary_20260419_193234.md (updated UTC: 2026-04-23T14:47:35Z)
- validation_summary_20260419_173440.md (updated UTC: 2026-04-23T14:47:35Z)
- validation_summary_20260412_203442.md (updated UTC: 2026-04-23T14:47:35Z)
- validation_summary_20260412_200336.md (updated UTC: 2026-04-23T14:47:35Z)
- validation_summary_20260412_182103.md (updated UTC: 2026-04-23T14:47:35Z)
- validation_summary_20260412_181940.md (updated UTC: 2026-04-23T14:47:35Z)
- validation_summary_20260412_172006.md (updated UTC: 2026-04-23T14:47:35Z)
- validation_summary_20260419_180500.md (updated UTC: 2026-04-19T16:30:45Z)
- validation_summary_20260419_152258.md (updated UTC: 2026-04-19T13:49:51Z)
- validation_summary_20260419_143541.md (updated UTC: 2026-04-19T13:02:35Z)
- validation_summary_20260412_214613.md (updated UTC: 2026-04-12T20:08:00Z)

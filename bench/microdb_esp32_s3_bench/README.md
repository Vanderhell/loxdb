# microdb Bench: ESP32-S3 N16R8

Tento priecinok obsahuje terminalovy benchmark runner pre `ESP32-S3 N16R8`:

- subor: `microdb_esp32_s3_bench.ino`
- ciel: overit funkcnost hlavnych API a zmerat latency/throughput metriky
- rezim: **terminalovy (manual trigger)**, testy sa nespustia automaticky po boote

## Co benchmark pokryva

Terminal app vie spustat:

1. `KV`: `microdb_kv_put`, `microdb_kv_get`, `microdb_kv_del`
2. `TS`: `microdb_ts_register`, `microdb_ts_insert`, `microdb_ts_query_buf`
3. `REL`: schema create + `microdb_rel_insert` + `microdb_rel_find`
4. `WAL`: zaplnenie WAL, `microdb_compact`, kontrola `microdb_inspect`
5. `Migration`: bump `schema_version` a kontrola `on_migrate` callbacku
6. `TXN`: `microdb_txn_begin`, `microdb_txn_commit`, `microdb_txn_rollback`

Kazdy merany krok vypisuje:

- `total=<x.xxx> ms`
- `avg=<x.xxx> us/op`
- `min/p50/p95/max` latency (`us`)
- `ops/s` throughput
- `MB/s` datovy throughput (kde ma zmysel)
- `ops=<N>`
- `heap_d=<delta>` (zmena free 8-bit heap)

## Poznamky k backendu

- Sketch pouziva **in-RAM storage HAL** (flash-like API nad RAM bufferom).
- Vyhoda: benchmark je reprodukovatelny bez pripravy custom flash partition.
- Stale sa testuju WAL cesty, compact, reopen/migration flow a transakcie.
- Na ESP32 sa buffer alokuje preferencne v **PSRAM** (`MALLOC_CAP_SPIRAM`),
  fallback je interna RAM.

Ak chces benchmarkovat realnu flash latenciu, v dalsom kroku sa da backend
vymenit za `port/esp32/microdb_port_esp32.*` s partition labelom.

## Integracia do Arduino projektu

V tomto priecinku su uz pripravene build helpery:

- `microdb.h` (lokalna kopia public headera)
- `src/*.c` + `src/*.h` (lokalne kopie microdb zdrojakov)

To znamena, ze Arduino IDE build funguje bez zavislosti na `../../...`
relativnych include cestach.

Poznamka: ak zmenis jadro `include/` alebo `src/` v repozitari, zosynchronizuj
aj lokalne kopie v `bench/microdb_esp32_s3_bench/`.

Pri PlatformIO/IDF mozes alternativne pridat repo ako komponent a benchmark
prepisat do `main.cpp`.

## Profily benchmarku

Sketch ma runtime profily:

- `quick` - rychly sanity run
- `deterministic` - odporucany profil pre stabilne latency + robustnost
- `balanced` - predvoleny profil (rozumny pomer cas/hlbka)
- `stress` - vacsi workload pre regresie a limity

Profil meni:

- `ram_kb` + `kv/ts/rel` split
- `kv_ops`, `ts_ops`, `rel_rows`
- WAL workload (`wal_ops`, `wal_key_span`, `wal_val_bytes`)
- `wal_compact_threshold_pct`

## Golden profily

Aktualne odporucane nastavenia:

- `deterministic`:
  - `kv=192`, `ts=384`, `rel=240`, `wal_ops=700`, `wal_key=140`, `wal_val=24`
  - `pace_every=1`, `pace_us=12`, `flush_every=0`
  - pouzitie: `run_det` (automaticky zapne deterministic + paced OFF + fresh DB)
- `balanced`:
  - `kv=320`, `ts=640`, `rel=500`, `wal_ops=1200`, `wal_key=200`, `wal_val=32`
  - pouzitie: porovnanie throughput medzi buildmi
- `stress`:
  - `ram=320`, `kv=900`, `ts=2400`, `rel=1200`, `wal_ops=3200`, `wal_key=320`, `wal_val=64`
  - poznamka: vyzaduje vacsi storage budget; ak open zlyha (`-6`), zniz workload alebo zvys storage

## Format vystupu

Ocakavany serial vystup:

```text
microdb ESP32-S3 terminal bench is ready.
Tests do NOT run automatically at power-on.
microdb-bench> run
=== microdb ESP32-S3 benchmark start (profile=balanced) ===
[BENCH] kv_put           total=... ms avg=... us p50=... p95=... min=... max=... max_op~... xmax/p50=... spk>1ms=...@... spk>5ms=...@... ops/s=... MB/s=... ops=... heap_d=...
[SLO] kv_put           OK/WARN (...)
[PHASE] kv_put           cold_ops=... cold_avg=... steady_ops=... steady_avg=...
...
=== microdb ESP32-S3 benchmark end ===
microdb-bench>
```

## Interpretacia metrik

- `max_op~N`:
  - priblizny index operacie, kde nastal `max` latency sample
- `spk>1ms=a@b`:
  - `a` = pocet sample > 1ms, `b` = prvy index sample > 1ms
- `spk>5ms=a@b`:
  - najdolezitejsia metrika pre deterministic spravanie
- `xmax/p50`:
  - pomer extremu k beznemu medianu; cim nizsie, tym stabilnejsie
- `[PHASE] cold/steady`:
  - oddeli startup spravanie od steady-state
- `[SLO]`:
  - profile-aware kontrola tail limitov (`OK` / `WARN`)

## Terminal prikazy

- `help` - vypise prikazy
- `run` - spusti cely benchmark suite (s fresh DB)
- `kv` / `ts` / `rel` / `wal` / `reopenchk` / `migrate` / `txn` - spusti len dany test
- `stats` - vypise `microdb_inspect()` snapshot
- `metrics` - vypise posledne namerane benchmark metriky
- `config` - vypise aktivnu konfiguraciu benchmarku
- `profiles` - vypise dostupne profily
- `profile` - vypise aktivny profil
- `profile <name>` - prepne profil + reopen DB s wipe
- `run_det` - deterministic profil + paced OFF + fresh DB + full run (odporucane)
- `run_det_paced` - deterministic profil + paced ON + full run (experimental)
- `paced` - zobrazi stav paced mode
- `paced on|off` - prepne paced mode
- `resetdb` - wipe storage + reopen DB
- `reopen` - reopen DB bez wipe

Poznamka: `run_det` validuje deterministic profil a jeho latency charakteristiku; nevypoveda automaticky o spravani ostatnych profilov ani inych workloadov.

## Odporucany workflow

1. `run_det`
2. `metrics`
3. Skontroluj hlavne:
   - `spk>5ms` na `kv_del`, `ts_insert`, `rel_insert`, `wal_kv_put`
   - `[SLO]` riadky (`OK`/`WARN`)
4. Pre throughput porovnanie spusti `profile balanced` + `run`

## Benchmark zapis (template)

Pouzi tuto tabulku pre logovanie behov:

| Date | FW commit | Board | CPU MHz | kv_put ms/op | kv_get ms/op | kv_del ms/op | ts_insert ms/op | rel_insert ms/op | compact total ms | Notes |
|---|---|---|---:|---:|---:|---:|---:|---:|---:|---|
| YYYY-MM-DD | `<sha>` | ESP32-S3 N16R8 | 240 |  |  |  |  |  |  |  |

## Odporucenia pre konzistentny bench

- fixni CPU freq (napr. 240 MHz)
- nechaj rovnaku `Serial` baudrate (`115200`)
- meraj po fresh boote
- vypni ine tasky, ktore periodicky zatazuju jadra
- porovnavaj aspon 3 behy a reportuj median

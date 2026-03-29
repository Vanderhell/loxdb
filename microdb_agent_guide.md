# microdb — Agent Implementation Guide

> **Tento dokument je doplnok k `microdb_spec.md`.**
> Obsahuje všetko čo agent potrebuje na implementáciu bez jedinej otázky:
> presné interné štruktúry, byte layout na storage, poradie krokov,
> magic numbers, CRC variant, build systém a commit stratégiu.

---

## Table of Contents

1. [Implementačné poradie — krok za krokom](#1-implementačné-poradie--krok-za-krokom)
2. [Presné interné štruktúry](#2-presné-interné-štruktúry)
3. [Arena alokátor — presný kód](#3-arena-alokátor--presný-kód)
4. [KV engine — interné detaily](#4-kv-engine--interné-detaily)
5. [TS engine — interné detaily](#5-ts-engine--interné-detaily)
6. [REL engine — interné detaily](#6-rel-engine--interné-detaily)
7. [WAL — presný byte layout](#7-wal--presný-byte-layout)
8. [Storage page layout](#8-storage-page-layout)
9. [CRC32 — presná implementácia](#9-crc32--presná-implementácia)
10. [Opaque handle sizes](#10-opaque-handle-sizes)
11. [Compile-time assertions](#11-compile-time-assertions)
12. [CMakeLists.txt — presná štruktúra](#12-cmakeliststxt--presná-štruktúra)
13. [Testovanie s microtest](#13-testovanie-s-microtest)
14. [Konvencie kódu](#14-konvencie-kódu)
15. [Commit a tag stratégia](#15-commit-a-tag-stratégia)
16. [Checklist pred každým commitom](#16-checklist-pred-každým-commitom)

---

## 1. Implementačné poradie — krok za krokom

Agent implementuje presne v tomto poradí. Každý krok musí kompilovať
a prejsť testami pred tým ako začne ďalší.

```
Krok 1:  microdb.h          — všetky typy, enums, forward decls, API signatúry
Krok 2:  microdb_arena.c/.h — interný bump alokátor (žiadna závislosť)
Krok 3:  microdb_crc.c/.h   — CRC32 (žiadna závislosť)
Krok 4:  microdb.c          — init/deinit/flush/stats (bez engine kódu)
Krok 5:  port/ram/          — RAM HAL (potrebný pre testy od kroku 6)
Krok 6:  microdb_kv.c/.h    — KV engine
         test_kv.c          — KV testy (všetky musia prejsť)
Krok 7:  microdb_ts.c/.h    — TS engine
         test_ts.c          — TS testy
Krok 8:  microdb_rel.c/.h   — REL engine
         test_rel.c         — REL testy
Krok 9:  microdb_wal.c/.h   — WAL (závisí na storage HAL)
         port/posix/        — POSIX HAL (pre WAL testy)
         test_wal.c         — WAL testy
Krok 10: test_integration.c — integračné testy
         test_limits.c      — limit testy
Krok 11: port/esp32/        — ESP32 HAL
Krok 12: examples/          — oba príklady
Krok 13: README.md          — finálna dokumentácia
```

**Pravidlo:** Nikdy neskáč dopredu. Ak krok 6 nefunguje, krok 7 sa nezačína.

---

## 2. Presné interné štruktúry

Toto sú **interné** štruktúry — patria do `src/microdb_*.h`, nie do `include/microdb.h`.

### 2.1 microdb_arena_t

```c
/* src/microdb_arena.h */
typedef struct {
    uint8_t *base;       /* Pointer do začiatku pridelenej oblasti */
    size_t   capacity;   /* Celková veľkosť oblasti v bajtoch */
    size_t   used;       /* Koľko bajtov je already pridelených */
} microdb_arena_t;
```

### 2.2 microdb_kv_bucket_t

```c
/* src/microdb_kv.h */
typedef enum {
    MDB_KV_EMPTY   = 0,   /* Nikdy nepoužitý bucket */
    MDB_KV_LIVE    = 1,   /* Živý záznam */
    MDB_KV_DELETED = 2,   /* Deleted (tombstone pre linear probing) */
} microdb_kv_bucket_state_t;

typedef struct {
    uint8_t  state;                          /* microdb_kv_bucket_state_t, 1 byte */
    uint8_t  _pad[3];                        /* padding na 4-byte align */
    char     key[MICRODB_KV_KEY_MAX_LEN];    /* Null-terminated key */
    uint32_t val_offset;                     /* Offset do value pool */
    uint32_t val_len;                        /* Dĺžka value v bajtoch */
    uint32_t expires_at;                     /* Unix timestamp, 0 = no expiry */
    uint32_t last_access;                    /* Monotónny counter pre LRU */
} microdb_kv_bucket_t;
```

### 2.3 microdb_kv_ctx_t

```c
/* src/microdb_kv.h */
typedef struct {
    microdb_kv_bucket_t *buckets;    /* Pole bucketov, veľkosť = bucket_count */
    uint8_t             *val_pool;   /* Packed blob pool pre hodnoty */
    uint32_t             bucket_count;  /* Počet bucketov, vždy power of 2 */
    uint32_t             live_count;    /* Počet živých záznamov */
    uint32_t             val_pool_size; /* Celková veľkosť val_pool v bajtoch */
    uint32_t             val_pool_used; /* Koľko val_pool je obsadené */
    uint32_t             access_clock; /* Monotónny counter, inkrement pri každom accesse */
} microdb_kv_ctx_t;
```

### 2.4 microdb_ts_stream_t

```c
/* src/microdb_ts.h */
typedef struct {
    char                 name[MICRODB_TS_STREAM_NAME_LEN];
    microdb_ts_type_t    type;
    size_t               raw_size;       /* Iba pre MICRODB_TS_RAW, inak 0 */
    uint32_t             head;           /* Index ďalšieho zápisu */
    uint32_t             tail;           /* Index najstaršieho sample */
    uint32_t             count;          /* Počet live samples */
    uint32_t             capacity;       /* Max samples v ring buffri */
    microdb_ts_sample_t *buf;            /* Ring buffer, alokované z TS arény */
    bool                 registered;     /* Bol stream zaregistrovaný? */
} microdb_ts_stream_t;
```

### 2.5 microdb_ts_ctx_t

```c
/* src/microdb_ts.h */
typedef struct {
    microdb_ts_stream_t streams[MICRODB_TS_MAX_STREAMS];
    uint32_t            stream_count;   /* Počet zaregistrovaných streamov */
} microdb_ts_ctx_t;
```

### 2.6 microdb_col_desc_t

```c
/* src/microdb_rel.h */
typedef struct {
    char                name[MICRODB_REL_COL_NAME_LEN];
    microdb_col_type_t  type;
    size_t              size;        /* Veľkosť stĺpca v bajtoch */
    size_t              offset;      /* Byte offset v row buffri (vypočítaný pri seal) */
    bool                is_index;    /* Je toto indexový stĺpec? */
} microdb_col_desc_t;
```

### 2.7 microdb_schema_s (interná definícia)

```c
/* src/microdb_rel.h — toto JE definícia opaque microdb_schema_t */
struct microdb_schema_s {
    char              name[MICRODB_REL_TABLE_NAME_LEN];
    microdb_col_desc_t cols[MICRODB_REL_MAX_COLS];
    uint32_t           col_count;
    uint32_t           max_rows;
    size_t             row_size;    /* Vypočítané pri seal, vrátane paddingu */
    uint32_t           index_col;  /* Index stĺpca, UINT32_MAX = žiadny */
    bool               sealed;
};
```

### 2.8 microdb_index_entry_t

```c
/* src/microdb_rel.h */
typedef struct {
    uint8_t  key_bytes[MICRODB_REL_INDEX_KEY_MAX];  /* Kópia hodnoty index stĺpca */
    uint32_t row_idx;                                /* Index do rows[] poľa */
} microdb_index_entry_t;

#define MICRODB_REL_INDEX_KEY_MAX  16   /* Max veľkosť index hodnoty. Override s #define. */
```

### 2.9 microdb_table_s (interná definícia)

```c
/* src/microdb_rel.h — toto JE definícia opaque microdb_table_t */
struct microdb_table_s {
    char                  name[MICRODB_REL_TABLE_NAME_LEN];
    microdb_col_desc_t    cols[MICRODB_REL_MAX_COLS];
    uint32_t              col_count;
    uint32_t              max_rows;
    size_t                row_size;
    uint32_t              index_col;       /* UINT32_MAX = žiadny */
    size_t                index_key_size;  /* sizeof index stĺpca */
    uint8_t              *rows;            /* Flat row storage, alokované z REL arény */
    uint8_t              *alive_bitmap;    /* 1 bit per row, alokované z REL arény */
    microdb_index_entry_t *index;          /* Sorted index array, alokované z REL arény */
    uint32_t              live_count;      /* Počet živých riadkov */
    bool                  registered;
};
```

### 2.10 microdb_rel_ctx_t

```c
/* src/microdb_rel.h */
typedef struct {
    microdb_table_t tables[MICRODB_REL_MAX_TABLES];
    uint32_t        table_count;
} microdb_rel_ctx_t;
```

### 2.11 microdb_s (hlavný handle — interná definícia)

```c
/* src/microdb.c — nie v public headeri */
struct microdb_s {
    uint8_t              *heap;          /* Jediný malloc, veľkosť = RAM_KB * 1024 */
    size_t                heap_size;

    microdb_arena_t       kv_arena;
    microdb_arena_t       ts_arena;
    microdb_arena_t       rel_arena;

    microdb_kv_ctx_t      kv;
    microdb_ts_ctx_t      ts;
    microdb_rel_ctx_t     rel;

    microdb_storage_t    *storage;       /* NULL = RAM only */
    microdb_timestamp_t (*now)(void);    /* NULL = timestamps always 0 */

    bool                  initialized;
};
```

---

## 3. Arena alokátor — presný kód

```c
/* src/microdb_arena.h */

static inline void microdb_arena_init(microdb_arena_t *a,
                                       uint8_t         *base,
                                       size_t           capacity) {
    a->base     = base;
    a->capacity = capacity;
    a->used     = 0;
}

/* Alokuje `size` bajtov zarovnaných na `align` bajtov.
 * align musí byť power of 2.
 * Vracia NULL ak nie je dostatok miesta. */
static inline void *microdb_arena_alloc(microdb_arena_t *a,
                                         size_t           size,
                                         size_t           align) {
    /* Zarovnaj used nahor */
    size_t aligned = (a->used + (align - 1u)) & ~(align - 1u);
    if (aligned + size > a->capacity) {
        return NULL;
    }
    void *ptr = a->base + aligned;
    a->used   = aligned + size;
    return ptr;
}

/* Koľko bajtov zostáva v aréne */
static inline size_t microdb_arena_remaining(const microdb_arena_t *a) {
    return a->capacity - a->used;
}
```

**Pravidlo:** `microdb_arena_alloc` sa volá VÝLUČNE z `microdb_init()`.
Nikdy inde. Po návrate z `microdb_init()` sa arény nikdy nemenia.

---

## 4. KV engine — interné detaily

### 4.1 Výpočet bucket_count

```c
/* V microdb_init(), pred alokáciou KV štruktúr: */
static uint32_t kv_bucket_count_for(uint32_t max_keys) {
    /* Cieľ: load factor ≤ 75% → bucket_count ≥ ceil(max_keys / 0.75) */
    uint32_t min_buckets = (max_keys * 4u + 2u) / 3u;   /* ceil(max_keys * 4/3) */
    /* Zaokrúhli nahor na najbližšiu power of 2 */
    uint32_t n = 1u;
    while (n < min_buckets) n <<= 1u;
    return n;
}
```

### 4.2 Hash funkcia

Použiť **FNV-1a 32-bit**. Žiadna iná. Dôvod: jednoduchá, dobrá distribúcia,
žiadna externá závislosť.

```c
static uint32_t kv_hash(const char *key) {
    uint32_t hash = 2166136261u;   /* FNV offset basis */
    while (*key) {
        hash ^= (uint8_t)*key++;
        hash *= 16777619u;         /* FNV prime */
    }
    return hash;
}

/* Bucket index: */
static uint32_t kv_bucket_idx(const microdb_kv_ctx_t *kv, const char *key) {
    return kv_hash(key) & (kv->bucket_count - 1u);   /* & funguje len pre power of 2 */
}
```

### 4.3 Linear probing — insert

```c
/* Vracia index bucketu kde key môže byť vložený, alebo UINT32_MAX ak plný. */
static uint32_t kv_probe_for_insert(const microdb_kv_ctx_t *kv, const char *key) {
    uint32_t idx  = kv_bucket_idx(kv, key);
    uint32_t first_deleted = UINT32_MAX;

    for (uint32_t i = 0; i < kv->bucket_count; i++) {
        uint32_t j = (idx + i) & (kv->bucket_count - 1u);
        microdb_kv_bucket_t *b = &kv->buckets[j];

        if (b->state == MDB_KV_EMPTY) {
            /* Prázdny bucket — môžeme použiť tento alebo prvý deleted */
            return (first_deleted != UINT32_MAX) ? first_deleted : j;
        }
        if (b->state == MDB_KV_DELETED && first_deleted == UINT32_MAX) {
            first_deleted = j;
            /* Pokračuj — môže existovať živý záznam s rovnakým kľúčom ďalej */
        }
        if (b->state == MDB_KV_LIVE && strcmp(b->key, key) == 0) {
            return j;   /* Overwrite existujúceho */
        }
    }
    return (first_deleted != UINT32_MAX) ? first_deleted : UINT32_MAX;
}
```

### 4.4 Linear probing — lookup

```c
/* Vracia index bucketu so živým kľúčom, alebo UINT32_MAX ak nenájdený. */
static uint32_t kv_probe_for_lookup(const microdb_kv_ctx_t *kv, const char *key) {
    uint32_t idx = kv_bucket_idx(kv, key);

    for (uint32_t i = 0; i < kv->bucket_count; i++) {
        uint32_t j = (idx + i) & (kv->bucket_count - 1u);
        microdb_kv_bucket_t *b = &kv->buckets[j];

        if (b->state == MDB_KV_EMPTY) return UINT32_MAX;   /* Stop — kľúč neexistuje */
        if (b->state == MDB_KV_LIVE && strcmp(b->key, key) == 0) return j;
        /* MDB_KV_DELETED → pokračuj (tombstone) */
    }
    return UINT32_MAX;
}
```

### 4.5 Value pool alokácia

```c
/* val_pool je lineárny packed buffer.
 * Každý záznam: [ uint32_t len | uint8_t data[len] | pad to 4-byte align ]
 *
 * val_offset v bucket_t ukazuje na začiatok `len` fieldu.
 *
 * Pri delete: bucket sa označí MDB_KV_DELETED, val_pool sa NEMAŽE.
 * Kompakcia val_pool sa spustí keď:
 *   val_pool_used > val_pool_size * 0.75  AND  live_count < bucket_count * 0.5
 *
 * Kompakcia:
 *   1. Alokuj tmp buffer rovnakej veľkosti ako val_pool (z kv_arena — pozor,
 *      arény sú read-only po init! Preto val_pool kompakcia NESMIE alokovať.
 *      Namiesto toho: in-place kompakcia s dvojitým pointerom.)
 *   2. Prechádzaj všetky LIVE buckety v poradí val_offset (sort by offset).
 *   3. Kopíruj hodnoty na začiatok val_pool, update val_offset v bucketoch.
 *   4. Reset val_pool_used.
 */
```

### 4.6 LRU eviction

```c
/* Spustí sa keď live_count == MICRODB_KV_MAX_KEYS a policy == OVERWRITE.
 * Nájde bucket s najmenším last_access a zavolá kv_delete_bucket(). */
static void kv_evict_lru(microdb_kv_ctx_t *kv) {
    uint32_t min_access = UINT32_MAX;
    uint32_t victim     = UINT32_MAX;

    for (uint32_t i = 0; i < kv->bucket_count; i++) {
        if (kv->buckets[i].state == MDB_KV_LIVE) {
            if (kv->buckets[i].last_access < min_access) {
                min_access = kv->buckets[i].last_access;
                victim     = i;
            }
        }
    }
    if (victim != UINT32_MAX) {
        kv->buckets[victim].state = MDB_KV_DELETED;
        kv->live_count--;
        /* val_pool priestor sa neuvoľňuje okamžite — kompakcia to vyrieši */
    }
}
```

---

## 5. TS engine — interné detaily

### 5.1 Výpočet capacity per stream

```c
/* V microdb_init(): */
static uint32_t ts_stream_capacity(size_t ts_arena_bytes, uint32_t stream_count) {
    /* Rovnomerné rozdelenie — každý stream dostane rovnako */
    size_t bytes_per_stream = ts_arena_bytes / stream_count;
    return (uint32_t)(bytes_per_stream / sizeof(microdb_ts_sample_t));
}
/* Minimálna kapacita: 4 samples per stream. Ak by kapacita vyšla < 4,
 * microdb_init() vracia MICRODB_ERR_NO_MEM. */
#define MICRODB_TS_MIN_CAPACITY_PER_STREAM  4
```

### 5.2 Ring buffer operácie

```c
/* Insert — head ukazuje na ďalší zápis */
static void ts_rb_insert(microdb_ts_stream_t *s, const microdb_ts_sample_t *sample) {
    s->buf[s->head] = *sample;
    s->head = (s->head + 1u) % s->capacity;

    if (s->count < s->capacity) {
        s->count++;
    } else {
        /* Buffer plný — tail sa posunie (oldest je prepísaný) */
        s->tail = (s->tail + 1u) % s->capacity;
    }
}

/* Iterácia od najstaršieho po najnovší */
static void ts_rb_iter(const microdb_ts_stream_t *s,
                        microdb_ts_query_cb_t      cb,
                        void                      *ctx,
                        MICRODB_TIMESTAMP_TYPE      from,
                        MICRODB_TIMESTAMP_TYPE      to) {
    for (uint32_t i = 0; i < s->count; i++) {
        uint32_t idx = (s->tail + i) % s->capacity;
        const microdb_ts_sample_t *sample = &s->buf[idx];
        if (sample->ts >= from && sample->ts <= to) {
            if (!cb(sample, ctx)) return;   /* Early stop */
        }
    }
}
```

### 5.3 DOWNSAMPLE policy

```c
/* Volá sa keď je buffer plný a MICRODB_TS_OVERFLOW_POLICY == DOWNSAMPLE */
static void ts_downsample_oldest(microdb_ts_stream_t *s) {
    /* Zlúč sample[tail] a sample[tail+1] do sample[tail] (priemer) */
    uint32_t i0 = s->tail;
    uint32_t i1 = (s->tail + 1u) % s->capacity;

    microdb_ts_sample_t *a = &s->buf[i0];
    microdb_ts_sample_t *b = &s->buf[i1];

    /* Priemer timestamp a hodnoty (len pre F32/I32/U32) */
    a->ts = a->ts / 2u + b->ts / 2u;   /* Zabráni overflow */

    switch (/* stream type */) {
        case MICRODB_TS_F32: a->v.f32 = (a->v.f32 + b->v.f32) * 0.5f; break;
        case MICRODB_TS_I32: a->v.i32 = (a->v.i32 / 2) + (b->v.i32 / 2); break;
        case MICRODB_TS_U32: a->v.u32 = (a->v.u32 / 2) + (b->v.u32 / 2); break;
        case MICRODB_TS_RAW:
            /* Pre RAW: ponechaj starší, zahoď novší */
            break;
    }

    /* Odstráň sample[i1] posunutím tail */
    s->tail  = (s->tail + 1u) % s->capacity;
    s->count--;
    /* Teraz je miesto pre nový sample — volajúci zavolá ts_rb_insert() */
}
```

---

## 6. REL engine — interné detaily

### 6.1 Výpočet row_size pri seal

```c
microdb_err_t microdb_schema_seal(microdb_schema_t *schema) {
    if (!schema || schema->col_count == 0) return MICRODB_ERR_INVALID;
    if (schema->sealed) return MICRODB_OK;   /* Idempotent */

    size_t offset = 0;
    for (uint32_t i = 0; i < schema->col_count; i++) {
        microdb_col_desc_t *col = &schema->cols[i];

        /* Zarovnaj na prirodzené zarovnanie stĺpca */
        size_t align = (col->size >= 8) ? 8u :
                       (col->size >= 4) ? 4u :
                       (col->size >= 2) ? 2u : 1u;
        offset = (offset + (align - 1u)) & ~(align - 1u);
        col->offset = offset;
        offset += col->size;
    }

    /* Celkový row_size zarovnaný na 4 bajty */
    schema->row_size = (offset + 3u) & ~3u;
    schema->sealed   = true;
    return MICRODB_OK;
}
```

### 6.2 Alive bitmap operácie

```c
/* alive_bitmap: 1 bit per row, index row_idx */
static bool rel_is_alive(const uint8_t *bitmap, uint32_t row_idx) {
    return (bitmap[row_idx >> 3u] >> (row_idx & 7u)) & 1u;
}

static void rel_set_alive(uint8_t *bitmap, uint32_t row_idx, bool alive) {
    if (alive) {
        bitmap[row_idx >> 3u] |=  (1u << (row_idx & 7u));
    } else {
        bitmap[row_idx >> 3u] &= ~(1u << (row_idx & 7u));
    }
}
```

### 6.3 Index insert (insertion sort)

```c
/* Vloží nový index entry na správnú pozíciu. O(n) — akceptovateľné pre n ≤ 1000. */
static void rel_index_insert(microdb_table_t       *t,
                              uint32_t               row_idx,
                              const void            *key_bytes) {
    /* Nájdi pozíciu (binárne hľadanie) */
    int32_t lo = 0, hi = (int32_t)t->live_count - 1, pos = (int32_t)t->live_count;
    while (lo <= hi) {
        int32_t mid = lo + (hi - lo) / 2;
        int cmp = memcmp(t->index[mid].key_bytes, key_bytes, t->index_key_size);
        if (cmp < 0)  { lo = mid + 1; }
        else          { pos = mid; hi = mid - 1; }
    }

    /* Posuň záznamy doprava */
    memmove(&t->index[pos + 1], &t->index[pos],
            (t->live_count - (size_t)pos) * sizeof(microdb_index_entry_t));

    /* Vlož */
    memcpy(t->index[pos].key_bytes, key_bytes, t->index_key_size);
    t->index[pos].row_idx = row_idx;
    t->live_count++;
}
```

### 6.4 Index find (binary search)

```c
/* Vracia index do t->index[] kde sa začínajú zhody, alebo UINT32_MAX. */
static uint32_t rel_index_find_first(const microdb_table_t *t, const void *key_bytes) {
    int32_t lo = 0, hi = (int32_t)t->live_count - 1, result = -1;
    while (lo <= hi) {
        int32_t mid = lo + (hi - lo) / 2;
        int cmp = memcmp(t->index[mid].key_bytes, key_bytes, t->index_key_size);
        if (cmp == 0) { result = mid; hi = mid - 1; }   /* Hľadaj ešte vľavo */
        else if (cmp < 0) lo = mid + 1;
        else              hi = mid - 1;
    }
    return (result >= 0) ? (uint32_t)result : UINT32_MAX;
}
```

---

## 7. WAL — presný byte layout

### 7.1 WAL header (prvých 32 bajtov WAL regiónu)

```
Offset  Size  Field
──────  ────  ─────────────────────────────────────────────────────
     0     4  magic         = 0x4D44424C  ("MDBL" v little-endian)
     4     4  version       = 0x00010000  (major=1, minor=0, patch=0)
     8     4  entry_count   Počet platných entries v tomto WAL bloku
    12     4  sequence      Monotónne číslo WAL bloku (inkrement pri každej kompakcii)
    16     4  crc32         CRC32 bajtov [0..15] tohto headeru
    20    12  _reserved     Nulové, rezervované pre budúce použitie
```

### 7.2 WAL entry

```
Offset  Size  Field
──────  ────  ─────────────────────────────────────────────────────
     0     4  magic         = 0x454E5452  ("ENTR" v little-endian)
     4     4  seq           Poradové číslo tejto entry (monotónne v rámci bloku)
     8     1  engine        0=KV  1=TS  2=REL
     9     1  op            0=SET/INSERT  1=DEL  2=CLEAR
    10     2  data_len      Počet bajtov v `data` poli
    12     4  crc32         CRC32 bajtov [0..11] + data[0..data_len-1]
    16  data  data          Serializovaný payload (pozri 7.3)
```

Každá entry je zarovnaná na 4 bajty (data_len sa zaokrúhli nahor).

### 7.3 WAL entry payload pre každú operáciu

**KV SET:**
```
[ uint8_t  key_len   ]   Dĺžka kľúča bez NUL
[ char     key[]     ]   Kľúč (key_len bajtov, bez NUL)
[ uint32_t val_len   ]   Dĺžka hodnoty
[ uint8_t  val[]     ]   Hodnota (val_len bajtov)
[ uint32_t expires_at]   TTL timestamp, 0 = no expiry
```

**KV DEL:**
```
[ uint8_t  key_len   ]
[ char     key[]     ]
```

**KV CLEAR:**
```
(žiadne dáta, data_len = 0)
```

**TS INSERT:**
```
[ uint8_t  name_len  ]
[ char     name[]    ]
[ uint32_t ts_low    ]   Spodných 32 bitov timestampu
[ uint32_t ts_high   ]   Vrchných 32 bitov (0 ak MICRODB_TIMESTAMP_TYPE=uint32_t)
[ uint8_t  val_type  ]   microdb_ts_type_t
[ uint8_t  val[]     ]   4 bajty pre F32/I32/U32, raw_size pre RAW
```

**REL INSERT:**
```
[ uint8_t  name_len  ]
[ char     name[]    ]   Meno tabuľky
[ uint32_t row_size  ]
[ uint8_t  row[]     ]   Flat row buffer (row_size bajtov)
```

**REL DELETE:**
```
[ uint8_t  name_len  ]
[ char     name[]    ]
[ uint8_t  key[]     ]   Hodnota index stĺpca (index_key_size bajtov)
```

### 7.4 WAL recovery algoritmus

```
microdb_init() s platným storage HAL:

1. Čítaj WAL header z storage offset 0.
2. Skontroluj magic == 0x4D44424C. Ak nie → WAL je prázdny alebo corrupt → preskočiť.
3. Vypočítaj CRC32 bajtov [0..15] headeru. Ak nesedí → WAL corrupt → reset WAL.
4. Načítaj entry_count.
5. Pre i = 0 .. entry_count-1:
   a. Čítaj entry header z offsetu 32 + i * (16 + data_len_zarovnaný).
      Pozor: data_len v každej entry určuje posun na ďalšiu.
   b. Skontroluj entry magic == 0x454E5452. Ak nie → STOP (čiastočný zápis).
   c. Vypočítaj CRC32 entry. Ak nesedí → STOP (čiastočný zápis).
   d. Aplikuj operáciu do RAM (KV/TS/REL).
6. Nastav WAL entry_count = 0, prepíš WAL header (nový CRC).
7. Načítaj hlavné stránky z storage do RAM (KV/TS/REL page regióny).
   Replay WAL ich mohol aktualizovať v RAM — flush ich teraz na storage.
```

---

## 8. Storage page layout

```
Storage rozdelenie (celkový offset od začiatku):

Offset 0:
  WAL región = 2 × erase_size
  (2 erase bloky vždy, bez ohľadu na RAM_KB)

Offset (2 × erase_size):
  KV región  = ceil(kv_arena_bytes / erase_size) × erase_size

Offset (2 × erase_size + KV_region_size):
  TS región  = ceil(ts_arena_bytes / erase_size) × erase_size

Offset (2 × erase_size + KV_region_size + TS_region_size):
  REL región = ceil(rel_arena_bytes / erase_size) × erase_size

Minimálna požadovaná storage kapacita:
  storage.capacity ≥ 2*erase_size + KV_region + TS_region + REL_region
  microdb_init() vracia MICRODB_ERR_STORAGE ak kapacita nestačí.
```

### 8.1 KV page formát

```
KV stránka je sekvenčný zápis všetkých LIVE bucketov:

[ uint32_t magic     ]  = 0x4B565047  ("KVPG")
[ uint32_t count     ]  Počet nasledujúcich entries
[ uint32_t crc32     ]  CRC32 všetkých nasledujúcich dát
Pre každý entry:
  [ uint8_t  key_len   ]
  [ char     key[]     ]
  [ uint32_t val_len   ]
  [ uint8_t  val[]     ]
  [ uint32_t expires_at]
```

### 8.2 TS page formát

```
[ uint32_t magic      ]  = 0x54535047  ("TSPG")
[ uint32_t stream_count]
[ uint32_t crc32      ]
Pre každý stream:
  [ uint8_t  name_len  ]
  [ char     name[]    ]
  [ uint8_t  type      ]
  [ uint32_t count     ]  Počet samples
  Pre každý sample (v chronologickom poradí):
    [ uint32_t ts_low  ]
    [ uint32_t ts_high ]
    [ uint8_t  val[]   ]  4 alebo raw_size bajtov
```

### 8.3 REL page formát

```
[ uint32_t magic       ]  = 0x524C5047  ("RLPG")
[ uint32_t table_count ]
[ uint32_t crc32       ]
Pre každú tabuľku:
  [ uint8_t  name_len  ]
  [ char     name[]    ]
  [ uint32_t row_count ]  Počet živých riadkov
  [ uint32_t row_size  ]
  Pre každý živý riadok:
    [ uint8_t row[]    ]  row_size bajtov
```

---

## 9. CRC32 — presná implementácia

Používať **CRC32/ISO-HDLC** (aka CRC32b, štandard v Ethernet/ZIP/PNG).
Polynóm: `0xEDB88320` (reflected).

```c
/* src/microdb_crc.h */
#ifndef MICRODB_CRC_H
#define MICRODB_CRC_H

#include <stdint.h>
#include <stddef.h>

/* Vypočíta CRC32 bloku dát.
 * Pre reťazené volania: microdb_crc32(prev_crc, data, len)
 * Prvé volanie: microdb_crc32(0xFFFFFFFFu, data, len) */
uint32_t microdb_crc32(uint32_t crc, const void *data, size_t len);

/* Konvenienčná makro pre celý buffer od nuly */
#define MICRODB_CRC32(data, len)  microdb_crc32(0xFFFFFFFFu, (data), (len))

#endif
```

```c
/* src/microdb_crc.c — tabuľková implementácia, 1KB RAM pre tabuľku */
#include "microdb_crc.h"

static const uint32_t crc_table[256] = {
    /* Generovaná štandardná CRC32 tabuľka pre polynóm 0xEDB88320 */
    /* Agent: vygeneruj tabuľku štandardným algoritmom, NEROB lookup online */
    0x00000000u, 0x77073096u, 0xEE0E612Cu, 0x990951BAu, /* ... */
    /* Všetkých 256 hodnôt */
};

uint32_t microdb_crc32(uint32_t crc, const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    crc = ~crc;
    while (len--) {
        crc = crc_table[(crc ^ *p++) & 0xFFu] ^ (crc >> 8u);
    }
    return ~crc;
}
```

**Poznámka pre agenta:** Celú CRC tabuľku vygeneruj algoritmicky pri compile time
pomocou `static const` poľa, nie `#include` externého súboru.
Generovací algoritmus:
```c
for (uint32_t i = 0; i < 256; i++) {
    uint32_t c = i;
    for (int j = 0; j < 8; j++) {
        c = (c & 1u) ? (0xEDB88320u ^ (c >> 1u)) : (c >> 1u);
    }
    crc_table[i] = c;
}
```

---

## 10. Opaque handle sizes

Tieto hodnoty patria do `include/microdb.h` aby caller mohol stack-alokovať.
Vypočítaj ich z veľkostí interných štruktúr + zaokrúhli nahor na 8 bajtov.

```c
/* include/microdb.h */

/* Opaque handle pre hlavnú databázu.
 * Caller alokuje na stacku alebo ako static:
 *   static microdb_t db;  */
typedef struct {
    uint8_t _opaque[MICRODB_HANDLE_SIZE];
} microdb_t;

/* Opaque schema handle.
 *   microdb_schema_t schema;  (stack-OK) */
typedef struct {
    uint8_t _opaque[MICRODB_SCHEMA_SIZE];
} microdb_schema_t;

/* Opaque table handle — NEALOKUJ na stacku.
 * Ukazuje dovnútra microdb_t interne.
 * Caller dostane pointer cez microdb_table_get(). */
typedef struct microdb_table_s microdb_table_t;
```

Veľkosti:

```c
/*
 * MICRODB_HANDLE_SIZE:
 *   sizeof(microdb_s) = sizeof(pointers + arenas + kv_ctx + ts_ctx + rel_ctx)
 *
 *   Orientačne pre default konfiguráciu:
 *   heap ptr:          8
 *   heap_size:         8
 *   3× microdb_arena:  3 × 24 = 72
 *   microdb_kv_ctx:    ~40
 *   microdb_ts_ctx:    MICRODB_TS_MAX_STREAMS × sizeof(microdb_ts_stream_t) + 4
 *                      = 8 × ~72 + 4 = ~580
 *   microdb_rel_ctx:   MICRODB_REL_MAX_TABLES × sizeof(microdb_table_s) + 4
 *                      = 4 × ~200 + 4 = ~804
 *   storage ptr:       8
 *   now ptr:           8
 *   initialized:       1 + 7 (pad)
 *   ─────────────────────────
 *   ~1533 → zaokrúhli na 1536
 *
 * Pre bezpečnosť použi:
 */
#define MICRODB_HANDLE_SIZE  1536

/*
 * MICRODB_SCHEMA_SIZE:
 *   sizeof(microdb_schema_s) = name + cols[] + col_count + max_rows +
 *                              row_size + index_col + sealed
 *   = 16 + (MICRODB_REL_MAX_COLS × sizeof(microdb_col_desc_t)) + ~24
 *   = 16 + (16 × ~52) + 24 = ~872 → zaokrúhli na 880
 */
#define MICRODB_SCHEMA_SIZE  880
```

**Pravidlo pre agenta:** Po implementácii interných štruktúr overiť skutočné
`sizeof()` hodnotami a prípadne `MICRODB_HANDLE_SIZE` / `MICRODB_SCHEMA_SIZE`
aktualizovať. Pridaj compile-time assert:

```c
_Static_assert(sizeof(struct microdb_s)        <= MICRODB_HANDLE_SIZE,
               "MICRODB_HANDLE_SIZE too small — update the define");
_Static_assert(sizeof(struct microdb_schema_s) <= MICRODB_SCHEMA_SIZE,
               "MICRODB_SCHEMA_SIZE too small — update the define");
```

---

## 11. Compile-time assertions

Všetky tieto asserty patria na začiatok `src/microdb.c`:

```c
/* Súčet PCT musí byť 100 */
_Static_assert(MICRODB_RAM_KV_PCT + MICRODB_RAM_TS_PCT + MICRODB_RAM_REL_PCT == 100,
               "MICRODB_RAM_KV_PCT + MICRODB_RAM_TS_PCT + MICRODB_RAM_REL_PCT must equal 100");

/* RAM_KB musí byť aspoň 8 */
_Static_assert(MICRODB_RAM_KB >= 8,
               "MICRODB_RAM_KB must be at least 8");

/* MAX_KEYS musí byť aspoň 1 */
_Static_assert(MICRODB_KV_MAX_KEYS >= 1,
               "MICRODB_KV_MAX_KEYS must be >= 1");

/* KEY_MAX_LEN musí byť aspoň 4 (pre zmysluplné kľúče) */
_Static_assert(MICRODB_KV_KEY_MAX_LEN >= 4,
               "MICRODB_KV_KEY_MAX_LEN must be >= 4");

/* VAL_MAX_LEN musí byť aspoň 1 */
_Static_assert(MICRODB_KV_VAL_MAX_LEN >= 1,
               "MICRODB_KV_VAL_MAX_LEN must be >= 1");

/* TS_MAX_STREAMS musí byť aspoň 1 */
_Static_assert(MICRODB_TS_MAX_STREAMS >= 1,
               "MICRODB_TS_MAX_STREAMS must be >= 1");

/* REL_MAX_TABLES musí byť aspoň 1 */
_Static_assert(MICRODB_REL_MAX_TABLES >= 1,
               "MICRODB_REL_MAX_TABLES must be >= 1");

/* REL_MAX_COLS musí byť aspoň 1 */
_Static_assert(MICRODB_REL_MAX_COLS >= 1,
               "MICRODB_REL_MAX_COLS must be >= 1");

/* Handle sizes */
_Static_assert(sizeof(struct microdb_s)        <= MICRODB_HANDLE_SIZE,
               "MICRODB_HANDLE_SIZE too small — update the define");
_Static_assert(sizeof(struct microdb_schema_s) <= MICRODB_SCHEMA_SIZE,
               "MICRODB_SCHEMA_SIZE too small — update the define");
```

---

## 12. CMakeLists.txt — presná štruktúra

### 12.1 Koreňový CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.16)
project(microdb C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ── Compiler warnings ────────────────────────────────────────────────────────
add_compile_options(
    -Wall -Wextra -Wpedantic
    -Wshadow -Wcast-align -Wstrict-prototypes
    -Wmissing-prototypes -Wconversion
    -Werror                   # Všetky warnings sú errors
)

# ── Build options ─────────────────────────────────────────────────────────────
option(MICRODB_BUILD_TESTS   "Build tests"    ON)
option(MICRODB_BUILD_EXAMPLES "Build examples" ON)
option(MICRODB_PORT_POSIX    "Build POSIX port" ON)

# ── Core knižnica ─────────────────────────────────────────────────────────────
add_library(microdb STATIC
    src/microdb.c
    src/microdb_kv.c
    src/microdb_ts.c
    src/microdb_rel.c
    src/microdb_wal.c
    src/microdb_crc.c
)

target_include_directories(microdb
    PUBLIC  include
    PRIVATE src
)

# ── POSIX port ────────────────────────────────────────────────────────────────
if(MICRODB_PORT_POSIX)
    add_library(microdb_port_posix STATIC
        port/posix/microdb_port_posix.c
    )
    target_link_libraries(microdb_port_posix PRIVATE microdb)
endif()

# ── RAM port (vždy pre testy) ─────────────────────────────────────────────────
add_library(microdb_port_ram STATIC
    port/ram/microdb_port_ram.c
)
target_link_libraries(microdb_port_ram PRIVATE microdb)

# ── Testy ─────────────────────────────────────────────────────────────────────
if(MICRODB_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ── Examples ──────────────────────────────────────────────────────────────────
if(MICRODB_BUILD_EXAMPLES AND MICRODB_PORT_POSIX)
    add_subdirectory(examples/posix_simulator)
endif()
```

### 12.2 tests/CMakeLists.txt

```cmake
# microtest je závislost — predpokladáme že je ako submodule alebo FetchContent
include(FetchContent)
FetchContent_Declare(microtest
    GIT_REPOSITORY https://github.com/Vanderhell/microtest.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(microtest)

foreach(TEST kv ts rel wal integration limits)
    add_executable(test_${TEST} test_${TEST}.c)
    target_link_libraries(test_${TEST}
        PRIVATE microdb microdb_port_ram microdb_port_posix microtest)
    add_test(NAME ${TEST} COMMAND test_${TEST})
endforeach()
```

---

## 13. Testovanie s microtest

microdb používa `microtest` — Vanderhellova vlastná test knižnica.
Tu sú presné vzory pre každý typ testu:

### 13.1 Základná štruktúra test súboru

```c
/* tests/test_kv.c */
#include "microtest.h"
#include "microdb.h"
#include "../port/ram/microdb_port_ram.h"

/* ── Globálne pre celý súbor ──────────────────────────────────────────────── */
static microdb_t         g_db;
static microdb_storage_t g_ram_storage;

static void setup(void) {
    microdb_port_ram_init(&g_ram_storage, 65536);   /* 64KB RAM storage */

    microdb_cfg_t cfg = {
        .storage = &g_ram_storage,
        .ram_kb  = 32,
        .now     = NULL,
    };
    ASSERT_EQ(microdb_init(&g_db, &cfg), MICRODB_OK);
}

static void teardown(void) {
    microdb_deinit(&g_db);
    microdb_port_ram_deinit(&g_ram_storage);
}

/* ── Testy ────────────────────────────────────────────────────────────────── */
MDB_TEST(kv_set_and_get_basic) {
    uint8_t val_in[4]  = {0xDE, 0xAD, 0xBE, 0xEF};
    uint8_t val_out[4] = {0};
    size_t  out_len    = 0;

    ASSERT_EQ(microdb_kv_put(&g_db, "testkey", val_in, sizeof(val_in)),
              MICRODB_OK);
    ASSERT_EQ(microdb_kv_get(&g_db, "testkey", val_out, sizeof(val_out), &out_len),
              MICRODB_OK);
    ASSERT_EQ(out_len, 4u);
    ASSERT_MEM_EQ(val_out, val_in, 4);
}

MDB_TEST(kv_get_nonexistent) {
    uint8_t buf[4];
    ASSERT_EQ(microdb_kv_get(&g_db, "nope", buf, sizeof(buf), NULL),
              MICRODB_ERR_NOT_FOUND);
}

/* ... ďalšie testy ... */

/* ── Main ─────────────────────────────────────────────────────────────────── */
int main(void) {
    MDB_RUN_SUITE("KV Engine", setup, teardown,
        kv_set_and_get_basic,
        kv_get_nonexistent,
        /* ... všetky testy z test_requirements ... */
    );
    return MDB_RESULT();
}
```

### 13.2 Mock clock pre TTL testy

```c
/* V test_kv.c — mock timestamp */
static uint32_t g_mock_time = 0;

static MICRODB_TIMESTAMP_TYPE mock_now(void) {
    return (MICRODB_TIMESTAMP_TYPE)g_mock_time;
}

/* V setup s TTL testami: */
static void setup_ttl(void) {
    g_mock_time = 1000;   /* Začíname na t=1000 */
    microdb_cfg_t cfg = {
        .storage = &g_ram_storage,
        .ram_kb  = 32,
        .now     = mock_now,
    };
    microdb_init(&g_db, &cfg);
}

/* V teste: */
MDB_TEST(kv_ttl_expires) {
    uint8_t val = 42;
    ASSERT_EQ(microdb_kv_set(&g_db, "expires", &val, 1, 10), MICRODB_OK);

    g_mock_time = 1009;   /* Ešte pred expiráciou */
    ASSERT_EQ(microdb_kv_exists(&g_db, "expires"), MICRODB_OK);

    g_mock_time = 1011;   /* Po expirácii */
    ASSERT_EQ(microdb_kv_exists(&g_db, "expires"), MICRODB_ERR_EXPIRED);
}
```

### 13.3 Test pattern pre WAL recovery

```c
MDB_TEST(wal_recovery_after_power_loss) {
    /* 1. Zapíš dáta */
    uint8_t val = 99;
    ASSERT_EQ(microdb_kv_put(&g_db, "survive", &val, 1), MICRODB_OK);

    /* 2. Simuluj power loss — deinit BEZ flush */
    /* Pre RAM port: priamo zavolaj internal deinit bez flush */
    microdb_port_ram_simulate_power_loss(&g_db);

    /* 3. Reinit — WAL replay musí obnoviť dáta */
    microdb_cfg_t cfg = { .storage = &g_ram_storage, .ram_kb = 32, .now = NULL };
    ASSERT_EQ(microdb_init(&g_db, &cfg), MICRODB_OK);

    /* 4. Skontroluj že dáta prežili */
    uint8_t recovered = 0;
    ASSERT_EQ(microdb_kv_get(&g_db, "survive", &recovered, 1, NULL), MICRODB_OK);
    ASSERT_EQ(recovered, 99);
}
```

---

## 14. Konvencie kódu

### 14.1 Naming

```
Public API:     microdb_*              (microdb_kv_set, microdb_ts_insert)
Internal:       mdb_*                  (mdb_kv_probe, mdb_ts_rb_insert)
Constants:      MICRODB_*              (MICRODB_OK, MICRODB_RAM_KB)
Types public:   microdb_*_t            (microdb_t, microdb_err_t)
Types internal: microdb_*_t alebo mdb_*_t
Enums:          MICRODB_KV_*, MICRODB_TS_*, MICRODB_REL_*, MICRODB_COL_*
```

### 14.2 Pravidlá

- Každá funkcia NAJPRV validuje vstupy, POTOM robí prácu.
- `NULL` check na každý pointer parameter — vracia `MICRODB_ERR_INVALID`.
- Žiadne `assert()` v produkčnom kóde — vždy vrať error code.
- Žiadny `printf` v knižničnom kóde — iba v testoch a príkladoch.
- Každá funkcia má max 50 riadkov. Ak viac → rozdeliť na pomocné funkcie.
- Každý súbor začína licenčnou hlavičkou (MIT, rovnaká ako microtest).
- `const` na všetko čo sa nemení.

### 14.3 Hlavičkový súbor vzor

```c
/*
 * microdb_kv.h — KV engine internals
 *
 * SPDX-License-Identifier: MIT
 * https://github.com/Vanderhell/microdb
 */

#ifndef MICRODB_KV_H
#define MICRODB_KV_H

#include "../include/microdb.h"
#include "microdb_arena.h"

/* ... interné typy a funkcie ... */

/* Init KV engine z arény. Volá sa z microdb_init(). */
microdb_err_t mdb_kv_init(microdb_kv_ctx_t *kv, microdb_arena_t *arena);

/* ... ostatné interné funkcie ... */

#endif /* MICRODB_KV_H */
```

---

## 15. Commit a tag stratégia

### 15.1 Commit message formát

```
<typ>(<scope>): <popis v angličtine, max 72 znakov>

Typy:
  feat     — nová funkcionalita
  fix      — oprava bugu
  test     — pridanie/oprava testov
  docs     — dokumentácia
  refactor — refaktoring bez zmeny správania
  perf     — optimalizácia výkonu
  chore    — build systém, závislosti

Scope:
  core, kv, ts, rel, wal, crc, hal, port, test, docs

Príklady:
  feat(kv): add LRU eviction for OVERWRITE policy
  feat(ts): implement ring buffer with DROP_OLDEST policy
  test(kv): add TTL expiry and LRU eviction tests (40/40 passing)
  feat(wal): implement WAL write and recovery
  feat(rel): add sorted index with binary search lookup
```

### 15.2 Tagy

```
Po dokončení každého kroku z implementačného poradia:

Krok 6 (KV hotový):   git tag v0.1.0
Krok 7 (TS hotový):   git tag v0.2.0
Krok 8 (REL hotový):  git tag v0.3.0
Krok 9 (WAL hotový):  git tag v0.4.0
Krok 10 (testy OK):   git tag v0.5.0
Krok 11 (ESP32 port): git tag v0.9.0
Krok 13 (README OK):  git tag v1.0.0
```

### 15.3 Vetvy

```
main         — vždy stabilný, vždy kompiluje, vždy testy prechádzajú
dev/kv       — práca na KV engine
dev/ts       — práca na TS engine
dev/rel      — práca na REL engine
dev/wal      — práca na WAL

Merge do main len keď všetky testy prechádzajú.
```

---

## 16. Checklist pred každým commitom

Agent musí overiť každý bod pred commitom:

```
[ ] cmake --build build/ --clean-first  →  nula warnings, nula errors
[ ] ctest --test-dir build/ -C Debug --output-on-failure  →  všetky testy PASS
[ ] Žiadny printf v src/ ani include/  (len v tests/ a examples/)
[ ] Žiadny malloc/free v src/ okrem microdb_init() a microdb_deinit()
[ ] Žiadny globálny mutable stav v src/ (všetko cez microdb_t handle)
[ ] Každá nová public funkcia má entry v microdb.h a doc komentár
[ ] Každá nová funkcia má aspoň 1 test
[ ] MICRODB_HANDLE_SIZE a MICRODB_SCHEMA_SIZE _Static_assert prechádzajú
[ ] Commit message dodržiava formát z §15.1
```

---

*microdb agent guide v1.0 — doplnok k microdb_spec.md*
*github.com/Vanderhell/microdb*

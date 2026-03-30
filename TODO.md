# TODO

Hotove:
- zalozena zakladna struktura projektu (`include/`, `src/`, `CMakeLists.txt`)
- vytvoreny verejny header `include/microdb.h` s compile-time konfiguraciou, typmi a API deklaraciami
- verejne handlye upravene podla `microdb_agent_guide.md` (`MICRODB_HANDLE_SIZE`, `MICRODB_SCHEMA_SIZE`, interny `microdb_table_t`)
- implementovane `microdb_init()`, `microdb_deinit()`, `microdb_flush()`, `microdb_stats()`
- zavedene rozdelenie jedinej alokacie RAM na KV/TS/REL slices
- pripraveny interny arena allocator a interny handle layout
- pridane samostatne moduly `src/microdb_arena.h` a `src/microdb_crc.[ch]`
- rozbehnuty funkcny zaklad KV enginu: set/get/del/exists/iter/purge_expired/clear
- TTL kontrola pri `get` a `exists`
- LRU eviction pri `MICRODB_KV_POLICY_OVERWRITE`
- compact value store po mazani alebo expirovani
- doplneny `port/ram/` HAL pre buduce testy
- build overeny mimo sandboxu, kniznica sa kompiluje
- pridany lokalny test shim `tests/microtest.h`
- KV engine dorobeny na plne spravanie pre aktualny scope:
- overwrite existujuceho kluca reuses val_pool slot bez zbytocneho growthu
- LRU eviction pre OVERWRITE policy
- samostatny build variant pre REJECT policy
- TTL/expire/get/exists/purge/iter/clear edge cases pokryte testami
- compaction trigger pri fragmentacii > 50%
- KV test suite rozsirena na 40/40 passing testov
- osetreny overwrite rovnakeho kluca bez zbytocneho rastu value pool metadata
- pripravene stuby pre TS, REL a WAL, aby sa projekt dal kompilovat a rozsirovat postupne

Rozpracovane / dalsie kroky:
- persistence cez WAL a storage HAL
- testy pre KV edge cases a limity
- TS engine zacaty:
- TS engine dokonceny pre aktualny scope:
- `register/insert/last/query/query_buf/count/clear`
- overflow policy build varianty: `DROP_OLDEST`, `REJECT`, `DOWNSAMPLE`
- downsample merge pre dve najstarsie samples
- TS test suite rozsirena na 35/35 passing testov
- REL engine dokonceny pre aktualny scope:
- schema seal/alignment, table create/get, row set/get, insert/find/find_by/delete/iter/count/clear
- sorted index + binary search + alive bitmap
- REL test suite rozsirena na 40/40 passing testov
- `port/posix/` HAL dokonceny pre file-backed persistence a power-loss simulaciu
- WAL engine dokonceny pre aktualny scope:
- WAL header + entry format
- write-ahead logging pre KV/TS/REL mutacie
- recovery po power-loss, stop na corrupt/partial entry, reset WAL po replay
- compaction do KV/TS/REL page regionov
- persistence metadata pre TS streamy a REL tabulky
- build variant bez WAL a samostatny stress variant s `MICRODB_KV_MAX_KEYS=128`
- WAL test coverage doplnena:
- `tests/test_wal.c` 25/25 passing
- `tests/test_wal_no_wal.c` passing
- `tests/test_wal_kv128.c` passing
- doplnit porty (`posix`, `ram`, `esp32`)
- doplnit integration + limits testy podla spec
- integration + limits testy doplnene:
- `tests/test_integration.c` passing
- `tests/test_limits.c` passing
- disabled-engine build varianty pre `MICRODB_ERR_DISABLED`
- small-limits 8KB build variant + compile-fail check pre zly sum PCT
- potom ist na `port/esp32/`, examples a README

Poznamky:
- KV/TS/REL/WAL/integration/limits milestone su hotove; dalsi krok je `port/esp32/`

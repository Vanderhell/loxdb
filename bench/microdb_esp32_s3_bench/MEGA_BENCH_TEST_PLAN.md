# Mega Bench Test Plan (Draft)

Podľa toho, čo máme v jadre (KV, TS, REL, WAL/compact, migrate, txn, reopen/reset/inspect), toto je checklist testov na neskoršiu implementáciu.

## 1. Boot / open / close / reopen

- Open prázdnej DB
- Open po wipe
- Reopen bez zmien
- Reopen po KV zápisoch
- Reopen po TS zápisoch
- Reopen po REL zápisoch
- Reopen po mixed workload
- Reopen po compact
- Reopen po migrate
- Reopen po successful txn commit
- Reopen po txn rollback
- Open s poškodeným headerom
- Open s neplatnou verziou formátu
- Open s neplatným magic
- Open s čiastočne zapísaným headerom
- Open s poškodeným inspect/stats metadata
- Open po dvoch reopen za sebou
- Open po mnohonásobnom open/close cykle
- Open keď storage driver vracia chybu read
- Open keď storage driver vracia chybu write

## 2. KV basic

- Put nového kľúča
- Put existujúceho kľúča overwrite
- Get existujúceho kľúča
- Get neexistujúceho kľúča
- Delete existujúceho kľúča
- Delete neexistujúceho kľúča
- Put key s minimálnou dĺžkou
- Put key s maximálnou dĺžkou
- Put value s nulovou dĺžkou
- Put value s maximálnou povolenou dĺžkou
- Put value o 1 byte väčšej než limit
- Put viacerých rôznych kľúčov
- Put rovnakého kľúča veľa krát
- Get po overwrite viackrát za sebou
- Delete a následný Get
- Delete a následný Put rovnakého kľúča
- Put binárnych dát s nulovými bytmi
- Put binárnych dát s 0xFF pattern
- Put binárnych dát s náhodným obsahom
- KV scan všetkých položiek

## 3. KV correctness / edge cases

- Kolízia hash/index pre dva rôzne kľúče
- Veľa kolízií v jednom buckete
- Eviction path korektne zachová nové dáta
- Eviction path nezničí staré dáta
- KV pri takmer plnej kapacite
- KV pri úplne plnej kapacite
- Put po full condition po compact/reclaim
- Delete tombstone zachovaný po reopen
- Viacnásobný delete toho istého kľúča
- Put→delete→put→delete rovnakého kľúča
- Iterácia nevracia delete položky
- Iterácia vracia najnovšiu verziu key
- Put key s rovnakým prefixom ako iný key
- Case-sensitive keys fungujú správne
- Long key + short value
- Short key + long value
- KV integrity po reopen
- KV integrity po compact
- KV integrity po migrate
- KV benchmark zároveň kontroluje obsah, nie len count

## 4. TS basic

- Create nového streamu
- Insert prvého sample do streamu
- Insert viacerých sample s rastúcim timestampom
- Query celý stream
- Query časového rozsahu
- Query prázdneho rozsahu
- Query neexistujúceho streamu
- Insert sample s minimálnym timestampom
- Insert sample s maximálnym timestampom
- Insert sample s nulovou payload dĺžkou
- Insert sample s maximálnou payload dĺžkou
- Insert sample väčšieho než limit
- Insert veľa sample do jedného streamu
- Insert sample do viacerých streamov
- Query po reopen
- Query po compact
- Query po migrate
- Query posledného sample
- Query prvého sample
- Count sample v streame

## 5. TS ordering / retention / correctness

- Insert out-of-order timestampu a očakávané správanie
- Insert duplicitného timestampu
- Insert duplicitného timestampu s inou hodnotou
- Query vracia správne poradie
- Query vracia len rozsah, nie susedné sample
- TS pri zaplnení segmentu
- TS rollover do ďalšieho segmentu
- TS retention zahodí najstaršie sample
- TS retention nepoškodí novšie sample
- TS stream metadata po reopen
- TS stream metadata po compact
- TS mixed inserts KV+TS
- TS mixed inserts REL+TS
- TS payload s binárnymi nulami
- TS payload s náhodnými dátami
- Query veľkého rozsahu cez viac segmentov
- Query streamu s jedným sample
- Query streamu po delete/reset DB
- TS fill percent zodpovedá realite
- TS inspect count sedí s reálnym počtom sample

## 6. REL basic

- Create tabuľky
- Create dvoch tabuliek
- Insert jedného riadku
- Insert viacerých riadkov
- Find row podľa primary key
- Find row podľa indexu
- Find neexistujúceho riadku
- Insert row s minimálnou veľkosťou
- Insert row s maximálnou veľkosťou
- Insert row väčšieho než limit
- Update existujúceho riadku
- Update neexistujúceho riadku
- Delete existujúceho riadku
- Delete neexistujúceho riadku
- Scan celej tabuľky
- Scan prázdnej tabuľky
- Reopen a find row
- Compact a find row
- Migrate a find row
- Mixed inserts do dvoch tabuliek

## 7. REL indexy / constraints / correctness

- Index sa vytvorí korektne
- Index nájde správny riadok
- Index nenájde zmazaný riadok
- Index sa opraví po update indexed field
- Duplicate key je správne odmietnutý
- Duplicate key v txn rollback sa neprepíše
- Delete row odstráni index entry
- Reopen zachová index konzistenciu
- Compact zachová index konzistenciu
- Scan poradie je definované alebo aspoň stabilne testované
- Insert s NULL-like hodnotami, ak ich formát podporuje
- Find po veľkom počte insertov
- Find po delete+insert rovnakého key
- Viacnásobný update toho istého row
- Row count sedí po mixed workload
- Table metadata po reopen
- Table metadata po compact
- Corrupt row record je detegovaný
- Corrupt index record je detegovaný
- REL inspect stats sú korektné

## 8. WAL / journaling

- WAL append jedného záznamu
- WAL append viacerých záznamov
- WAL replay po reopen
- WAL replay po simulated crash
- WAL partial write posledného záznamu
- WAL corrupt header
- WAL corrupt tail
- WAL corrupt CRC v jednom recorde
- WAL compact na prázdnom WAL
- WAL compact na čiastočne zaplnenom WAL
- WAL compact na takmer plnom WAL
- WAL compact zachová validné dáta
- WAL compact odstráni obsolete dáta
- WAL fill percent pred compact je korektné
- WAL fill percent po compact je korektné
- WAL reopen po compact
- WAL compact opakovane za sebou
- WAL append po compact
- WAL replay idempotentne po dvoch reopen
- WAL benchmark validuje obsah po compact

## 9. Transactions

- TXN begin→commit bez operácií
- TXN begin→rollback bez operácií
- TXN KV put→commit
- TXN KV put→rollback
- TXN KV delete→commit
- TXN KV delete→rollback
- TXN TS insert→commit
- TXN TS insert→rollback
- TXN REL insert→commit
- TXN REL insert→rollback
- TXN mixed KV+TS+REL commit
- TXN mixed KV+TS+REL rollback
- TXN viaceré operácie na rovnakom key commit
- TXN viaceré operácie na rovnakom key rollback
- TXN commit po reopen
- TXN rollback po reopen
- Simulated crash pred commit marker
- Simulated crash po commit marker pred finalize
- Simulated crash počas rollback
- Txn recovery je idempotentná

## 10. Migration / schema

- Open DB s old schema a migrate na new
- Migration callback sa zavolá presne raz
- Migration callback sa nezavolá pri rovnakej verzii
- Migration callback zlyhá a DB ostane konzistentná
- Partial migration po crash
- Recovery po partial migration
- Migrate KV dáta bez straty
- Migrate TS metadata bez straty
- Migrate REL tabuľky bez straty
- Migrate a následný reopen
- Migrate a následný compact
- Migrate dvoch verzií za sebou
- Downgrade nepovolený je odmietnutý
- Unknown future schema version je odmietnutá
- Migration mení len to, čo má zmeniť

## 11. Corruption / recovery / power-fail

- Power-cut počas header write
- Power-cut počas KV append
- Power-cut počas TS append
- Power-cut počas REL insert
- Power-cut počas WAL append
- Power-cut počas compact
- Power-cut počas metadata update
- Recovery po jednom poškodenom recorde
- Recovery po poškodenom segmente
- Recovery po poškodenom CRC
- Recovery po náhodných bytoch v tail oblasti
- Recovery po odtrhnutom poslednom write
- Recovery nevráti ghost records
- Recovery zachová last committed state
- Recovery po opakovanom crash cykle

## 12. Capacity / limits / stress

- Fill DB až po limit len KV
- Fill DB až po limit len TS
- Fill DB až po limit len REL
- Fill DB mixed workload do limitu
- Správanie pri ENOSPC stave
- Správanie po reclaim uvoľní miesto
- Long-run 10k operácií
- Long-run 100k operácií
- Reopen každých N operácií
- Compact každých N operácií
- Random mixed workload
- Random mixed workload + reopen
- Random mixed workload + compact
- Random mixed workload + crash injection
- Inspect stats po stress teste

## 13. API / contract / misuse

- Null pointer parameter je odmietnutý
- Invalid handle je odmietnutý
- Double close sa správa definovane
- API volanie pred open je odmietnuté
- API volanie po close je odmietnuté
- Buffer too small v query/read
- Output length je korektne vrátená
- Unsupported mode je odmietnutý
- Invalid table/stream name je odmietnuté
- Error codes sú stabilné a správne

## 14. Stats / inspect / observability

- Stats po fresh DB
- Stats po KV writes
- Stats po TS writes
- Stats po REL writes
- Stats po delete
- Stats po compact
- Stats po reopen
- Stats po migrate
- Stats po txn commit
- Stats po txn rollback
- Collision count sedí
- Evict count sedí
- WAL used/total sedí
- TS fill percent sedí
- REL row count sedí

## 15. Performance regression tests

- KV put latency median pod limit
- KV put worst-case pod limit
- TS insert latency median pod limit
- TS insert worst-case pod limit
- REL insert latency median pod limit
- REL find latency pod limit
- Compact latency pod limit
- Reopen latency pod limit
- Migration latency pod limit
- Recovery latency pod limit


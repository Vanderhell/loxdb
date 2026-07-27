# SPDX-License-Identifier: MIT
if(NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "SOURCE_DIR is required")
endif()

set(core_sources
    loxdb.c
    lox_arena.h
    lox_crc.c
    lox_crc.h
    lox_internal.h
    lox_kv.c
    lox_lock.h
    lox_rel.c
    lox_ts.c
    lox_wal.c
)
set(bundle_roots
    "bench/loxdb_esp32_s3_bench_head/src"
    "bench/loxdb_esp32_s3_bench_head/lox_esp32_s3_bench/src"
    "bench/loxdb_esp32_s3_sd_stress_bench/src"
)

function(assert_same_file canonical bundled)
    file(SHA256 "${canonical}" canonical_hash)
    file(SHA256 "${bundled}" bundled_hash)
    if(NOT canonical_hash STREQUAL bundled_hash)
        message(FATAL_ERROR "Stale benchmark bundle: ${bundled} differs from ${canonical}")
    endif()
endfunction()

foreach(bundle_root IN LISTS bundle_roots)
    foreach(source_name IN LISTS core_sources)
        assert_same_file(
            "${SOURCE_DIR}/src/${source_name}"
            "${SOURCE_DIR}/${bundle_root}/${source_name}"
        )
    endforeach()
    assert_same_file(
        "${SOURCE_DIR}/src/modules/lox_import_export.c"
        "${SOURCE_DIR}/${bundle_root}/lox_import_export.c"
    )
    assert_same_file(
        "${SOURCE_DIR}/src/modules/lox_json_wrapper.c"
        "${SOURCE_DIR}/${bundle_root}/lox_json_wrapper.c"
    )
endforeach()

set(bundle_header_roots
    "bench/loxdb_esp32_s3_bench_head"
    "bench/loxdb_esp32_s3_bench_head/lox_esp32_s3_bench"
    "bench/loxdb_esp32_s3_sd_stress_bench"
)
foreach(bundle_root IN LISTS bundle_header_roots)
    foreach(header_name IN ITEMS lox.h lox_import_export.h lox_json_wrapper.h)
        assert_same_file(
            "${SOURCE_DIR}/include/${header_name}"
            "${SOURCE_DIR}/${bundle_root}/${header_name}"
        )
    endforeach()
endforeach()

assert_same_file(
    "${SOURCE_DIR}/bench/loxdb_esp32_s3_bench_head/lox_esp32_s3_bench.ino"
    "${SOURCE_DIR}/bench/loxdb_esp32_s3_bench_head/lox_esp32_s3_bench/lox_esp32_s3_bench.ino"
)

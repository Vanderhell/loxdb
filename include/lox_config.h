// SPDX-License-Identifier: MIT
#ifndef LOX_CONFIG_H
#define LOX_CONFIG_H

#include <stdint.h>

#define LOX_CONFIG_PUBLIC_VERSION 1u
#define LOX_CONFIG_PROFILE_CORE_MIN 0u
#define LOX_CONFIG_PROFILE_CORE_WAL 1u
#define LOX_CONFIG_PROFILE_CORE_PERF 0u
#define LOX_CONFIG_PROFILE_CORE_HIMEM 0u
#define LOX_CONFIG_PROFILE_FOOTPRINT_MIN 0u
#define LOX_CONFIG_ENABLE_KV 1u
#define LOX_CONFIG_ENABLE_TS 1u
#define LOX_CONFIG_ENABLE_REL 1u
#define LOX_CONFIG_ENABLE_WAL 1u
#define LOX_CONFIG_THREAD_SAFE 0u
#define LOX_CONFIG_KV_MAX_KEYS 64u
#define LOX_CONFIG_KV_KEY_MAX_LEN 32u
#define LOX_CONFIG_KV_VAL_MAX_LEN 128u
#define LOX_CONFIG_TXN_STAGE_KEYS 8u
#define LOX_CONFIG_TS_MAX_STREAMS 8u
#define LOX_CONFIG_TS_STREAM_NAME_LEN 16u
#define LOX_CONFIG_HANDLE_SIZE 8192u
#define LOX_CONFIG_SCHEMA_SIZE 880u
#define LOX_CONFIG_TIMESTAMP_TYPE uint32_t
#define LOX_CONFIG_TS_RAW_MAX 16u
#define LOX_CONFIG_REL_INDEX_KEY_MAX 16u
#define LOX_CONFIG_REL_MAX_TABLES 4u
#define LOX_CONFIG_REL_MAX_COLS 16u
#define LOX_CONFIG_REL_COL_NAME_LEN 16u
#define LOX_CONFIG_REL_TABLE_NAME_LEN 16u

#ifndef LOX_PROFILE_CORE_MIN
#define LOX_PROFILE_CORE_MIN LOX_CONFIG_PROFILE_CORE_MIN
#elif LOX_PROFILE_CORE_MIN != LOX_CONFIG_PROFILE_CORE_MIN
#error "LOX_PROFILE_CORE_MIN does not match the installed loxdb ABI"
#endif

#ifndef LOX_PROFILE_CORE_WAL
#define LOX_PROFILE_CORE_WAL LOX_CONFIG_PROFILE_CORE_WAL
#elif LOX_PROFILE_CORE_WAL != LOX_CONFIG_PROFILE_CORE_WAL
#error "LOX_PROFILE_CORE_WAL does not match the installed loxdb ABI"
#endif

#ifndef LOX_PROFILE_CORE_PERF
#define LOX_PROFILE_CORE_PERF LOX_CONFIG_PROFILE_CORE_PERF
#elif LOX_PROFILE_CORE_PERF != LOX_CONFIG_PROFILE_CORE_PERF
#error "LOX_PROFILE_CORE_PERF does not match the installed loxdb ABI"
#endif

#ifndef LOX_PROFILE_CORE_HIMEM
#define LOX_PROFILE_CORE_HIMEM LOX_CONFIG_PROFILE_CORE_HIMEM
#elif LOX_PROFILE_CORE_HIMEM != LOX_CONFIG_PROFILE_CORE_HIMEM
#error "LOX_PROFILE_CORE_HIMEM does not match the installed loxdb ABI"
#endif

#ifndef LOX_PROFILE_FOOTPRINT_MIN
#define LOX_PROFILE_FOOTPRINT_MIN LOX_CONFIG_PROFILE_FOOTPRINT_MIN
#elif LOX_PROFILE_FOOTPRINT_MIN != LOX_CONFIG_PROFILE_FOOTPRINT_MIN
#error "LOX_PROFILE_FOOTPRINT_MIN does not match the installed loxdb ABI"
#endif

#ifndef LOX_ENABLE_KV
#define LOX_ENABLE_KV LOX_CONFIG_ENABLE_KV
#elif LOX_ENABLE_KV != LOX_CONFIG_ENABLE_KV
#error "LOX_ENABLE_KV does not match the installed loxdb ABI"
#endif

#ifndef LOX_ENABLE_TS
#define LOX_ENABLE_TS LOX_CONFIG_ENABLE_TS
#elif LOX_ENABLE_TS != LOX_CONFIG_ENABLE_TS
#error "LOX_ENABLE_TS does not match the installed loxdb ABI"
#endif

#ifndef LOX_ENABLE_REL
#define LOX_ENABLE_REL LOX_CONFIG_ENABLE_REL
#elif LOX_ENABLE_REL != LOX_CONFIG_ENABLE_REL
#error "LOX_ENABLE_REL does not match the installed loxdb ABI"
#endif

#ifndef LOX_ENABLE_WAL
#define LOX_ENABLE_WAL LOX_CONFIG_ENABLE_WAL
#elif LOX_ENABLE_WAL != LOX_CONFIG_ENABLE_WAL
#error "LOX_ENABLE_WAL does not match the installed loxdb ABI"
#endif

#ifndef LOX_THREAD_SAFE
#define LOX_THREAD_SAFE LOX_CONFIG_THREAD_SAFE
#elif LOX_THREAD_SAFE != LOX_CONFIG_THREAD_SAFE
#error "LOX_THREAD_SAFE does not match the installed loxdb ABI"
#endif

#ifndef LOX_KV_MAX_KEYS
#define LOX_KV_MAX_KEYS LOX_CONFIG_KV_MAX_KEYS
#elif LOX_KV_MAX_KEYS != LOX_CONFIG_KV_MAX_KEYS
#error "LOX_KV_MAX_KEYS does not match the installed loxdb ABI"
#endif

#ifndef LOX_KV_KEY_MAX_LEN
#define LOX_KV_KEY_MAX_LEN LOX_CONFIG_KV_KEY_MAX_LEN
#elif LOX_KV_KEY_MAX_LEN != LOX_CONFIG_KV_KEY_MAX_LEN
#error "LOX_KV_KEY_MAX_LEN does not match the installed loxdb ABI"
#endif

#ifndef LOX_KV_VAL_MAX_LEN
#define LOX_KV_VAL_MAX_LEN LOX_CONFIG_KV_VAL_MAX_LEN
#elif LOX_KV_VAL_MAX_LEN != LOX_CONFIG_KV_VAL_MAX_LEN
#error "LOX_KV_VAL_MAX_LEN does not match the installed loxdb ABI"
#endif

#ifndef LOX_TXN_STAGE_KEYS
#define LOX_TXN_STAGE_KEYS LOX_CONFIG_TXN_STAGE_KEYS
#elif LOX_TXN_STAGE_KEYS != LOX_CONFIG_TXN_STAGE_KEYS
#error "LOX_TXN_STAGE_KEYS does not match the installed loxdb ABI"
#endif

#ifndef LOX_TS_MAX_STREAMS
#define LOX_TS_MAX_STREAMS LOX_CONFIG_TS_MAX_STREAMS
#elif LOX_TS_MAX_STREAMS != LOX_CONFIG_TS_MAX_STREAMS
#error "LOX_TS_MAX_STREAMS does not match the installed loxdb ABI"
#endif

#ifndef LOX_TS_STREAM_NAME_LEN
#define LOX_TS_STREAM_NAME_LEN LOX_CONFIG_TS_STREAM_NAME_LEN
#elif LOX_TS_STREAM_NAME_LEN != LOX_CONFIG_TS_STREAM_NAME_LEN
#error "LOX_TS_STREAM_NAME_LEN does not match the installed loxdb ABI"
#endif

#ifndef LOX_HANDLE_SIZE
#define LOX_HANDLE_SIZE LOX_CONFIG_HANDLE_SIZE
#elif LOX_HANDLE_SIZE != LOX_CONFIG_HANDLE_SIZE
#error "LOX_HANDLE_SIZE does not match the installed loxdb ABI"
#endif

#ifndef LOX_SCHEMA_SIZE
#define LOX_SCHEMA_SIZE LOX_CONFIG_SCHEMA_SIZE
#elif LOX_SCHEMA_SIZE != LOX_CONFIG_SCHEMA_SIZE
#error "LOX_SCHEMA_SIZE does not match the installed loxdb ABI"
#endif

#ifndef LOX_TIMESTAMP_TYPE
#define LOX_TIMESTAMP_TYPE LOX_CONFIG_TIMESTAMP_TYPE
#endif
typedef LOX_TIMESTAMP_TYPE lox_config_timestamp_t;
typedef char lox_config_timestamp_width_check[(sizeof(lox_config_timestamp_t) == sizeof(uint32_t)) ? 1 : -1];

#ifndef LOX_TS_RAW_MAX
#define LOX_TS_RAW_MAX LOX_CONFIG_TS_RAW_MAX
#elif LOX_TS_RAW_MAX != LOX_CONFIG_TS_RAW_MAX
#error "LOX_TS_RAW_MAX does not match the installed loxdb ABI"
#endif

#ifndef LOX_REL_INDEX_KEY_MAX
#define LOX_REL_INDEX_KEY_MAX LOX_CONFIG_REL_INDEX_KEY_MAX
#elif LOX_REL_INDEX_KEY_MAX != LOX_CONFIG_REL_INDEX_KEY_MAX
#error "LOX_REL_INDEX_KEY_MAX does not match the installed loxdb ABI"
#endif

#ifndef LOX_REL_MAX_TABLES
#define LOX_REL_MAX_TABLES LOX_CONFIG_REL_MAX_TABLES
#elif LOX_REL_MAX_TABLES != LOX_CONFIG_REL_MAX_TABLES
#error "LOX_REL_MAX_TABLES does not match the installed loxdb ABI"
#endif

#ifndef LOX_REL_MAX_COLS
#define LOX_REL_MAX_COLS LOX_CONFIG_REL_MAX_COLS
#elif LOX_REL_MAX_COLS != LOX_CONFIG_REL_MAX_COLS
#error "LOX_REL_MAX_COLS does not match the installed loxdb ABI"
#endif

#ifndef LOX_REL_COL_NAME_LEN
#define LOX_REL_COL_NAME_LEN LOX_CONFIG_REL_COL_NAME_LEN
#elif LOX_REL_COL_NAME_LEN != LOX_CONFIG_REL_COL_NAME_LEN
#error "LOX_REL_COL_NAME_LEN does not match the installed loxdb ABI"
#endif

#ifndef LOX_REL_TABLE_NAME_LEN
#define LOX_REL_TABLE_NAME_LEN LOX_CONFIG_REL_TABLE_NAME_LEN
#elif LOX_REL_TABLE_NAME_LEN != LOX_CONFIG_REL_TABLE_NAME_LEN
#error "LOX_REL_TABLE_NAME_LEN does not match the installed loxdb ABI"
#endif

/* Stable build/runtime identity for install-time compatibility checks. */
#define LOX_CONFIG_FINGERPRINT                                                                 \
    (0x6C6F7864u ^ (uint32_t)LOX_CONFIG_HANDLE_SIZE ^                                          \
     ((uint32_t)LOX_CONFIG_SCHEMA_SIZE << 1u) ^                                                \
     ((uint32_t)sizeof(LOX_CONFIG_TIMESTAMP_TYPE) << 2u) ^                                     \
     ((uint32_t)LOX_CONFIG_TS_RAW_MAX << 3u) ^                                                 \
     ((uint32_t)LOX_CONFIG_REL_INDEX_KEY_MAX << 4u) ^                                          \
     ((uint32_t)LOX_CONFIG_PUBLIC_VERSION << 5u) ^                                             \
     ((uint32_t)LOX_CONFIG_PROFILE_CORE_MIN << 6u) ^                                           \
     ((uint32_t)LOX_CONFIG_PROFILE_CORE_WAL << 7u) ^                                           \
     ((uint32_t)LOX_CONFIG_PROFILE_CORE_PERF << 8u) ^                                          \
     ((uint32_t)LOX_CONFIG_PROFILE_CORE_HIMEM << 9u) ^                                          \
     ((uint32_t)LOX_CONFIG_PROFILE_FOOTPRINT_MIN << 10u) ^                                     \
     ((uint32_t)LOX_CONFIG_ENABLE_KV << 11u) ^                                                  \
     ((uint32_t)LOX_CONFIG_ENABLE_TS << 12u) ^                                                  \
     ((uint32_t)LOX_CONFIG_ENABLE_REL << 13u) ^                                                 \
     ((uint32_t)LOX_CONFIG_ENABLE_WAL << 14u) ^                                                 \
     ((uint32_t)LOX_CONFIG_THREAD_SAFE << 15u) ^                                                \
     ((uint32_t)LOX_CONFIG_KV_MAX_KEYS << 16u) ^                                                \
     ((uint32_t)LOX_CONFIG_KV_KEY_MAX_LEN << 17u) ^                                             \
     ((uint32_t)LOX_CONFIG_KV_VAL_MAX_LEN << 18u) ^                                             \
     ((uint32_t)LOX_CONFIG_TXN_STAGE_KEYS << 19u) ^                                             \
     ((uint32_t)LOX_CONFIG_TS_MAX_STREAMS << 20u) ^                                             \
     ((uint32_t)LOX_CONFIG_TS_STREAM_NAME_LEN << 21u) ^                                         \
     ((uint32_t)LOX_CONFIG_REL_MAX_TABLES << 22u) ^                                             \
     ((uint32_t)LOX_CONFIG_REL_MAX_COLS << 23u) ^                                               \
     ((uint32_t)LOX_CONFIG_REL_COL_NAME_LEN << 24u) ^                                           \
     ((uint32_t)LOX_CONFIG_REL_TABLE_NAME_LEN << 25u))

#endif

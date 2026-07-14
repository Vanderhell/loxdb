// SPDX-License-Identifier: MIT
#ifndef LOX_CONFIG_H
#define LOX_CONFIG_H

#include <stdint.h>

#define LOX_CONFIG_PUBLIC_VERSION 1u
#define LOX_CONFIG_HANDLE_SIZE 8192u
#define LOX_CONFIG_SCHEMA_SIZE 880u
#define LOX_CONFIG_TIMESTAMP_TYPE uint32_t
#define LOX_CONFIG_TS_RAW_MAX 16u
#define LOX_CONFIG_REL_INDEX_KEY_MAX 16u

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

/* Stable build/runtime identity for install-time compatibility checks. */
#define LOX_CONFIG_FINGERPRINT                                                                 \
    (0x6C6F7864u ^ (uint32_t)LOX_CONFIG_HANDLE_SIZE ^                                          \
     ((uint32_t)LOX_CONFIG_SCHEMA_SIZE << 1u) ^                                                \
     ((uint32_t)sizeof(LOX_CONFIG_TIMESTAMP_TYPE) << 2u) ^                                     \
     ((uint32_t)LOX_CONFIG_TS_RAW_MAX << 3u) ^                                                 \
     ((uint32_t)LOX_CONFIG_REL_INDEX_KEY_MAX << 4u) ^                                          \
     ((uint32_t)LOX_CONFIG_PUBLIC_VERSION << 5u))

#endif

// SPDX-License-Identifier: MIT
#ifndef LOX_PORT_ESP32_H
#define LOX_PORT_ESP32_H

#include "lox.h"

#if defined(IDF_TARGET)
#include "esp_partition.h"
#endif

typedef struct {
#if defined(IDF_TARGET)
    const esp_partition_t *partition;
#else
    const void *partition;
#endif
} lox_esp32_ctx_t;

/* Sync is a no-op here because esp_partition_* calls are synchronous.
 * The caller owns the context object and must keep it alive while storage uses it.
 */
lox_err_t lox_port_esp32_init(lox_storage_t *storage, lox_esp32_ctx_t *ctx, const char *partition_label);
void lox_port_esp32_deinit(lox_storage_t *storage);

#endif

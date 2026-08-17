// SPDX-License-Identifier: MIT
#define IDF_TARGET 1

#include "microtest.h"
#include "lox.h"
#include "esp_partition.h"

#include <string.h>

static uint8_t g_alpha_storage[64u];
static uint8_t g_beta_storage[64u];
static esp_partition_t g_alpha = { "alpha", 64u, 16u, g_alpha_storage };
static esp_partition_t g_beta = { "beta", 64u, 16u, g_beta_storage };

#include "../port/esp32/lox_port_esp32.c"

static void reset_partitions(void) {
    memset(g_alpha_storage, 0xFF, sizeof(g_alpha_storage));
    memset(g_beta_storage, 0xFF, sizeof(g_beta_storage));
}

static void setup_empty(void) {
}

static void teardown_empty(void) {
}

const esp_partition_t *esp_partition_find_first(uint8_t type, uint8_t subtype, const char *label) {
    (void)type;
    (void)subtype;
    if (label == NULL) {
        return NULL;
    }
    if (strcmp(label, "alpha") == 0) {
        return &g_alpha;
    }
    if (strcmp(label, "beta") == 0) {
        return &g_beta;
    }
    return NULL;
}

esp_err_t esp_partition_read(const esp_partition_t *partition, uint32_t offset, void *buf, size_t len) {
    if (partition == NULL || buf == NULL || offset > partition->size || len > (size_t)(partition->size - offset)) {
        return -1;
    }
    memcpy(buf, partition->storage + offset, len);
    return ESP_OK;
}

esp_err_t esp_partition_write(const esp_partition_t *partition, uint32_t offset, const void *buf, size_t len) {
    size_t i;

    if (partition == NULL || buf == NULL || offset > partition->size || len > (size_t)(partition->size - offset)) {
        return -1;
    }
    for (i = 0u; i < len; ++i) {
        uint8_t old = partition->storage[offset + i];
        uint8_t next = ((const uint8_t *)buf)[i];
        if ((old & next) != next) {
            return -1;
        }
    }
    for (i = 0u; i < len; ++i) {
        partition->storage[offset + i] = (uint8_t)(partition->storage[offset + i] & ((const uint8_t *)buf)[i]);
    }
    return ESP_OK;
}

esp_err_t esp_partition_erase_range(const esp_partition_t *partition, uint32_t offset, uint32_t len) {
    if (partition == NULL || offset > partition->size || len > (partition->size - offset) || len != partition->erase_size) {
        return -1;
    }
    if ((offset % partition->erase_size) != 0u) {
        return -1;
    }
    memset(partition->storage + offset, 0xFF, len);
    return ESP_OK;
}

MDB_TEST(esp32_two_caller_owned_contexts_are_isolated) {
    lox_storage_t alpha_storage;
    lox_storage_t beta_storage;
    lox_esp32_ctx_t alpha_ctx;
    lox_esp32_ctx_t beta_ctx;
    uint8_t alpha_value = 0xA5u;
    uint8_t beta_value = 0x5Au;
    uint8_t out = 0u;

    reset_partitions();
    ASSERT_EQ(lox_port_esp32_init(&alpha_storage, &alpha_ctx, "alpha"), LOX_OK);
    ASSERT_EQ(lox_port_esp32_init(&beta_storage, &beta_ctx, "beta"), LOX_OK);
    ASSERT_EQ(alpha_storage.ctx == beta_storage.ctx, 0);
    ASSERT_EQ(alpha_ctx.partition == beta_ctx.partition, 0);
    ASSERT_EQ(alpha_storage.sync(alpha_storage.ctx), LOX_OK);
    ASSERT_EQ(alpha_storage.erase(alpha_storage.ctx, 1u), LOX_ERR_INVALID);
    ASSERT_EQ(alpha_storage.write(alpha_storage.ctx, 0u, &alpha_value, 1u), LOX_OK);
    ASSERT_EQ(beta_storage.write(beta_storage.ctx, 0u, &beta_value, 1u), LOX_OK);
    ASSERT_EQ(alpha_storage.sync(alpha_storage.ctx), LOX_OK);
    ASSERT_EQ(beta_storage.sync(beta_storage.ctx), LOX_OK);

    lox_port_esp32_deinit(&alpha_storage);
    ASSERT_EQ(beta_storage.read(beta_storage.ctx, 0u, &out, 1u), LOX_OK);
    ASSERT_EQ(out, beta_value);
    ASSERT_EQ(beta_ctx.partition != NULL, 1);
    ASSERT_EQ(alpha_ctx.partition == NULL, 1);
    ASSERT_EQ(alpha_storage.ctx == NULL, 1);

    lox_port_esp32_deinit(&beta_storage);
}

MDB_TEST(esp32_init_rejects_missing_partition) {
    lox_storage_t storage;
    lox_esp32_ctx_t ctx;

    reset_partitions();
    ASSERT_EQ(lox_port_esp32_init(&storage, &ctx, "missing"), LOX_ERR_STORAGE);
}

int main(void) {
    MDB_RUN_TEST(setup_empty, teardown_empty, esp32_two_caller_owned_contexts_are_isolated);
    MDB_RUN_TEST(setup_empty, teardown_empty, esp32_init_rejects_missing_partition);
    return MDB_RESULT();
}

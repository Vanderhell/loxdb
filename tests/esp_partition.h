// SPDX-License-Identifier: MIT
#ifndef TESTS_ESP_PARTITION_H
#define TESTS_ESP_PARTITION_H

#include <stddef.h>
#include <stdint.h>

#ifndef ESP_OK
#define ESP_OK 0
#endif

typedef int esp_err_t;

typedef struct esp_partition_t {
    const char *label;
    uint32_t size;
    uint32_t erase_size;
    uint8_t *storage;
} esp_partition_t;

typedef enum {
    ESP_PARTITION_TYPE_DATA = 0,
    ESP_PARTITION_SUBTYPE_ANY = 0
} esp_partition_type_t;

const esp_partition_t *esp_partition_find_first(uint8_t type, uint8_t subtype, const char *label);
esp_err_t esp_partition_read(const esp_partition_t *partition, uint32_t offset, void *buf, size_t len);
esp_err_t esp_partition_write(const esp_partition_t *partition, uint32_t offset, const void *buf, size_t len);
esp_err_t esp_partition_erase_range(const esp_partition_t *partition, uint32_t offset, uint32_t len);

#endif

// SPDX-License-Identifier: MIT
#ifndef STRICT_NOR_EMULATOR_H
#define STRICT_NOR_EMULATOR_H

#include "lox.h"

#include <string.h>

#ifndef NOR_EMU_CAPACITY
#define NOR_EMU_CAPACITY 131072u
#endif

#ifndef NOR_EMU_TRACE_MAX
#define NOR_EMU_TRACE_MAX 2048u
#endif

typedef enum {
    NOR_TRACE_ERASE = 1u,
    NOR_TRACE_PROGRAM = 2u,
    NOR_TRACE_SYNC = 3u
} nor_trace_kind_t;

typedef struct {
    nor_trace_kind_t kind;
    uint32_t offset;
    uint32_t len;
    lox_err_t rc;
} nor_trace_event_t;

typedef struct {
    uint8_t durable[NOR_EMU_CAPACITY];
    uint8_t working[NOR_EMU_CAPACITY];
    nor_trace_event_t trace[NOR_EMU_TRACE_MAX];
    uint32_t trace_count;
    uint32_t sync_calls;
    uint32_t erase_calls;
    uint32_t program_calls;
    uint32_t erase_size;
    uint32_t program_unit;
    uint8_t fail_next_erase;
    uint8_t fail_next_program;
    uint8_t fail_next_sync;
    uint32_t torn_next_program_bytes;
} nor_flash_ctx_t;

static inline void nor_flash_trace(nor_flash_ctx_t *ctx, nor_trace_kind_t kind, uint32_t offset, uint32_t len, lox_err_t rc) {
    if (ctx == NULL || ctx->trace_count >= NOR_EMU_TRACE_MAX) {
        return;
    }
    ctx->trace[ctx->trace_count].kind = kind;
    ctx->trace[ctx->trace_count].offset = offset;
    ctx->trace[ctx->trace_count].len = len;
    ctx->trace[ctx->trace_count].rc = rc;
    ctx->trace_count++;
}

static inline void nor_flash_reset(nor_flash_ctx_t *ctx) {
    if (ctx == NULL) {
        return;
    }
    memset(ctx->durable, 0xFF, sizeof(ctx->durable));
    memcpy(ctx->working, ctx->durable, sizeof(ctx->working));
    ctx->trace_count = 0u;
    ctx->sync_calls = 0u;
    ctx->erase_calls = 0u;
    ctx->program_calls = 0u;
    ctx->fail_next_erase = 0u;
    ctx->fail_next_program = 0u;
    ctx->fail_next_sync = 0u;
    ctx->torn_next_program_bytes = 0u;
}

static inline void nor_flash_power_loss(nor_flash_ctx_t *ctx) {
    if (ctx == NULL) {
        return;
    }
    memcpy(ctx->working, ctx->durable, sizeof(ctx->working));
}

static inline lox_err_t nor_flash_read(void *ctx, uint32_t offset, void *buf, size_t len) {
    nor_flash_ctx_t *emu = (nor_flash_ctx_t *)ctx;
    if (emu == NULL || buf == NULL || ((uint64_t)offset + (uint64_t)len) > NOR_EMU_CAPACITY) {
        return LOX_ERR_STORAGE;
    }
    memcpy(buf, emu->working + offset, len);
    return LOX_OK;
}

static inline lox_err_t nor_flash_write(void *ctx, uint32_t offset, const void *buf, size_t len) {
    nor_flash_ctx_t *emu = (nor_flash_ctx_t *)ctx;
    uint32_t i;
    uint32_t limit;
    bool torn = false;
    if (emu == NULL || buf == NULL || ((uint64_t)offset + (uint64_t)len) > NOR_EMU_CAPACITY) {
        return LOX_ERR_STORAGE;
    }
    if (emu->program_unit == 0u) {
        return LOX_ERR_INVALID;
    }
    if ((offset % emu->program_unit) != 0u || (len % emu->program_unit) != 0u) {
        return LOX_ERR_STORAGE;
    }

    emu->program_calls++;
    if (emu->fail_next_program != 0u) {
        emu->fail_next_program = 0u;
        nor_flash_trace(emu, NOR_TRACE_PROGRAM, offset, (uint32_t)len, LOX_ERR_STORAGE);
        return LOX_ERR_STORAGE;
    }

    limit = (uint32_t)len;
    if (emu->torn_next_program_bytes != 0u && emu->torn_next_program_bytes < limit) {
        limit = emu->torn_next_program_bytes;
        emu->torn_next_program_bytes = 0u;
        torn = true;
    }

    for (i = 0u; i < limit; ++i) {
        uint8_t old = emu->working[offset + i];
        uint8_t next = ((const uint8_t *)buf)[i];
        if ((old & next) != next) {
            nor_flash_trace(emu, NOR_TRACE_PROGRAM, offset, (uint32_t)len, LOX_ERR_STORAGE);
            return LOX_ERR_STORAGE;
        }
        emu->working[offset + i] = (uint8_t)(old & next);
        if (torn) {
            emu->durable[offset + i] = emu->working[offset + i];
        }
    }

    if (limit != len) {
        nor_flash_trace(emu, NOR_TRACE_PROGRAM, offset, (uint32_t)len, LOX_ERR_STORAGE);
        return LOX_ERR_STORAGE;
    }

    nor_flash_trace(emu, NOR_TRACE_PROGRAM, offset, (uint32_t)len, LOX_OK);
    return LOX_OK;
}

static inline lox_err_t nor_flash_erase(void *ctx, uint32_t offset) {
    nor_flash_ctx_t *emu = (nor_flash_ctx_t *)ctx;
    uint32_t base;
    if (emu == NULL || emu->erase_size == 0u || offset >= NOR_EMU_CAPACITY) {
        return LOX_ERR_STORAGE;
    }
    if ((offset % emu->erase_size) != 0u) {
        return LOX_ERR_STORAGE;
    }
    if (((uint64_t)offset + (uint64_t)emu->erase_size) > NOR_EMU_CAPACITY) {
        return LOX_ERR_STORAGE;
    }

    emu->erase_calls++;
    if (emu->fail_next_erase != 0u) {
        emu->fail_next_erase = 0u;
        nor_flash_trace(emu, NOR_TRACE_ERASE, offset, emu->erase_size, LOX_ERR_STORAGE);
        return LOX_ERR_STORAGE;
    }

    base = offset;
    memset(emu->working + base, 0xFF, emu->erase_size);
    nor_flash_trace(emu, NOR_TRACE_ERASE, offset, emu->erase_size, LOX_OK);
    return LOX_OK;
}

static inline lox_err_t nor_flash_sync(void *ctx) {
    nor_flash_ctx_t *emu = (nor_flash_ctx_t *)ctx;
    if (emu == NULL) {
        return LOX_ERR_STORAGE;
    }
    emu->sync_calls++;
    if (emu->fail_next_sync != 0u) {
        emu->fail_next_sync = 0u;
        nor_flash_trace(emu, NOR_TRACE_SYNC, 0u, 0u, LOX_ERR_STORAGE);
        return LOX_ERR_STORAGE;
    }
    memcpy(emu->durable, emu->working, sizeof(emu->durable));
    nor_flash_trace(emu, NOR_TRACE_SYNC, 0u, 0u, LOX_OK);
    return LOX_OK;
}

static inline void nor_flash_bind_storage(lox_storage_t *storage,
                                              nor_flash_ctx_t *ctx,
                                              uint32_t erase_size,
                                              uint32_t program_unit) {
    if (storage == NULL || ctx == NULL) {
        return;
    }
    memset(storage, 0, sizeof(*storage));
    ctx->erase_size = erase_size;
    ctx->program_unit = program_unit;
    storage->read = nor_flash_read;
    storage->write = nor_flash_write;
    storage->erase = nor_flash_erase;
    storage->sync = nor_flash_sync;
    storage->capacity = NOR_EMU_CAPACITY;
    storage->erase_size = erase_size;
    storage->write_size = program_unit;
    storage->ctx = ctx;
}

#endif

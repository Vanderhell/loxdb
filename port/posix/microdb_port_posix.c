#include "microdb_port_posix.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *posix_file(void *ctx) {
    return (FILE *)((microdb_port_posix_ctx_t *)ctx)->file;
}

static microdb_err_t microdb_port_posix_read(void *ctx, uint32_t offset, void *buf, size_t len) {
    microdb_port_posix_ctx_t *posix = (microdb_port_posix_ctx_t *)ctx;
    FILE *fp;

    if (posix == NULL || buf == NULL || ((size_t)offset + len) > posix->capacity) {
        return MICRODB_ERR_STORAGE;
    }

    fp = posix_file(ctx);
    if (fseek(fp, (long)offset, SEEK_SET) != 0) {
        return MICRODB_ERR_STORAGE;
    }
    if (fread(buf, 1u, len, fp) != len) {
        return MICRODB_ERR_STORAGE;
    }
    return MICRODB_OK;
}

static microdb_err_t microdb_port_posix_write(void *ctx, uint32_t offset, const void *buf, size_t len) {
    microdb_port_posix_ctx_t *posix = (microdb_port_posix_ctx_t *)ctx;
    FILE *fp;

    if (posix == NULL || buf == NULL || ((size_t)offset + len) > posix->capacity) {
        return MICRODB_ERR_STORAGE;
    }

    fp = posix_file(ctx);
    if (fseek(fp, (long)offset, SEEK_SET) != 0) {
        return MICRODB_ERR_STORAGE;
    }
    if (fwrite(buf, 1u, len, fp) != len) {
        return MICRODB_ERR_STORAGE;
    }
    return MICRODB_OK;
}

static microdb_err_t microdb_port_posix_erase(void *ctx, uint32_t offset) {
    microdb_port_posix_ctx_t *posix = (microdb_port_posix_ctx_t *)ctx;
    FILE *fp;
    uint8_t *ff;
    uint32_t block_start;

    if (posix == NULL || offset >= posix->capacity) {
        return MICRODB_ERR_STORAGE;
    }

    ff = (uint8_t *)malloc(posix->erase_size);
    if (ff == NULL) {
        return MICRODB_ERR_NO_MEM;
    }
    memset(ff, 0xFF, posix->erase_size);
    block_start = (offset / posix->erase_size) * posix->erase_size;
    fp = posix_file(ctx);
    if (fseek(fp, (long)block_start, SEEK_SET) != 0 || fwrite(ff, 1u, posix->erase_size, fp) != posix->erase_size) {
        free(ff);
        return MICRODB_ERR_STORAGE;
    }
    free(ff);
    return MICRODB_OK;
}

static microdb_err_t microdb_port_posix_sync(void *ctx) {
    microdb_port_posix_ctx_t *posix = (microdb_port_posix_ctx_t *)ctx;
    if (posix == NULL) {
        return MICRODB_ERR_STORAGE;
    }
    return fflush(posix_file(ctx)) == 0 ? MICRODB_OK : MICRODB_ERR_STORAGE;
}

microdb_err_t microdb_port_posix_init(microdb_storage_t *storage, const char *path, uint32_t capacity) {
    microdb_port_posix_ctx_t *ctx;
    FILE *fp;
    uint8_t ff = 0xFFu;
    uint32_t i;

    if (storage == NULL || path == NULL || path[0] == '\0' || capacity == 0u) {
        return MICRODB_ERR_INVALID;
    }

    memset(storage, 0, sizeof(*storage));
    ctx = (microdb_port_posix_ctx_t *)malloc(sizeof(*ctx));
    if (ctx == NULL) {
        return MICRODB_ERR_NO_MEM;
    }
    memset(ctx, 0, sizeof(*ctx));
    if (strlen(path) >= sizeof(ctx->path)) {
        free(ctx);
        return MICRODB_ERR_INVALID;
    }
    memcpy(ctx->path, path, strlen(path) + 1u);
    ctx->capacity = capacity;
    ctx->erase_size = 256u;

#if defined(_MSC_VER)
    if (fopen_s(&fp, path, "w+b") != 0) {
        free(ctx);
        return MICRODB_ERR_STORAGE;
    }
#else
    fp = fopen(path, "w+b");
    if (fp == NULL) {
        free(ctx);
        return MICRODB_ERR_STORAGE;
    }
#endif

    for (i = 0; i < capacity; ++i) {
        if (fwrite(&ff, 1u, 1u, fp) != 1u) {
            fclose(fp);
            free(ctx);
            return MICRODB_ERR_STORAGE;
        }
    }
    fflush(fp);

    ctx->file = fp;
    storage->read = microdb_port_posix_read;
    storage->write = microdb_port_posix_write;
    storage->erase = microdb_port_posix_erase;
    storage->sync = microdb_port_posix_sync;
    storage->capacity = capacity;
    storage->erase_size = ctx->erase_size;
    storage->write_size = 1u;
    storage->ctx = ctx;
    return MICRODB_OK;
}

void microdb_port_posix_deinit(microdb_storage_t *storage) {
    microdb_port_posix_ctx_t *ctx;

    if (storage == NULL || storage->ctx == NULL) {
        return;
    }

    ctx = (microdb_port_posix_ctx_t *)storage->ctx;
    fclose(posix_file(ctx));
    remove(ctx->path);
    free(ctx);
    memset(storage, 0, sizeof(*storage));
}

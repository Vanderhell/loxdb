// SPDX-License-Identifier: MIT
#include "lox_port_posix.h"

#if defined(_WIN32)
#include <fcntl.h>
#include <errno.h>
#include <io.h>
#include <share.h>
#include <sys/stat.h>
#ifndef LOX_POSIX_SOPEN
#define LOX_POSIX_SOPEN _sopen_s
#endif
#ifndef LOX_POSIX_CLOSE
#define LOX_POSIX_CLOSE _close
#endif
#ifndef LOX_POSIX_READ
#define LOX_POSIX_READ _read
#endif
#ifndef LOX_POSIX_WRITE
#define LOX_POSIX_WRITE _write
#endif
#ifndef LOX_POSIX_LSEEK
#define LOX_POSIX_LSEEK _lseeki64
#endif
#ifndef LOX_POSIX_SYNC
#define LOX_POSIX_SYNC _commit
#endif
#define LOX_POSIX_FLAGS (_O_BINARY | _O_RDWR | _O_CREAT)
#else
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
#ifndef LOX_POSIX_CLOSE
#define LOX_POSIX_CLOSE close
#endif
#ifndef LOX_POSIX_OPEN
#define LOX_POSIX_OPEN open
#endif
#ifndef LOX_POSIX_READ
#define LOX_POSIX_READ read
#endif
#ifndef LOX_POSIX_WRITE
#define LOX_POSIX_WRITE write
#endif
#ifndef LOX_POSIX_LSEEK
#define LOX_POSIX_LSEEK lseek
#endif
#ifndef LOX_POSIX_SYNC
#define LOX_POSIX_SYNC fsync
#endif
#define LOX_POSIX_FLAGS (O_RDWR | O_CREAT)
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int posix_fd(void *ctx) {
    return (int)(intptr_t)((lox_port_posix_ctx_t *)ctx)->file;
}

static lox_err_t lox_port_posix_seek_fd(int fd, uint32_t offset) {
#if defined(_WIN32)
    return (LOX_POSIX_LSEEK(fd, (long long)offset, SEEK_SET) >= 0) ? LOX_OK : LOX_ERR_STORAGE;
#else
    return (LOX_POSIX_LSEEK(fd, (off_t)offset, SEEK_SET) >= 0) ? LOX_OK : LOX_ERR_STORAGE;
#endif
}

static lox_err_t lox_port_posix_full_read(int fd, void *buf, size_t len) {
    size_t done = 0u;

    while (done < len) {
        size_t want = len - done;
        long long rc;

        if (want > (size_t)INT_MAX) {
            want = (size_t)INT_MAX;
        }
        rc = (long long)LOX_POSIX_READ(fd, (uint8_t *)buf + done, (unsigned int)want);
        if (rc < 0) {
#if defined(EINTR)
            if (errno == EINTR) {
                continue;
            }
#endif
            return LOX_ERR_STORAGE;
        }
        if (rc == 0) {
            return LOX_ERR_STORAGE;
        }
        done += (size_t)rc;
    }

    return LOX_OK;
}

static lox_err_t lox_port_posix_full_write(int fd, const void *buf, size_t len) {
    size_t done = 0u;

    while (done < len) {
        size_t want = len - done;
        long long rc;

        if (want > (size_t)INT_MAX) {
            want = (size_t)INT_MAX;
        }
        rc = (long long)LOX_POSIX_WRITE(fd, (const uint8_t *)buf + done, (unsigned int)want);
        if (rc < 0) {
#if defined(EINTR)
            if (errno == EINTR) {
                continue;
            }
#endif
            return LOX_ERR_STORAGE;
        }
        if (rc == 0) {
            return LOX_ERR_STORAGE;
        }
        done += (size_t)rc;
    }

    return LOX_OK;
}

static lox_err_t lox_port_posix_sync_fd(int fd) {
    int rc;

    do {
        rc = LOX_POSIX_SYNC(fd);
    } while (rc != 0 &&
#if defined(EINTR)
             errno == EINTR
#else
             0
#endif
    );

    return (rc == 0) ? LOX_OK : LOX_ERR_STORAGE;
}

static lox_err_t lox_port_posix_read(void *ctx, uint32_t offset, void *buf, size_t len) {
    lox_port_posix_ctx_t *posix = (lox_port_posix_ctx_t *)ctx;
    int fd;

    if (posix == NULL || buf == NULL || ((size_t)offset + len) > posix->capacity) {
        return LOX_ERR_STORAGE;
    }

    fd = posix_fd(ctx);
    if (fd < 0 || lox_port_posix_seek_fd(fd, offset) != LOX_OK) {
        return LOX_ERR_STORAGE;
    }
    if (lox_port_posix_full_read(fd, buf, len) != LOX_OK) {
        return LOX_ERR_STORAGE;
    }
    return LOX_OK;
}

static lox_err_t lox_port_posix_write(void *ctx, uint32_t offset, const void *buf, size_t len) {
    lox_port_posix_ctx_t *posix = (lox_port_posix_ctx_t *)ctx;
    int fd;

    if (posix == NULL || buf == NULL || ((size_t)offset + len) > posix->capacity) {
        return LOX_ERR_STORAGE;
    }

    fd = posix_fd(ctx);
    if (fd < 0 || lox_port_posix_seek_fd(fd, offset) != LOX_OK) {
        return LOX_ERR_STORAGE;
    }
    if (lox_port_posix_full_write(fd, buf, len) != LOX_OK) {
        return LOX_ERR_STORAGE;
    }
    return LOX_OK;
}

static lox_err_t lox_port_posix_erase(void *ctx, uint32_t offset) {
    lox_port_posix_ctx_t *posix = (lox_port_posix_ctx_t *)ctx;
    int fd;
    uint8_t *ff;
    uint32_t block_start;

    if (posix == NULL || offset >= posix->capacity) {
        return LOX_ERR_STORAGE;
    }

    ff = (uint8_t *)malloc(posix->erase_size);
    if (ff == NULL) {
        return LOX_ERR_NO_MEM;
    }
    memset(ff, 0xFF, posix->erase_size);
    block_start = (offset / posix->erase_size) * posix->erase_size;
    fd = posix_fd(ctx);
    if (fd < 0 || lox_port_posix_seek_fd(fd, block_start) != LOX_OK ||
        lox_port_posix_full_write(fd, ff, posix->erase_size) != LOX_OK) {
        free(ff);
        return LOX_ERR_STORAGE;
    }
    free(ff);
    return LOX_OK;
}

static lox_err_t lox_port_posix_sync(void *ctx) {
    lox_port_posix_ctx_t *posix = (lox_port_posix_ctx_t *)ctx;
    if (posix == NULL) {
        return LOX_ERR_STORAGE;
    }
    return lox_port_posix_sync_fd(posix_fd(ctx));
}

lox_err_t lox_port_posix_init(lox_storage_t *storage, const char *path, uint32_t capacity) {
    lox_port_posix_ctx_t *ctx;
    int fd;
    uint8_t ff_block[4096];
    long long current_size = 0;
    uint32_t remaining = 0u;
    uint32_t chunk = 0u;

    if (storage == NULL || path == NULL || path[0] == '\0' || capacity == 0u) {
        return LOX_ERR_INVALID;
    }

    memset(storage, 0, sizeof(*storage));
    ctx = (lox_port_posix_ctx_t *)malloc(sizeof(*ctx));
    if (ctx == NULL) {
        return LOX_ERR_NO_MEM;
    }
    memset(ctx, 0, sizeof(*ctx));
    if (strlen(path) >= sizeof(ctx->path)) {
        free(ctx);
        return LOX_ERR_INVALID;
    }
    memcpy(ctx->path, path, strlen(path) + 1u);
    ctx->capacity = capacity;
    ctx->erase_size = 256u;

#if defined(_WIN32)
    {
        errno_t open_err = LOX_POSIX_SOPEN(&fd, path, LOX_POSIX_FLAGS, _SH_DENYNO, _S_IREAD | _S_IWRITE);
        if (open_err != 0) {
            free(ctx);
            return LOX_ERR_STORAGE;
        }
    }
#else
    fd = LOX_POSIX_OPEN(path, LOX_POSIX_FLAGS, 0666);
    if (fd < 0) {
        free(ctx);
        return LOX_ERR_STORAGE;
    }
#endif

    current_size = LOX_POSIX_LSEEK(fd, 0LL, SEEK_END);
    if (current_size < 0) {
        LOX_POSIX_CLOSE(fd);
        free(ctx);
        return LOX_ERR_STORAGE;
    }

    if ((uint64_t)current_size < (uint64_t)capacity) {
        memset(ff_block, 0xFF, sizeof(ff_block));
        if (LOX_POSIX_LSEEK(fd, current_size, SEEK_SET) < 0) {
            LOX_POSIX_CLOSE(fd);
            free(ctx);
            return LOX_ERR_STORAGE;
        }
        remaining = capacity - (uint32_t)current_size;
        while (remaining > 0u) {
            chunk = (remaining > sizeof(ff_block)) ? (uint32_t)sizeof(ff_block) : remaining;
            if (lox_port_posix_full_write(fd, ff_block, chunk) != LOX_OK) {
                LOX_POSIX_CLOSE(fd);
                free(ctx);
                return LOX_ERR_STORAGE;
            }
            remaining -= chunk;
        }
    }
    if (lox_port_posix_sync_fd(fd) != LOX_OK) {
        LOX_POSIX_CLOSE(fd);
        free(ctx);
        return LOX_ERR_STORAGE;
    }

    ctx->file = (void *)(intptr_t)fd;
    storage->read = lox_port_posix_read;
    storage->write = lox_port_posix_write;
    storage->erase = lox_port_posix_erase;
    storage->sync = lox_port_posix_sync;
    storage->capacity = capacity;
    storage->erase_size = ctx->erase_size;
    storage->write_size = 1u;
    storage->ctx = ctx;
    return LOX_OK;
}

void lox_port_posix_deinit(lox_storage_t *storage) {
    lox_port_posix_ctx_t *ctx;

    if (storage == NULL || storage->ctx == NULL) {
        return;
    }

    ctx = (lox_port_posix_ctx_t *)storage->ctx;
    if (ctx->file != NULL) {
        LOX_POSIX_CLOSE(posix_fd(ctx));
    }
    free(ctx);
    memset(storage, 0, sizeof(*storage));
}

void lox_port_posix_simulate_power_loss(lox_storage_t *storage) {
    lox_port_posix_ctx_t *ctx;
    if (storage == NULL || storage->ctx == NULL) {
        return;
    }

    ctx = (lox_port_posix_ctx_t *)storage->ctx;
    if (ctx->file != NULL) {
        (void)LOX_POSIX_CLOSE(posix_fd(ctx));
    }
    ctx->file = NULL;
}

void lox_port_posix_remove(const char *path) {
    if (path != NULL) {
        (void)remove(path);
    }
}

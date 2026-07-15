// SPDX-License-Identifier: MIT
#include "microtest.h"
#include "lox.h"

#include <errno.h>
#include <limits.h>
#include <string.h>

static uint8_t g_file[512u];
static size_t g_file_size = 0u;
static long long g_seek_pos = 0;
static int g_open_calls = 0;
static int g_close_calls = 0;
static int g_sync_calls = 0;
static int g_read_eintr_once = 0;
static int g_write_eintr_once = 0;
static int g_sync_fail_once = 0;
static unsigned g_max_read_chunk = 2u;
static unsigned g_max_write_chunk = 2u;

static int test_open(const char *filename, int oflag, ...);
static int test_sopen_s(int *pfh, const char *filename, int oflag, int shflag, int pmode);
static int test_close(int fd);
static long long test_lseek64(int fd, long long offset, int origin);
static int test_read(int fd, void *buf, unsigned int len);
static int test_write(int fd, const void *buf, unsigned int len);
static int test_commit(int fd);

#if defined(_WIN32)
#define LOX_POSIX_SOPEN test_sopen_s
#else
#define LOX_POSIX_OPEN test_open
#endif
#define LOX_POSIX_CLOSE test_close
#define LOX_POSIX_READ test_read
#define LOX_POSIX_WRITE test_write
#define LOX_POSIX_LSEEK test_lseek64
#define LOX_POSIX_SYNC test_commit
#include "../port/posix/lox_port_posix.c"

static void reset_fake_file(void) {
    memset(g_file, 0x00, sizeof(g_file));
    g_file_size = 0u;
    g_seek_pos = 0;
    g_open_calls = 0;
    g_close_calls = 0;
    g_sync_calls = 0;
    g_read_eintr_once = 0;
    g_write_eintr_once = 0;
    g_sync_fail_once = 0;
    g_max_read_chunk = 2u;
    g_max_write_chunk = 2u;
}

static void setup_empty(void) {
}

static void teardown_empty(void) {
}

static int test_open(const char *filename, int oflag, ...) {
    (void)filename;
    (void)oflag;
    g_open_calls++;
    g_seek_pos = 0;
    return 17;
}

static int test_sopen_s(int *pfh, const char *filename, int oflag, int shflag, int pmode) {
    (void)filename;
    (void)oflag;
    (void)shflag;
    (void)pmode;
    if (pfh == NULL) {
        return EINVAL;
    }
    g_open_calls++;
    g_seek_pos = 0;
    *pfh = 17;
    return 0;
}

static int test_close(int fd) {
    (void)fd;
    g_close_calls++;
    return 0;
}

static long long test_lseek64(int fd, long long offset, int origin) {
    (void)fd;
    switch (origin) {
        case SEEK_SET:
            g_seek_pos = offset;
            break;
        case SEEK_CUR:
            g_seek_pos += offset;
            break;
        case SEEK_END:
            g_seek_pos = (long long)g_file_size + offset;
            break;
        default:
            return -1;
    }
    if (g_seek_pos < 0) {
        return -1;
    }
    return g_seek_pos;
}

static int test_read(int fd, void *buf, unsigned int len) {
    unsigned int want;

    (void)fd;
    if (g_read_eintr_once != 0) {
        g_read_eintr_once = 0;
        errno = EINTR;
        return -1;
    }
    if ((size_t)g_seek_pos >= g_file_size) {
        return 0;
    }
    want = len;
    if (want > g_max_read_chunk) {
        want = g_max_read_chunk;
    }
    if (((size_t)g_seek_pos + (size_t)want) > g_file_size) {
        want = (unsigned int)(g_file_size - (size_t)g_seek_pos);
    }
    memcpy(buf, g_file + (size_t)g_seek_pos, want);
    g_seek_pos += (long long)want;
    return (int)want;
}

static int test_write(int fd, const void *buf, unsigned int len) {
    unsigned int want;

    (void)fd;
    if (g_write_eintr_once != 0) {
        g_write_eintr_once = 0;
        errno = EINTR;
        return -1;
    }
    want = len;
    if (want > g_max_write_chunk) {
        want = g_max_write_chunk;
    }
    if (((size_t)g_seek_pos + (size_t)want) > sizeof(g_file)) {
        want = (unsigned int)(sizeof(g_file) - (size_t)g_seek_pos);
    }
    memcpy(g_file + (size_t)g_seek_pos, buf, want);
    g_seek_pos += (long long)want;
    if ((size_t)g_seek_pos > g_file_size) {
        g_file_size = (size_t)g_seek_pos;
    }
    return (int)want;
}

static int test_commit(int fd) {
    (void)fd;
    g_sync_calls++;
    if (g_sync_fail_once != 0) {
        g_sync_fail_once = 0;
        errno = EIO;
        return -1;
    }
    return 0;
}

MDB_TEST(posix_short_io_and_eintr_are_retried) {
    lox_storage_t storage;
    uint8_t input[6u] = { 1u, 2u, 3u, 4u, 5u, 6u };
    uint8_t output[6u];
    uint32_t i;

    reset_fake_file();
    g_write_eintr_once = 1;
    g_read_eintr_once = 1;
    ASSERT_EQ(lox_port_posix_init(&storage, "phase04-posix.bin", 256u), LOX_OK);
    ASSERT_EQ(g_open_calls, 1);
    ASSERT_EQ(storage.capacity, 256u);
    ASSERT_EQ(storage.erase_size, 256u);
    ASSERT_EQ(storage.write_size, 1u);

    ASSERT_EQ(storage.write(storage.ctx, 8u, input, sizeof(input)), LOX_OK);
    memset(output, 0, sizeof(output));
    ASSERT_EQ(storage.read(storage.ctx, 8u, output, sizeof(output)), LOX_OK);
    ASSERT_MEM_EQ(output, input, sizeof(input));

    ASSERT_EQ(storage.erase(storage.ctx, 0u), LOX_OK);
    for (i = 0u; i < storage.erase_size; ++i) {
        ASSERT_EQ(g_file[i], 0xFFu);
    }

    ASSERT_EQ(storage.sync(storage.ctx), LOX_OK);
    ASSERT_EQ(g_sync_calls, 2);
    lox_port_posix_deinit(&storage);
    ASSERT_EQ(g_close_calls, 1);
    ASSERT_EQ(storage.ctx == NULL, 1);
}

MDB_TEST(posix_init_sync_failure_cleans_up_storage) {
    lox_storage_t storage;

    reset_fake_file();
    g_sync_fail_once = 1;
    ASSERT_EQ(lox_port_posix_init(&storage, "phase04-posix-fail.bin", 256u), LOX_ERR_STORAGE);
    ASSERT_EQ(g_open_calls, 1);
    ASSERT_EQ(g_close_calls, 1);
    ASSERT_EQ(storage.ctx == NULL, 1);
}

int main(void) {
    MDB_RUN_TEST(setup_empty, teardown_empty, posix_short_io_and_eintr_are_retried);
    MDB_RUN_TEST(setup_empty, teardown_empty, posix_init_sync_failure_cleans_up_storage);
    return MDB_RESULT();
}

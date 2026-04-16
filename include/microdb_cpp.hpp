#ifndef MICRODB_CPP_HPP
#define MICRODB_CPP_HPP

#include <cstring>

extern "C" {
#include "microdb.h"
}

namespace microdb {
namespace cpp {

class Database final {
public:
    Database() = default;
    ~Database() {
        if (initialized_) {
            (void)microdb_deinit(&db_);
        }
    }

    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;
    Database(Database &&) = delete;
    Database &operator=(Database &&) = delete;

    microdb_err_t init(const microdb_cfg_t &cfg) {
        microdb_err_t rc;
        if (initialized_) {
            return MICRODB_ERR_INVALID;
        }
        rc = microdb_init(&db_, &cfg);
        if (rc == MICRODB_OK) {
            initialized_ = true;
        }
        return rc;
    }

    microdb_err_t deinit() {
        microdb_err_t rc;
        if (!initialized_) {
            return MICRODB_ERR_INVALID;
        }
        rc = microdb_deinit(&db_);
        if (rc == MICRODB_OK) {
            initialized_ = false;
            std::memset(&db_, 0, sizeof(db_));
        }
        return rc;
    }

    bool initialized() const {
        return initialized_;
    }

    microdb_t *handle() {
        return initialized_ ? &db_ : nullptr;
    }

    const microdb_t *handle() const {
        return initialized_ ? &db_ : nullptr;
    }

    microdb_err_t flush() {
        if (!initialized_) {
            return MICRODB_ERR_INVALID;
        }
        return microdb_flush(&db_);
    }

    microdb_err_t stats(microdb_stats_t *out) const {
        if (!initialized_) {
            return MICRODB_ERR_INVALID;
        }
        return microdb_stats(&db_, out);
    }

    microdb_err_t db_stats(microdb_db_stats_t *out) {
        if (!initialized_) {
            return MICRODB_ERR_INVALID;
        }
        return microdb_get_db_stats(&db_, out);
    }

    microdb_err_t kv_stats(microdb_kv_stats_t *out) {
        if (!initialized_) {
            return MICRODB_ERR_INVALID;
        }
        return microdb_get_kv_stats(&db_, out);
    }

    microdb_err_t ts_stats(microdb_ts_stats_t *out) {
        if (!initialized_) {
            return MICRODB_ERR_INVALID;
        }
        return microdb_get_ts_stats(&db_, out);
    }

    microdb_err_t rel_stats(microdb_rel_stats_t *out) {
        if (!initialized_) {
            return MICRODB_ERR_INVALID;
        }
        return microdb_get_rel_stats(&db_, out);
    }

    microdb_err_t effective_capacity(microdb_effective_capacity_t *out) {
        if (!initialized_) {
            return MICRODB_ERR_INVALID;
        }
        return microdb_get_effective_capacity(&db_, out);
    }

    microdb_err_t pressure(microdb_pressure_t *out) {
        if (!initialized_) {
            return MICRODB_ERR_INVALID;
        }
        return microdb_get_pressure(&db_, out);
    }

    microdb_err_t kv_set(const char *key, const void *val, size_t len, uint32_t ttl = 0u) {
        if (!initialized_) {
            return MICRODB_ERR_INVALID;
        }
        return microdb_kv_set(&db_, key, val, len, ttl);
    }

    microdb_err_t kv_put(const char *key, const void *val, size_t len) {
        return kv_set(key, val, len, 0u);
    }

    microdb_err_t kv_get(const char *key, void *buf, size_t buf_len, size_t *out_len = nullptr) {
        if (!initialized_) {
            return MICRODB_ERR_INVALID;
        }
        return microdb_kv_get(&db_, key, buf, buf_len, out_len);
    }

    microdb_err_t kv_del(const char *key) {
        if (!initialized_) {
            return MICRODB_ERR_INVALID;
        }
        return microdb_kv_del(&db_, key);
    }

    microdb_err_t kv_exists(const char *key) {
        if (!initialized_) {
            return MICRODB_ERR_INVALID;
        }
        return microdb_kv_exists(&db_, key);
    }

    microdb_err_t kv_clear() {
        if (!initialized_) {
            return MICRODB_ERR_INVALID;
        }
        return microdb_kv_clear(&db_);
    }

    microdb_err_t kv_purge_expired() {
        if (!initialized_) {
            return MICRODB_ERR_INVALID;
        }
        return microdb_kv_purge_expired(&db_);
    }

    microdb_err_t kv_iter(microdb_kv_iter_cb_t cb, void *ctx) {
        if (!initialized_) {
            return MICRODB_ERR_INVALID;
        }
        return microdb_kv_iter(&db_, cb, ctx);
    }

    microdb_err_t admit_kv_set(const char *key, size_t val_len, microdb_admission_t *out) {
        if (!initialized_) {
            return MICRODB_ERR_INVALID;
        }
        return microdb_admit_kv_set(&db_, key, val_len, out);
    }

private:
    microdb_t db_ {};
    bool initialized_ = false;
};

}  // namespace cpp
}  // namespace microdb

#endif

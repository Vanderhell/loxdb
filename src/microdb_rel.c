#include "microdb_internal.h"

#include <string.h>

static microdb_err_t microdb_rel_unavailable(const void *ptr) {
    if (ptr == NULL) {
        return MICRODB_ERR_INVALID;
    }
    return MICRODB_ERR_DISABLED;
}

#if MICRODB_ENABLE_REL
microdb_err_t microdb_schema_init(microdb_schema_t *schema, const char *name, uint32_t max_rows) {
    microdb_schema_impl_t *impl;
    size_t len;

    if (schema == NULL || name == NULL || name[0] == '\0' || max_rows == 0u) {
        return MICRODB_ERR_INVALID;
    }

    len = strlen(name);
    if (len >= MICRODB_REL_TABLE_NAME_LEN) {
        return MICRODB_ERR_INVALID;
    }

    memset(schema, 0, sizeof(*schema));
    impl = (microdb_schema_impl_t *)&schema->_opaque[0];
    memcpy(impl->name, name, len + 1u);
    impl->max_rows = max_rows;
    return MICRODB_OK;
}

microdb_err_t microdb_schema_add(microdb_schema_t *schema, const char *col_name, microdb_col_type_t type, size_t size, bool is_index) {
    (void)col_name;
    (void)type;
    (void)size;
    (void)is_index;
    return microdb_rel_unavailable(schema);
}

microdb_err_t microdb_schema_seal(microdb_schema_t *schema) {
    return microdb_rel_unavailable(schema);
}

microdb_err_t microdb_table_create(microdb_t *db, microdb_schema_t *schema) {
    (void)schema;
    return microdb_rel_unavailable(db);
}

microdb_err_t microdb_table_get(microdb_t *db, const char *name, microdb_table_t **out_table) {
    (void)name;
    (void)out_table;
    return microdb_rel_unavailable(db);
}

size_t microdb_table_row_size(const microdb_table_t *table) {
    if (table == NULL) {
        return 0u;
    }
    return table->row_size;
}

microdb_err_t microdb_row_set(const microdb_table_t *table, void *row_buf, const char *col_name, const void *val) {
    (void)row_buf;
    (void)col_name;
    (void)val;
    return microdb_rel_unavailable(table);
}

microdb_err_t microdb_row_get(const microdb_table_t *table, const void *row_buf, const char *col_name, void *out, size_t *out_len) {
    (void)row_buf;
    (void)col_name;
    (void)out;
    (void)out_len;
    return microdb_rel_unavailable(table);
}

microdb_err_t microdb_rel_insert(microdb_t *db, microdb_table_t *table, const void *row_buf) {
    (void)table;
    (void)row_buf;
    return microdb_rel_unavailable(db);
}

microdb_err_t microdb_rel_find(microdb_t *db, microdb_table_t *table, const void *search_val, microdb_rel_iter_cb_t cb, void *ctx) {
    (void)table;
    (void)search_val;
    (void)cb;
    (void)ctx;
    return microdb_rel_unavailable(db);
}

microdb_err_t microdb_rel_find_by(microdb_t *db, microdb_table_t *table, const char *col_name, const void *search_val, void *out_buf) {
    (void)table;
    (void)col_name;
    (void)search_val;
    (void)out_buf;
    return microdb_rel_unavailable(db);
}

microdb_err_t microdb_rel_delete(microdb_t *db, microdb_table_t *table, const void *search_val, uint32_t *out_deleted) {
    (void)table;
    (void)search_val;
    (void)out_deleted;
    return microdb_rel_unavailable(db);
}

microdb_err_t microdb_rel_iter(microdb_t *db, microdb_table_t *table, microdb_rel_iter_cb_t cb, void *ctx) {
    (void)table;
    (void)cb;
    (void)ctx;
    return microdb_rel_unavailable(db);
}

microdb_err_t microdb_rel_count(const microdb_table_t *table, uint32_t *out_count) {
    (void)out_count;
    return microdb_rel_unavailable(table);
}

microdb_err_t microdb_rel_clear(microdb_t *db, microdb_table_t *table) {
    (void)table;
    return microdb_rel_unavailable(db);
}
#endif

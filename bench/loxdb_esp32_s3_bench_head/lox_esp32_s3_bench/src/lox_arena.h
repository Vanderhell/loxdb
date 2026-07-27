// SPDX-License-Identifier: MIT
#ifndef LOX_ARENA_H
#define LOX_ARENA_H

#include "lox_internal.h"

static inline void lox_arena_init(lox_arena_t *arena, uint8_t *base, size_t capacity) {
    arena->base = base;
    arena->capacity = capacity;
    arena->used = 0;
}

static inline void *lox_arena_alloc(lox_arena_t *arena, size_t size, size_t align) {
    size_t aligned = 0u;
    void *ptr;

    if (arena == NULL || arena->base == NULL || size == 0u) {
        return NULL;
    }
    if (!lox_checked_align_up_size(arena->used, align, &aligned)) {
        return NULL;
    }
    if (aligned > arena->capacity || size > (arena->capacity - aligned)) {
        return NULL;
    }

    ptr = arena->base + aligned;
    arena->used = aligned + size;
    return ptr;
}

static inline size_t lox_arena_remaining(const lox_arena_t *arena) {
    if (arena == NULL || arena->used >= arena->capacity) {
        return 0u;
    }
    return arena->capacity - arena->used;
}

static inline void lox_arena_rewind(lox_arena_t *arena, size_t used) {
    if (arena != NULL) {
        arena->used = used;
    }
}

#endif

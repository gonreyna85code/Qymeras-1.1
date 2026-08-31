/**
 * Qymera Dashboard - Ring Buffer Abstraction
 * Fixed-capacity, bounded ring buffer for embedded use
 */
#pragma once

#include "qymera_types.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================
 * Ring Buffer Type Definition (full definition in header)
 * ========================= */

typedef struct {
    void *data;
    size_t capacity;
    size_t element_size;
    size_t head;      // Next write position
    size_t count;     // Current number of elements
    bool overwrite;   // true = overwrite oldest, false = reject when full
    size_t pushes;
    size_t pops;
    size_t overwrites;
    size_t rejected;
} qymera_ring_t;

typedef struct {
    void *data;
    size_t capacity;
    size_t element_size;
    bool overwrite;
} qymera_ring_config_t;

/* =========================
 * Ring Buffer API
 * ========================= */

qymera_err_t qymera_ring_init(qymera_ring_t *ring, const qymera_ring_config_t *config);
qymera_err_t qymera_ring_push(qymera_ring_t *ring, const void *element);
qymera_err_t qymera_ring_pop(qymera_ring_t *ring, void *element);
qymera_err_t qymera_ring_peek(const qymera_ring_t *ring, void *element);
qymera_err_t qymera_ring_get_at(const qymera_ring_t *ring, size_t index, void *element);
size_t qymera_ring_count(const qymera_ring_t *ring);
size_t qymera_ring_capacity(const qymera_ring_t *ring);
bool qymera_ring_is_empty(const qymera_ring_t *ring);
bool qymera_ring_is_full(const qymera_ring_t *ring);
void qymera_ring_clear(qymera_ring_t *ring);

typedef bool (*qymera_ring_iter_cb_t)(const void *element, size_t index, void *context);
size_t qymera_ring_iterate(const qymera_ring_t *ring, qymera_ring_iter_cb_t callback, void *context);

typedef struct {
    size_t capacity;
    size_t count;
    size_t pushes;
    size_t pops;
    size_t overwrites;
    size_t rejected;
} qymera_ring_stats_t;

void qymera_ring_get_stats(const qymera_ring_t *ring, qymera_ring_stats_t *stats);

/* =========================
 * Convenience Macros
 * ========================= */

#define QYMERA_RING_DEFINE(name, type, capacity, overwrite) \
    static type name##_storage[capacity]; \
    static qymera_ring_t name; \
    static bool name##_initialized = false; \
    static inline qymera_err_t name##_init(void) { \
        if (name##_initialized) return QYMERA_OK; \
        qymera_ring_config_t cfg = { \
            .data = name##_storage, \
            .capacity = capacity, \
            .element_size = sizeof(type), \
            .overwrite = overwrite, \
        }; \
        qymera_err_t err = qymera_ring_init(&name, &cfg); \
        if (err == QYMERA_OK) name##_initialized = true; \
        return err; \
    } \
    static inline qymera_err_t name##_push(const type *elem) { \
        if (!name##_initialized) name##_init(); \
        return qymera_ring_push(&name, elem); \
    } \
    static inline qymera_err_t name##_pop(type *elem) { \
        if (!name##_initialized) return QYMERA_ERR_INVALID_STATE; \
        return qymera_ring_pop(&name, elem); \
    } \
    static inline size_t name##_count(void) { \
        return qymera_ring_count(&name); \
    }

#ifdef __cplusplus
}
#endif
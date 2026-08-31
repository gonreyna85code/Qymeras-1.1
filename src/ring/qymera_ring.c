/**
 * Qymera Dashboard - Ring Buffer Implementation
 */
#include "qymera_ring.h"
#include <string.h>
#include <stdlib.h>

struct qymera_ring_s {
    uint8_t *data;
    size_t capacity;
    size_t element_size;
    size_t head;
    size_t count;
    bool overwrite;
    size_t pushes;
    size_t pops;
    size_t overwrites;
    size_t rejected;
};

qymera_err_t qymera_ring_init(qymera_ring_t *ring, const qymera_ring_config_t *config) {
    if (!ring || !config) return QYMERA_ERR_INVALID_ARG;
    if (!config->data || config->capacity == 0 || config->element_size == 0) {
        return QYMERA_ERR_INVALID_ARG;
    }
    
    ring->data = (uint8_t *)config->data;
    ring->capacity = config->capacity;
    ring->element_size = config->element_size;
    ring->head = 0;
    ring->count = 0;
    ring->overwrite = config->overwrite;
    ring->pushes = 0;
    ring->pops = 0;
    ring->overwrites = 0;
    ring->rejected = 0;
    
    return QYMERA_OK;
}

qymera_err_t qymera_ring_push(qymera_ring_t *ring, const void *element) {
    if (!ring || !element) return QYMERA_ERR_INVALID_ARG;
    
    if (ring->count >= ring->capacity) {
        if (!ring->overwrite) {
            ring->rejected++;
            return QYMERA_ERR_NO_SPACE;
        }
        ring->overwrites++;
    } else {
        ring->count++;
    }
    
    size_t offset = ring->head * ring->element_size;
    memcpy(&ring->data[offset], element, ring->element_size);
    
    ring->head = (ring->head + 1) % ring->capacity;
    ring->pushes++;
    
    return QYMERA_OK;
}

qymera_err_t qymera_ring_pop(qymera_ring_t *ring, void *element) {
    if (!ring || !element) return QYMERA_ERR_INVALID_ARG;
    if (ring->count == 0) return QYMERA_ERR_NOT_FOUND;
    
    size_t tail = (ring->capacity + ring->head - ring->count) % ring->capacity;
    size_t offset = tail * ring->element_size;
    memcpy(element, &ring->data[offset], ring->element_size);
    
    ring->count--;
    ring->pops++;
    
    return QYMERA_OK;
}

qymera_err_t qymera_ring_peek(const qymera_ring_t *ring, void *element) {
    if (!ring || !element) return QYMERA_ERR_INVALID_ARG;
    if (ring->count == 0) return QYMERA_ERR_NOT_FOUND;
    
    size_t tail = (ring->capacity + ring->head - ring->count) % ring->capacity;
    size_t offset = tail * ring->element_size;
    memcpy(element, &ring->data[offset], ring->element_size);
    
    return QYMERA_OK;
}

qymera_err_t qymera_ring_get_at(const qymera_ring_t *ring, size_t index, void *element) {
    if (!ring || !element) return QYMERA_ERR_INVALID_ARG;
    if (index >= ring->count) return QYMERA_ERR_INVALID_ARG;
    
    size_t tail = (ring->capacity + ring->head - ring->count) % ring->capacity;
    size_t pos = (tail + index) % ring->capacity;
    size_t offset = pos * ring->element_size;
    memcpy(element, &ring->data[offset], ring->element_size);
    
    return QYMERA_OK;
}

size_t qymera_ring_count(const qymera_ring_t *ring) {
    return ring ? ring->count : 0;
}

size_t qymera_ring_capacity(const qymera_ring_t *ring) {
    return ring ? ring->capacity : 0;
}

bool qymera_ring_is_empty(const qymera_ring_t *ring) {
    return ring ? (ring->count == 0) : true;
}

bool qymera_ring_is_full(const qymera_ring_t *ring) {
    return ring ? (ring->count >= ring->capacity) : false;
}

void qymera_ring_clear(qymera_ring_t *ring) {
    if (!ring) return;
    ring->head = 0;
    ring->count = 0;
}

size_t qymera_ring_iterate(const qymera_ring_t *ring, qymera_ring_iter_cb_t callback, void *context) {
    if (!ring || !callback) return 0;
    
    size_t visited = 0;
    size_t tail = (ring->capacity + ring->head - ring->count) % ring->capacity;
    
    for (size_t i = 0; i < ring->count; i++) {
        size_t pos = (tail + i) % ring->capacity;
        size_t offset = pos * ring->element_size;
        
        if (!callback(&ring->data[offset], i, context)) {
            break;
        }
        visited++;
    }
    
    return visited;
}

void qymera_ring_get_stats(const qymera_ring_t *ring, qymera_ring_stats_t *stats) {
    if (!ring || !stats) return;
    
    stats->capacity = ring->capacity;
    stats->count = ring->count;
    stats->pushes = ring->pushes;
    stats->pops = ring->pops;
    stats->overwrites = ring->overwrites;
    stats->rejected = ring->rejected;
}
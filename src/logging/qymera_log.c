/**
 * Qymera Dashboard - Structured Logging Implementation
 */
#include "qymera_log.h"
#include "qymera_hal.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

struct qymera_log_s {
    qymera_ring_t log_ring;
    qymera_log_layer_t min_layer;
    bool layer_enabled[9];
    bool serial_output;
    uint32_t global_seq;
};

static const char *LAYER_NAMES[] = {
    "DEBUG", "INFO", "WARNING", "ERROR",
    "EVENT", "ACTION", "AUTOMATION", "AI", "SYSTEM"
};

qymera_err_t qymera_log_init(qymera_log_t **log, const qymera_log_config_t *config) {
    if (!log || !config) return QYMERA_ERR_INVALID_ARG;
    if (!config->log_ring.data) return QYMERA_ERR_INVALID_ARG;
    
    qymera_log_t *l = calloc(1, sizeof(qymera_log_t));
    if (!l) return QYMERA_ERR_NO_SPACE;
    
    l->log_ring = config->log_ring;
    l->min_layer = config->min_layer;
    l->serial_output = config->serial_output;
    
    for (int i = 0; i < 9; i++) {
        l->layer_enabled[i] = config->layer_enabled[i];
    }
    
    l->global_seq = 1;
    
    *log = l;
    return QYMERA_OK;
}

static bool should_log(const qymera_log_t *log, qymera_log_layer_t layer) {
    if (!log) return false;
    if (layer < log->min_layer) return false;
    if (layer >= 9) return false;
    return log->layer_enabled[layer];
}

static void format_message(char *buffer, size_t buf_len, const char *fmt, va_list args) {
    vsnprintf(buffer, buf_len, fmt, args);
    buffer[buf_len - 1] = '\0';
}

static void output_log(const qymera_log_t *log, const qymera_log_entry_t *entry) {
    if (!log->serial_output) return;
    
    char time_str[32];
    snprintf(time_str, sizeof(time_str), "%u.%03u", 
             (unsigned)entry->timestamp.seconds, (unsigned)entry->timestamp.millis);
    
    printf("[%u][%s][%s][%s] %s\n",
           (unsigned)entry->seq,
           time_str,
           LAYER_NAMES[entry->layer],
           entry->source,
           entry->message);
}

qymera_err_t qymera_log_logf(qymera_log_t *log, qymera_log_layer_t layer, const char *source, const char *fmt, ...) {
    if (!log || !source || !fmt) return QYMERA_ERR_INVALID_ARG;
    if (!should_log(log, layer)) return QYMERA_OK;
    
    qymera_log_entry_t entry = {0};
    entry.seq = log->global_seq++;
    entry.timestamp = qymera_timestamp_now();
    entry.layer = layer;
    strncpy(entry.source, source, sizeof(entry.source) - 1);
    
    va_list args;
    va_start(args, fmt);
    format_message(entry.message, sizeof(entry.message), fmt, args);
    va_end(args);
    
    qymera_log_entry_t last;
    if (qymera_ring_peek(&log->log_ring, &last) == QYMERA_OK) {
        if (last.layer == layer && 
            strcmp(last.source, source) == 0 &&
            strcmp(last.message, entry.message) == 0 &&
            last.repeat_count < 65535) {
            size_t tail = (log->log_ring.capacity + log->log_ring.head - log->log_ring.count) % log->log_ring.capacity;
            size_t offset = tail * log->log_ring.element_size;
            qymera_log_entry_t *ring_entry = (qymera_log_entry_t *)((uint8_t *)log->log_ring.data + offset);
            ring_entry->repeat_count++;
            ring_entry->timestamp = entry.timestamp;
            return QYMERA_OK;
        }
    }
    
    qymera_err_t err = qymera_ring_push(&log->log_ring, &entry);
    if (err == QYMERA_OK) {
        output_log(log, &entry);
    }
    return err;
}

qymera_err_t qymera_log_logf_ref(qymera_log_t *log, qymera_log_layer_t layer, const char *source,
                                  uint32_t ref_rule, uint16_t ref_entity, const char *fmt, ...) {
    if (!log || !source || !fmt) return QYMERA_ERR_INVALID_ARG;
    if (!should_log(log, layer)) return QYMERA_OK;
    
    qymera_log_entry_t entry = {0};
    entry.seq = log->global_seq++;
    entry.timestamp = qymera_timestamp_now();
    entry.layer = layer;
    strncpy(entry.source, source, sizeof(entry.source) - 1);
    entry.ref_rule_id = ref_rule;
    entry.ref_entity_idx = ref_entity;
    
    va_list args;
    va_start(args, fmt);
    format_message(entry.message, sizeof(entry.message), fmt, args);
    va_end(args);
    
    qymera_err_t err = qymera_ring_push(&log->log_ring, &entry);
    if (err == QYMERA_OK) {
        output_log(log, &entry);
    }
    return err;
}

qymera_err_t qymera_log_get_recent_json(qymera_log_t *log, char *buffer, size_t buf_len, size_t max_entries) {
    if (!log || !buffer || buf_len == 0) return QYMERA_ERR_INVALID_ARG;
    
    size_t count = qymera_ring_count(&log->log_ring);
    size_t to_show = (count < max_entries) ? count : max_entries;
    
    size_t pos = 0;
    pos += snprintf(buffer + pos, buf_len - pos, "[");
    
    for (size_t i = 0; i < to_show; i++) {
        qymera_log_entry_t entry;
        size_t idx = (count > to_show) ? (count - to_show + i) : i;
        if (qymera_ring_get_at(&log->log_ring, idx, &entry) != QYMERA_OK) continue;
        
        if (i > 0) pos += snprintf(buffer + pos, buf_len - pos, ",");
        
        pos += snprintf(buffer + pos, buf_len - pos,
            "{\"seq\":%u,\"ts\":%u.%03u,\"layer\":\"%s\",\"source\":\"%s\",\"msg\":\"%s\"",
            (unsigned)entry.seq,
            (unsigned)entry.timestamp.seconds, (unsigned)entry.timestamp.millis,
            LAYER_NAMES[entry.layer],
            entry.source,
            entry.message);
        
        if (entry.ref_rule_id != 0) {
            pos += snprintf(buffer + pos, buf_len - pos, ",\"rule\":%u", (unsigned)entry.ref_rule_id);
        }
        if (entry.ref_entity_idx != 0xFFFF) {
            pos += snprintf(buffer + pos, buf_len - pos, ",\"entity\":%u", (unsigned)entry.ref_entity_idx);
        }
        if (entry.repeat_count > 1) {
            pos += snprintf(buffer + pos, buf_len - pos, ",\"repeat\":%u", (unsigned)entry.repeat_count);
        }
        
        pos += snprintf(buffer + pos, buf_len - pos, "}");
        
        if (pos >= buf_len - 2) break;
    }
    
    snprintf(buffer + pos, buf_len - pos, "]");
    return QYMERA_OK;
}

void qymera_log_clear(qymera_log_t *log) {
    if (!log) return;
    qymera_ring_clear(&log->log_ring);
    log->global_seq = 1;
}

void qymera_log_set_min_layer(qymera_log_t *log, qymera_log_layer_t layer) {
    if (log && layer < 9) log->min_layer = layer;
}

void qymera_log_set_layer_enabled(qymera_log_t *log, qymera_log_layer_t layer, bool enabled) {
    if (log && layer < 9) log->layer_enabled[layer] = enabled;
}

void qymera_log_set_serial_output(qymera_log_t *log, bool enabled) {
    if (log) log->serial_output = enabled;
}

void qymera_log_get_stats(qymera_log_t *log, qymera_ring_stats_t *stats) {
    if (!log || !stats) return;
    qymera_ring_get_stats(&log->log_ring, stats);
}
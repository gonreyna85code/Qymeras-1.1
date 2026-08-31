/**
 * Qymera Dashboard - Structured Logging
 * Bounded ring buffer with layered log levels
 */
#pragma once

#include "qymera_types.h"
#include "qymera_ring.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct qymera_log_s qymera_log_t;

typedef struct {
    uint32_t seq;
    qymera_timestamp_t timestamp;
    qymera_log_layer_t layer;
    char source[32];
    char message[128];
    uint32_t ref_rule_id;
    uint16_t ref_entity_idx;
    uint16_t repeat_count;
} qymera_log_entry_t;

typedef struct {
    qymera_ring_t log_ring;
    qymera_log_layer_t min_layer;
    bool layer_enabled[9];
    bool serial_output;
} qymera_log_config_t;

qymera_err_t qymera_log_init(qymera_log_t **log, const qymera_log_config_t *config);
qymera_err_t qymera_log_logf(qymera_log_t *log, qymera_log_layer_t layer, const char *source, const char *fmt, ...);
qymera_err_t qymera_log_logf_ref(qymera_log_t *log, qymera_log_layer_t layer, const char *source,
                                  uint32_t ref_rule, uint16_t ref_entity, const char *fmt, ...);

#define qymera_log_debug(log, source, ...) \
    qymera_log_logf(log, QYMERA_LOG_DEBUG, source, __VA_ARGS__)

#define qymera_log_info(log, source, ...) \
    qymera_log_logf(log, QYMERA_LOG_INFO, source, __VA_ARGS__)

#define qymera_log_warn(log, source, ...) \
    qymera_log_logf(log, QYMERA_LOG_WARNING, source, __VA_ARGS__)

#define qymera_log_error(log, source, ...) \
    qymera_log_logf(log, QYMERA_LOG_ERROR, source, __VA_ARGS__)

#define qymera_log_event(log, source, ...) \
    qymera_log_logf(log, QYMERA_LOG_EVENT, source, __VA_ARGS__)

#define qymera_log_action(log, source, ...) \
    qymera_log_logf(log, QYMERA_LOG_ACTION, source, __VA_ARGS__)

#define qymera_log_automation(log, source, ...) \
    qymera_log_logf(log, QYMERA_LOG_AUTOMATION, source, __VA_ARGS__)

#define qymera_log_ai(log, source, ...) \
    qymera_log_logf(log, QYMERA_LOG_AI, source, __VA_ARGS__)

#define qymera_log_system(log, source, ...) \
    qymera_log_logf(log, QYMERA_LOG_SYSTEM, source, __VA_ARGS__)

qymera_err_t qymera_log_get_recent_json(qymera_log_t *log, char *buffer, size_t buf_len, size_t max_entries);
void qymera_log_clear(qymera_log_t *log);
void qymera_log_set_min_layer(qymera_log_t *log, qymera_log_layer_t layer);
void qymera_log_set_layer_enabled(qymera_log_t *log, qymera_log_layer_t layer, bool enabled);
void qymera_log_set_serial_output(qymera_log_t *log, bool enabled);
void qymera_log_get_stats(qymera_log_t *log, qymera_ring_stats_t *stats);

/**
 * Early logging before log system is initialized (uses printf directly)
 */
void qymera_log_early(const char *fmt, ...);

#ifdef __cplusplus
}
#endif
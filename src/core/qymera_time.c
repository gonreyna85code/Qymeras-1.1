/**
 * Qymera Dashboard - Timestamp Utilities
 */
#include "qymera_types.h"
#include "qymera_hal.h"

qymera_timestamp_t qymera_timestamp_now(void) {
    qymera_timestamp_t ts;
    uint32_t uptime_ms = qymera_system_get_uptime_ms();
    ts.seconds = uptime_ms / 1000;
    ts.millis = uptime_ms % 1000;
    return ts;
}

uint32_t qymera_timestamp_diff_ms(const qymera_timestamp_t *a, const qymera_timestamp_t *b) {
    if (!a || !b) return 0;
    uint32_t a_ms = (a->seconds * 1000u) + a->millis;
    uint32_t b_ms = (b->seconds * 1000u) + b->millis;
    return a_ms - b_ms;
}

bool qymera_timestamp_expired(const qymera_timestamp_t *ts, uint32_t timeout_ms) {
    if (!ts) return true;
    qymera_timestamp_t now = qymera_timestamp_now();
    return qymera_timestamp_diff_ms(&now, ts) >= timeout_ms;
}
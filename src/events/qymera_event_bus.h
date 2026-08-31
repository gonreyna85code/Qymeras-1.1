/**
 * Qymera Dashboard - Event Bus
 * Central event system with bounded queues and fan-out
 */
#pragma once

#include "qymera_types.h"
#include "qymera_ring.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================
 * Event Structure
 * ========================= */

typedef struct {
    uint32_t seq;                 // Monotonic sequence number
    qymera_timestamp_t timestamp; // Wall clock + monotonic
    qymera_event_type_t type;     // Event type
    uint8_t priority;             // 0=high, 1=normal, 2=low
    
    // Source identification
    char device_id[QYMERA_DEVICE_ID_LEN];
    char entity_id[QYMERA_ENTITY_ID_LEN];
    
    // Payload (union for common types)
    union {
        struct {
            qymera_entity_value_t value;
            qymera_entity_value_t prev_value;
        } sensor_changed;
        
        struct {
            qymera_entity_value_t value;
        } actuator_changed;
        
        struct {
            char rule_id[QYMERA_RULE_ID_LEN];
            uint32_t revision;
            bool fired;
        } automation_triggered;
        
        struct {
            char rule_id[QYMERA_RULE_ID_LEN];
            uint32_t revision;
            uint8_t action_count;
            bool success;
        } automation_completed;
        
        struct {
            int error_code;
            char message[64];
        } system_error;
        
        struct {
            char provider[32];
            char request_id[32];
        } inference_request;
        
        struct {
            char provider[32];
            char request_id[32];
            bool valid;
            float result_value;
        } inference_result;
        
        struct {
            char rule_id[QYMERA_RULE_ID_LEN];
            uint8_t lifecycle;  // 0=created, 1=updated, 2=deleted, 3=enabled, 4=disabled
        } rule_lifecycle;
        
        struct {
            uint32_t alarm_id;
        } schedule_alarm;
        
        uint8_t raw[64];  // Generic payload
    } payload;
} qymera_event_t;

/* =========================
 * Subscription
 * ========================= */

typedef struct {
    qymera_event_type_t event_types[8];  // Event types to subscribe to
    uint8_t type_count;
    char device_filter[QYMERA_DEVICE_ID_LEN];  // Empty = all devices
    char entity_filter[QYMERA_ENTITY_ID_LEN];  // Empty = all entities
    void (*callback)(const qymera_event_t *event, void *context);
    void *context;
    bool active;
} qymera_subscription_t;

#define QYMERA_MAX_SUBSCRIPTIONS 32

/* =========================
 * Event Bus Handle
 * ========================= */

typedef struct qymera_event_bus_s qymera_event_bus_t;

/* =========================
 * Event Bus Configuration
 * ========================= */

typedef struct {
    qymera_ring_t event_ring;
    qymera_subscription_t *subscriptions;
    size_t max_subscriptions;
} qymera_event_bus_config_t;

/* =========================
 * Event Bus API
 * ========================= */

/**
 * Initialize the event bus
 * @param bus     Output bus handle
 * @param config  Configuration with pre-allocated storage
 * @return QYMERA_OK on success
 */
qymera_err_t qymera_event_bus_init(qymera_event_bus_t **bus, const qymera_event_bus_config_t *config);

/**
 * Publish an event to the bus (fan-out to subscribers)
 * @param bus   Event bus
 * @param event Event to publish
 * @return QYMERA_OK on success, QYMERA_ERR_NO_SPACE if ring full
 */
qymera_err_t qymera_event_bus_publish(qymera_event_bus_t *bus, const qymera_event_t *event);

/**
 * Subscribe to events
 * @param bus          Event bus
 * @param subscription Subscription configuration
 * @param sub_idx      Output subscription index
 * @return QYMERA_OK on success
 */
qymera_err_t qymera_event_bus_subscribe(qymera_event_bus_t *bus, const qymera_subscription_t *subscription, uint8_t *sub_idx);

/**
 * Unsubscribe from events
 * @param bus     Event bus
 * @param sub_idx Subscription index
 * @return QYMERA_OK on success
 */
qymera_err_t qymera_event_bus_unsubscribe(qymera_event_bus_t *bus, uint8_t sub_idx);

/**
 * Process pending events (call from main loop)
 * Dispatches events to subscribers
 * @param bus Event bus
 * @return Number of events processed
 */
size_t qymera_event_bus_process(qymera_event_bus_t *bus);

/**
 * Create a sensor.changed event
 * @param event       Output event
 * @param device_id   Device ID
 * @param entity_id   Entity ID
 * @param value       New value
 * @param prev_value  Previous value
 */
void qymera_event_make_sensor_changed(qymera_event_t *event, const char *device_id, const char *entity_id,
                                       const qymera_entity_value_t *value, const qymera_entity_value_t *prev_value);

/**
 * Create an actuator.changed event
 * @param event       Output event
 * @param device_id   Device ID
 * @param entity_id   Entity ID
 * @param value       New value
 */
void qymera_event_make_actuator_changed(qymera_event_t *event, const char *device_id, const char *entity_id,
                                         const qymera_entity_value_t *value);

/**
 * Create an automation.triggered event
 * @param event      Output event
 * @param rule_id    Rule ID
 * @param revision   Rule revision
 * @param fired      Whether rule fired
 */
void qymera_event_make_automation_triggered(qymera_event_t *event, const char *rule_id, uint32_t revision, bool fired);

/**
 * Create a system.error event
 * @param event      Output event
 * @param error_code Error code
 * @param message    Error message
 */
void qymera_event_make_system_error(qymera_event_t *event, int error_code, const char *message);

/**
 * Get event ring statistics
 * @param bus   Event bus
 * @param stats Output statistics
 */
void qymera_event_bus_get_stats(qymera_event_bus_t *bus, qymera_ring_stats_t *stats);

/**
 * Get subscriptions array for shutdown cleanup
 * @param bus   Event bus
 * @return subscriptions array or NULL
 */
qymera_subscription_t *qymera_event_bus_get_subscriptions(qymera_event_bus_t *bus);

#ifdef __cplusplus
}
#endif
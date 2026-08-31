/**
 * Qymera Dashboard - Event Bus Implementation
 */
#include "qymera_event_bus.h"
#include <string.h>
#include <stdlib.h>

struct qymera_event_bus_s {
    qymera_ring_t event_ring;
    qymera_subscription_t *subscriptions;
    size_t max_subscriptions;
    size_t subscription_count;
    uint32_t global_seq;
};

static bool event_matches_subscription(const qymera_event_t *event, const qymera_subscription_t *sub) {
    if (!sub->active) return false;
    
    bool type_match = false;
    for (uint8_t i = 0; i < sub->type_count; i++) {
        if (sub->event_types[i] == event->type) {
            type_match = true;
            break;
        }
    }
    if (!type_match) return false;
    
    if (sub->device_filter[0] != '\0' && 
        strcmp(sub->device_filter, event->device_id) != 0) {
        return false;
    }
    
    if (sub->entity_filter[0] != '\0' && 
        strcmp(sub->entity_filter, event->entity_id) != 0) {
        return false;
    }
    
    return true;
}

qymera_err_t qymera_event_bus_init(qymera_event_bus_t **bus, const qymera_event_bus_config_t *config) {
    if (!bus || !config) return QYMERA_ERR_INVALID_ARG;
    if (!config->event_ring.data) return QYMERA_ERR_INVALID_ARG;
    if (!config->subscriptions || config->max_subscriptions == 0) return QYMERA_ERR_INVALID_ARG;
    
    qymera_event_bus_t *b = calloc(1, sizeof(qymera_event_bus_t));
    if (!b) return QYMERA_ERR_NO_SPACE;
    
    b->event_ring = config->event_ring;
    b->subscriptions = config->subscriptions;
    b->max_subscriptions = config->max_subscriptions;
    b->subscription_count = 0;
    b->global_seq = 1;
    
    *bus = b;
    return QYMERA_OK;
}

qymera_err_t qymera_event_bus_publish(qymera_event_bus_t *bus, const qymera_event_t *event) {
    if (!bus || !event) return QYMERA_ERR_INVALID_ARG;
    
    qymera_event_t ev = *event;
    ev.seq = bus->global_seq++;
    
    return qymera_ring_push(&bus->event_ring, &ev);
}

qymera_err_t qymera_event_bus_subscribe(qymera_event_bus_t *bus, const qymera_subscription_t *subscription, uint8_t *sub_idx) {
    if (!bus || !subscription || !sub_idx) return QYMERA_ERR_INVALID_ARG;
    if (bus->subscription_count >= bus->max_subscriptions) return QYMERA_ERR_NO_SPACE;
    if (subscription->callback == NULL) return QYMERA_ERR_INVALID_ARG;
    if (subscription->type_count == 0) return QYMERA_ERR_INVALID_ARG;
    
    uint8_t idx = bus->subscription_count;
    memcpy(&bus->subscriptions[idx], subscription, sizeof(qymera_subscription_t));
    bus->subscriptions[idx].active = true;
    bus->subscription_count++;
    *sub_idx = idx;
    
    return QYMERA_OK;
}

qymera_err_t qymera_event_bus_unsubscribe(qymera_event_bus_t *bus, uint8_t sub_idx) {
    if (!bus || sub_idx >= bus->max_subscriptions) return QYMERA_ERR_INVALID_ARG;
    if (!bus->subscriptions[sub_idx].active) return QYMERA_ERR_NOT_FOUND;
    
    bus->subscriptions[sub_idx].active = false;
    bus->subscriptions[sub_idx].callback = NULL;
    return QYMERA_OK;
}

size_t qymera_event_bus_process(qymera_event_bus_t *bus) {
    if (!bus) return 0;
    
    size_t processed = 0;
    qymera_event_t event;
    
    while (qymera_ring_pop(&bus->event_ring, &event) == QYMERA_OK) {
        for (size_t i = 0; i < bus->subscription_count; i++) {
            qymera_subscription_t *sub = &bus->subscriptions[i];
            if (event_matches_subscription(&event, sub)) {
                sub->callback(&event, sub->context);
            }
        }
        processed++;
    }
    
    return processed;
}

void qymera_event_make_sensor_changed(qymera_event_t *event, const char *device_id, const char *entity_id,
                                       const qymera_entity_value_t *value, const qymera_entity_value_t *prev_value) {
    if (!event) return;
    memset(event, 0, sizeof(qymera_event_t));
    
    event->type = QYMERA_EVENT_SENSOR_CHANGED;
    event->priority = 1;
    event->timestamp = qymera_timestamp_now();
    
    if (device_id) strncpy(event->device_id, device_id, QYMERA_DEVICE_ID_LEN - 1);
    if (entity_id) strncpy(event->entity_id, entity_id, QYMERA_ENTITY_ID_LEN - 1);
    
    if (value) event->payload.sensor_changed.value = *value;
    if (prev_value) event->payload.sensor_changed.prev_value = *prev_value;
}

void qymera_event_make_actuator_changed(qymera_event_t *event, const char *device_id, const char *entity_id,
                                         const qymera_entity_value_t *value) {
    if (!event) return;
    memset(event, 0, sizeof(qymera_event_t));
    
    event->type = QYMERA_EVENT_ACTUATOR_CHANGED;
    event->priority = 0;
    event->timestamp = qymera_timestamp_now();
    
    if (device_id) strncpy(event->device_id, device_id, QYMERA_DEVICE_ID_LEN - 1);
    if (entity_id) strncpy(event->entity_id, entity_id, QYMERA_ENTITY_ID_LEN - 1);
    
    if (value) event->payload.actuator_changed.value = *value;
}

void qymera_event_make_automation_triggered(qymera_event_t *event, const char *rule_id, uint32_t revision, bool fired) {
    if (!event) return;
    memset(event, 0, sizeof(qymera_event_t));
    
    event->type = QYMERA_EVENT_AUTOMATION_TRIGGERED;
    event->priority = 0;
    event->timestamp = qymera_timestamp_now();
    
    if (rule_id) strncpy(event->payload.automation_triggered.rule_id, rule_id, QYMERA_RULE_ID_LEN - 1);
    event->payload.automation_triggered.revision = revision;
    event->payload.automation_triggered.fired = fired;
}

void qymera_event_make_system_error(qymera_event_t *event, int error_code, const char *message) {
    if (!event) return;
    memset(event, 0, sizeof(qymera_event_t));
    
    event->type = QYMERA_EVENT_SYSTEM_ERROR;
    event->priority = 0;
    event->timestamp = qymera_timestamp_now();
    
    event->payload.system_error.error_code = error_code;
    if (message) strncpy(event->payload.system_error.message, message, sizeof(event->payload.system_error.message) - 1);
}

void qymera_event_make_rule_lifecycle(qymera_event_t *event, const char *rule_id, uint8_t lifecycle) {
    if (!event) return;
    memset(event, 0, sizeof(qymera_event_t));
    
    event->type = QYMERA_EVENT_RULE_LIFECYCLE;
    event->priority = 0;
    event->timestamp = qymera_timestamp_now();
    
    if (rule_id) strncpy(event->payload.rule_lifecycle.rule_id, rule_id, QYMERA_RULE_ID_LEN - 1);
    event->payload.rule_lifecycle.lifecycle = lifecycle;
}

void qymera_event_bus_get_stats(qymera_event_bus_t *bus, qymera_ring_stats_t *stats) {
    if (!bus || !stats) return;
    qymera_ring_get_stats(&bus->event_ring, stats);
}

qymera_subscription_t *qymera_event_bus_get_subscriptions(qymera_event_bus_t *bus) {
    if (!bus) return NULL;
    return bus->subscriptions;
}
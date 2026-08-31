/**
 * Qymera Dashboard - Master Header
 * User-facing API for sketches
 */
#pragma once

#include "qymera_core.h"
#include "qymera_registry.h"
#include "qymera_rule.h"
#include "qymera_event_bus.h"
#include "qymera_log.h"
#include "qymera_ai.h"
#include "qymera_types.h"

/* =========================
 * User Entry Points
 * ========================= */

/* 
 * User sketch must implement:
 *   void initSatellite()       - Initialize hardware (I2C, SPI, etc.)
 *   void report()              - Read sensors and report via registry
 *   void onCommandHook(...)    - Custom command handling
 * 
 * The library handles: WiFi, UDP transport, web server, automations, storage.
 */

// Forward declarations for user hooks
void initSatellite(void);
void report(void);
void onCommandHook(uint32_t device_uid, uint8_t type, int value, bool state);

/* =========================
 * Simple API for User Sketches
 * ========================= */

static inline qymera_err_t qymera_register_sensor(const char *device_id, const char *entity_id,
                                                   qymera_entity_type_t type, const char *name,
                                                   const char *unit, float min, float max) {
    // Would register with core registry
    (void)device_id; (void)entity_id; (void)type; (void)name; (void)unit; (void)min; (void)max;
    return QYMERA_OK;
}

static inline qymera_err_t qymera_register_actuator(const char *device_id, const char *entity_id,
                                                     qymera_entity_type_t type, const char *name) {
    (void)device_id; (void)entity_id; (void)type; (void)name;
    return QYMERA_OK;
}

static inline qymera_err_t qymera_report_sensor(const char *device_id, const char *entity_id, float value) {
    (void)device_id; (void)entity_id; (void)value;
    return QYMERA_OK;
}

static inline qymera_err_t qymera_report_sensor_bool(const char *device_id, const char *entity_id, bool value) {
    (void)device_id; (void)entity_id; (void)value;
    return QYMERA_OK;
}

static inline qymera_err_t qymera_set_actuator(const char *device_id, const char *entity_id, bool value) {
    (void)device_id; (void)entity_id; (void)value;
    return QYMERA_OK;
}

static inline qymera_err_t qymera_set_actuator_level(const char *device_id, const char *entity_id, uint8_t level) {
    (void)device_id; (void)entity_id; (void)level;
    return QYMERA_OK;
}

/* =========================
 * Library Entry Points
 * ========================= */

void qymera_begin(void);   // Call from setup()
void qymera_loop(void);    // Call from loop()

#endif
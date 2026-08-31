/**
 * Qymera Dashboard - Core Types
 * Shared fundamental types and constants
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* =========================
 * Platform Configuration
 * ========================= */

#define QYMERA_MAX_DEVICES        256
#define QYMERA_MAX_ENTITIES       1024
#define QYMERA_MAX_RULES          500
#define QYMERA_MAX_EVENT_QUEUE    256
#define QYMERA_MAX_LOG_ENTRIES    256
#define QYMERA_MAX_UDP_PACKET     1280

#define QYMERA_DEVICE_ID_LEN      32
#define QYMERA_ENTITY_ID_LEN      32
#define QYMERA_RULE_ID_LEN        64

#define QYMERA_UDP_PORT_DISCOVERY 13345
#define QYMERA_UDP_PORT_CONTROL   13346

/* =========================
 * Result Codes
 * ========================= */

typedef enum {
    QYMERA_OK = 0,
    QYMERA_ERR_INVALID_ARG = -1,
    QYMERA_ERR_NOT_FOUND = -2,
    QYMERA_ERR_NO_SPACE = -3,
    QYMERA_ERR_BUSY = -4,
    QYMERA_ERR_TIMEOUT = -5,
    QYMERA_ERR_INVALID_STATE = -6,
    QYMERA_ERR_PROTOCOL = -7,
    QYMERA_ERR_STORAGE = -8,
    QYMERA_ERR_NETWORK = -9,
} qymera_err_t;

/* =========================
 * Capability Types
 * ========================= */

typedef enum {
    QYMERA_CAP_NONE = 0,
    QYMERA_CAP_SENSOR_NUMERIC,
    QYMERA_CAP_SENSOR_DIGITAL,
    QYMERA_CAP_ACTUATOR_RELAY,
    QYMERA_CAP_ACTUATOR_DIMMER,
    QYMERA_CAP_ACTUATOR_GENERIC,
    QYMERA_CAP_INFERENCE_RESULT,
    QYMERA_CAP_TIME_SOURCE,
} qymera_capability_t;

/* =========================
 * Entity Types
 * ========================= */

typedef enum {
    QYMERA_ENTITY_NONE = 0,
    QYMERA_ENTITY_SENSOR_TEMPERATURE,
    QYMERA_ENTITY_SENSOR_HUMIDITY,
    QYMERA_ENTITY_SENSOR_LUMINOSITY,
    QYMERA_ENTITY_SENSOR_PRESSURE,
    QYMERA_ENTITY_SENSOR_LEVEL,
    QYMERA_ENTITY_SENSOR_AIRQ,
    QYMERA_ENTITY_SENSOR_RAIN,
    QYMERA_ENTITY_SENSOR_CONTACT,
    QYMERA_ENTITY_SENSOR_GENERIC,
    QYMERA_ENTITY_ACTUATOR_RELAY,
    QYMERA_ENTITY_ACTUATOR_DIMMER,
    QYMERA_ENTITY_VIRTUAL_DIGITAL_AI,
    QYMERA_ENTITY_VIRTUAL_ANALOG_AI,
    QYMERA_ENTITY_INFERENCE_RESULT,
    QYMERA_ENTITY_TIME,
} qymera_entity_type_t;

/* =========================
 * Log Levels / Layers
 * ========================= */

typedef enum {
    QYMERA_LOG_DEBUG = 0,
    QYMERA_LOG_INFO,
    QYMERA_LOG_WARNING,
    QYMERA_LOG_ERROR,
    QYMERA_LOG_EVENT,
    QYMERA_LOG_ACTION,
    QYMERA_LOG_AUTOMATION,
    QYMERA_LOG_AI,
    QYMERA_LOG_SYSTEM,
} qymera_log_layer_t;

/* =========================
 * Event Types
 * ========================= */

typedef enum {
    QYMERA_EVENT_NONE = 0,
    QYMERA_EVENT_SENSOR_CHANGED,
    QYMERA_EVENT_DEVICE_ONLINE,
    QYMERA_EVENT_DEVICE_OFFLINE,
    QYMERA_EVENT_ACTUATOR_CHANGED,
    QYMERA_EVENT_AUTOMATION_TRIGGERED,
    QYMERA_EVENT_AUTOMATION_COMPLETED,
    QYMERA_EVENT_SYSTEM_ERROR,
    QYMERA_EVENT_INFERENCE_REQUEST,
    QYMERA_EVENT_INFERENCE_RESULT,
    QYMERA_EVENT_RULE_LIFECYCLE,
    QYMERA_EVENT_SCHEDULE_ALARM,
} qymera_event_type_t;

/* =========================
 * Rule Types
 * ========================= */

typedef enum {
    QYMERA_RULE_NONE = 0,
    QYMERA_RULE_THRESHOLD,
    QYMERA_RULE_EDGE,
    QYMERA_RULE_TIME_WINDOW,
    QYMERA_RULE_INTERVAL,
    QYMERA_RULE_DEVICE_STATE,
    QYMERA_RULE_INFERENCE,
    QYMERA_RULE_HISTORICAL,
} qymera_rule_type_t;

/* =========================
 * Operators
 * ========================= */

typedef enum {
    QYMERA_OP_NONE = 0,
    QYMERA_OP_GT,
    QYMERA_OP_LT,
    QYMERA_OP_GE,
    QYMERA_OP_LE,
    QYMERA_OP_EQ,
    QYMERA_OP_NE,
    QYMERA_OP_IN_RANGE,
    QYMERA_OP_OUT_RANGE,
} qymera_operator_t;

/* =========================
 * Action Types
 * ========================= */

typedef enum {
    QYMERA_ACTION_NONE = 0,
    QYMERA_ACTION_SET_BOOL,
    QYMERA_ACTION_SET_LEVEL,
    QYMERA_ACTION_SET_VALUE,
    QYMERA_ACTION_TOGGLE,
    QYMERA_ACTION_PULSE,
    QYMERA_ACTION_FADE,
} qymera_action_type_t;

/* =========================
 * Common Structures
 * ========================= */

typedef struct {
    uint32_t seconds;
    uint32_t millis;
} qymera_timestamp_t;

typedef struct {
    char device_id[QYMERA_DEVICE_ID_LEN];
    char entity_id[QYMERA_ENTITY_ID_LEN];
} qymera_entity_ref_t;

typedef struct {
    bool valid;
    float numeric_value;
    bool bool_value;
    qymera_timestamp_t timestamp;
    uint8_t reliability;  // 0=live, 1=stale, 2=offline
} qymera_entity_value_t;

/* =========================
 * Utility Macros
 * ========================= */

#define QYMERA_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define QYMERA_MIN(a, b) ((a) < (b) ? (a) : (b))
#define QYMERA_MAX(a, b) ((a) > (b) ? (a) : (b))

/* Compile-time assertions */
#define QYMERA_STATIC_ASSERT(cond, msg) typedef char static_assert_##msg[(cond) ? 1 : -1]

QYMERA_STATIC_ASSERT(QYMERA_MAX_DEVICES <= 65535, max_devices_fits_u16);
QYMERA_STATIC_ASSERT(QYMERA_MAX_ENTITIES <= 65535, max_entities_fits_u16);
QYMERA_STATIC_ASSERT(QYMERA_MAX_RULES <= 65535, max_rules_fits_u16);

#ifdef __cplusplus
extern "C" {
#endif

/* Timestamp utilities */
qymera_timestamp_t qymera_timestamp_now(void);
uint32_t qymera_timestamp_diff_ms(const qymera_timestamp_t *a, const qymera_timestamp_t *b);
bool qymera_timestamp_expired(const qymera_timestamp_t *ts, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
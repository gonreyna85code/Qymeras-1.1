/**
 * Qymera Dashboard - Rule Engine
 * Core rule representation and deterministic evaluation
 */
#pragma once

#include "qymera_types.h"
#include "qymera_event_bus.h"
#include "qymera_log.h"
#include "qymera_registry.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct qymera_log_s;
struct qymera_registry_s;

/* =========================
 * Timer Wheel for scheduling
 * ========================= */

#define QYMERA_TIMER_WHEEL_SLOTS 256
#define QYMERA_TIMER_WHEEL_RESOLUTION_MS 100

typedef struct {
    uint32_t expires_at;
    uint16_t rule_slot;
    uint8_t timer_type;  // 0=cooldown, 1=interval, 2=sustained_window, 3=delay
    bool active;
} qymera_timer_entry_t;

typedef struct {
    qymera_timer_entry_t slots[QYMERA_TIMER_WHEEL_SLOTS];
    uint32_t current_tick;
} qymera_timer_wheel_t;

/* =========================
 * Rule Condition
 * ========================= */

typedef struct {
    qymera_entity_ref_t entity;
    qymera_operator_t operator_;
    float threshold;
    float threshold_high;
    uint32_t duration_ms;
    uint32_t window_ms;
    uint8_t condition_type;
    bool negate;
} qymera_condition_t;

#define QYMERA_MAX_CONDITIONS 8

/* =========================
 * Rule Action
 * ========================= */

typedef struct {
    qymera_entity_ref_t entity;
    qymera_action_type_t action;
    float value_f;
    uint32_t value_u32;
    uint32_t duration_ms;
    uint8_t action_type;
} qymera_action_t;

#define QYMERA_MAX_ACTIONS 8

/* =========================
 * Rule Definition (authoring form)
 * ========================= */

typedef struct {
    char rule_id[QYMERA_RULE_ID_LEN];
    char name[64];
    bool enabled;
    uint32_t revision;
    uint32_t created_ts;
    uint32_t updated_ts;
    
    qymera_condition_t trigger;
    
    qymera_condition_t conditions[QYMERA_MAX_CONDITIONS];
    uint8_t condition_count;
    
    qymera_action_t actions[QYMERA_MAX_ACTIONS];
    uint8_t action_count;
    
    uint32_t cooldown_ms;
    uint32_t max_activations_per_hour;
    uint8_t priority;
} qymera_rule_t;

/* =========================
 * Rule Runtime State
 * ========================= */

typedef struct {
    uint32_t last_triggered;
    uint32_t last_action;
    uint32_t activation_count;
    uint32_t window_start;
    uint32_t sustained_start;
    bool trigger_armed;
    bool in_cooldown;
    uint32_t hourly_activations;
    uint32_t hourly_window_start;
    
    // Timer wheel indices for this rule
    int16_t cooldown_timer_idx;
    int16_t interval_timer_idx;
    int16_t sustained_timer_idx;
    int16_t delay_timer_idx;

    // Feedback-loop guard: entity this rule last acted on, and when.
    // Prevents the actuator change caused by this rule's own action from
    // immediately re-triggering the same rule through the event bus.
    char feedback_entity[QYMERA_ENTITY_ID_LEN];
    uint32_t feedback_entity_ms;
} qymera_rule_state_t;

/* =========================
 * Compiled Rule (execution form)
 * ========================= */

typedef struct {
    qymera_rule_t rule;
    qymera_rule_state_t state;
    bool compiled;
    uint32_t checksum;
} qymera_compiled_rule_t;

/* =========================
 * Event-driven subscription index
 * ========================= */

#define QYMERA_MAX_SUBSCRIPTIONS_PER_ENTITY 16

typedef struct {
    uint16_t rule_slots[QYMERA_MAX_SUBSCRIPTIONS_PER_ENTITY];
    uint8_t count;
} qymera_entity_subscribers_t;

/* =========================
 * Rule Engine Handle
 * ========================= */

typedef struct qymera_rule_engine_s qymera_rule_engine_t;

/* =========================
 * Rule Engine Configuration
 * ========================= */

typedef struct {
    qymera_compiled_rule_t *rules;
    size_t max_rules;
    qymera_event_bus_t *event_bus;
    struct qymera_log_s *log;
    struct qymera_registry_s *registry;
} qymera_rule_engine_config_t;

/* =========================
 * Validation Result
 * ========================= */

typedef struct {
    bool valid;
    char errors[10][128];
    uint8_t error_count;
    char warnings[5][128];
    uint8_t warning_count;
} qymera_validation_result_t;

/* =========================
 * Rule Engine API
 * ========================= */

qymera_err_t qymera_rule_engine_init(qymera_rule_engine_t **engine, const qymera_rule_engine_config_t *config);
qymera_err_t qymera_rule_engine_validate(qymera_rule_engine_t *engine, const qymera_rule_t *rule, qymera_validation_result_t *result);
qymera_err_t qymera_rule_engine_compile(qymera_rule_engine_t *engine, const qymera_rule_t *rule, qymera_compiled_rule_t *compiled);
qymera_err_t qymera_rule_engine_load(qymera_rule_engine_t *engine, const qymera_compiled_rule_t *compiled, uint16_t *slot_idx);
qymera_err_t qymera_rule_engine_unload(qymera_rule_engine_t *engine, uint16_t slot_idx);
qymera_err_t qymera_rule_engine_set_enabled(qymera_rule_engine_t *engine, uint16_t slot_idx, bool enabled);
size_t qymera_rule_engine_evaluate(qymera_rule_engine_t *engine, const qymera_event_t *event);
size_t qymera_rule_engine_tick(qymera_rule_engine_t *engine, uint32_t now_ms);
qymera_err_t qymera_rule_engine_get(qymera_rule_engine_t *engine, uint16_t slot_idx, qymera_compiled_rule_t *compiled);

typedef bool (*qymera_rule_list_cb_t)(uint16_t slot_idx, const qymera_compiled_rule_t *rule, void *context);
size_t qymera_rule_engine_list(qymera_rule_engine_t *engine, qymera_rule_list_cb_t callback, void *context);

qymera_err_t qymera_rule_engine_execute_actions(qymera_rule_engine_t *engine, qymera_compiled_rule_t *compiled, const qymera_event_t *event);
qymera_err_t qymera_rule_engine_dry_run(qymera_rule_engine_t *engine, const qymera_rule_t *rule, const qymera_event_t *event, bool *fired);

/* Subscription management */
qymera_err_t qymera_rule_engine_subscribe_entity(qymera_rule_engine_t *engine, const qymera_entity_ref_t *entity_ref, uint16_t rule_slot);
qymera_err_t qymera_rule_engine_unsubscribe_entity(qymera_rule_engine_t *engine, const qymera_entity_ref_t *entity_ref, uint16_t rule_slot);

/* Timer wheel API */
void qymera_timer_wheel_init(qymera_timer_wheel_t *wheel);
int16_t qymera_timer_wheel_add(qymera_timer_wheel_t *wheel, uint32_t expires_at, uint16_t rule_slot, uint8_t timer_type);
void qymera_timer_wheel_remove(qymera_timer_wheel_t *wheel, int16_t timer_idx);
void qymera_timer_wheel_tick(qymera_timer_wheel_t *wheel, qymera_rule_engine_t *engine, uint32_t now_ms);

#ifdef __cplusplus
}
#endif
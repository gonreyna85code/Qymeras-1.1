/**
 * Qymera Dashboard - Rule Engine Implementation
 * Deterministic rule evaluation with event-driven architecture
 */
#include "qymera_rule.h"
#include "qymera_log.h"
#include "qymera_hal.h"
#include "qymera_registry.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

struct qymera_rule_engine_s {
    qymera_compiled_rule_t *rules;
    size_t max_rules;
    size_t loaded_count;
    qymera_event_bus_t *event_bus;
    qymera_log_t *log;
    qymera_registry_t *registry;
    
    // Timer wheel for cooldowns, intervals, sustained windows, delays
    qymera_timer_wheel_t timer_wheel;
    
    // Event-driven subscription index: entity -> list of rule slots
    qymera_entity_subscribers_t *entity_subscribers;
    size_t max_entities;
    
    // Subscription indices for event types
    uint8_t sensor_changed_sub;
    uint8_t device_state_sub;
    uint8_t schedule_sub;
};

static bool evaluate_condition(const qymera_condition_t *cond, const qymera_entity_value_t *value) {
    if (!cond || !value || !value->valid) return false;
    
    float v = value->numeric_value;
    bool result = false;
    
    switch (cond->operator_) {
        case QYMERA_OP_GT:
            result = (v > cond->threshold);
            break;
        case QYMERA_OP_LT:
            result = (v < cond->threshold);
            break;
        case QYMERA_OP_GE:
            result = (v >= cond->threshold);
            break;
        case QYMERA_OP_LE:
            result = (v <= cond->threshold);
            break;
        case QYMERA_OP_EQ:
            result = (v == cond->threshold);
            break;
        case QYMERA_OP_NE:
            result = (v != cond->threshold);
            break;
        case QYMERA_OP_IN_RANGE:
            result = (v >= cond->threshold && v <= cond->threshold_high);
            break;
        case QYMERA_OP_OUT_RANGE:
            result = (v < cond->threshold || v > cond->threshold_high);
            break;
        default:
            return false;
    }
    
    return cond->negate ? !result : result;
}

static bool evaluate_sustained(const qymera_condition_t *cond, qymera_rule_state_t *state, uint32_t now_ms) {
    if (cond->duration_ms == 0) return true;  // Instant condition
    
    if (state->sustained_start == 0) {
        state->sustained_start = now_ms;
        return false;
    }
    
    if (now_ms - state->sustained_start >= cond->duration_ms) {
        state->sustained_start = 0;
        return true;
    }
    
    return false;
}

static qymera_err_t fetch_entity_value(qymera_rule_engine_t *engine, const qymera_entity_ref_t *entity_ref, qymera_entity_value_t *out_value) {
    if (!engine || !engine->registry || !entity_ref || !out_value) return QYMERA_ERR_INVALID_ARG;
    
    uint16_t entity_idx;
    qymera_err_t err = qymera_registry_find_entity_by_ref(engine->registry, entity_ref, &entity_idx);
    if (err != QYMERA_OK) return err;
    
    qymera_entity_t entity;
    err = qymera_registry_get_entity(engine->registry, entity_idx, &entity);
    if (err != QYMERA_OK) return err;
    
    // For now, we return a basic value - in a real implementation,
    // the registry would store the current value
    out_value->valid = true;
    out_value->numeric_value = 0.0f;
    out_value->bool_value = false;
    out_value->timestamp = qymera_timestamp_now();
    out_value->reliability = 0;
    
    return QYMERA_OK;
}

static bool evaluate_all_conditions(qymera_rule_engine_t *engine, const qymera_rule_t *rule, qymera_rule_state_t *state, uint32_t now_ms) {
    // Evaluate all conditions (AND logic)
    for (uint8_t c = 0; c < rule->condition_count; c++) {
        const qymera_condition_t *cond = &rule->conditions[c];
        
        qymera_entity_value_t value;
        qymera_err_t err = fetch_entity_value(engine, &cond->entity, &value);
        if (err != QYMERA_OK) return false;
        
        if (!evaluate_condition(cond, &value)) {
            return false;
        }
    }
    
    return true;
}

qymera_err_t qymera_rule_engine_init(qymera_rule_engine_t **engine, const qymera_rule_engine_config_t *config) {
    if (!engine || !config) return QYMERA_ERR_INVALID_ARG;
    if (!config->rules || config->max_rules == 0) return QYMERA_ERR_INVALID_ARG;
    if (!config->event_bus) return QYMERA_ERR_INVALID_ARG;
    if (!config->registry) return QYMERA_ERR_INVALID_ARG;
    
    qymera_rule_engine_t *e = calloc(1, sizeof(qymera_rule_engine_t));
    if (!e) return QYMERA_ERR_NO_SPACE;
    
    e->rules = config->rules;
    e->max_rules = config->max_rules;
    e->loaded_count = 0;
    e->event_bus = config->event_bus;
    e->log = config->log;
    e->registry = config->registry;
    
    // Initialize timer wheel
    qymera_timer_wheel_init(&e->timer_wheel);
    
    // Initialize entity subscription index
    e->max_entities = QYMERA_MAX_ENTITIES;
    e->entity_subscribers = calloc(e->max_entities, sizeof(qymera_entity_subscribers_t));
    if (!e->entity_subscribers) {
        free(e);
        return QYMERA_ERR_NO_SPACE;
    }
    
    // Subscribe to relevant events
    qymera_subscription_t sub = {0};
    sub.event_types[0] = QYMERA_EVENT_SENSOR_CHANGED;
    sub.type_count = 1;
    sub.callback = NULL;  // Would set to internal handler
    sub.context = e;
    qymera_event_bus_subscribe(e->event_bus, &sub, &e->sensor_changed_sub);
    
    sub.type_count = 1;
    sub.event_types[0] = QYMERA_EVENT_DEVICE_ONLINE;
    qymera_event_bus_subscribe(e->event_bus, &sub, &e->device_state_sub);
    
    sub.event_types[0] = QYMERA_EVENT_SCHEDULE_ALARM;
    qymera_event_bus_subscribe(e->event_bus, &sub, &e->schedule_sub);
    
    *engine = e;
    return QYMERA_OK;
}

qymera_err_t qymera_rule_engine_validate(qymera_rule_engine_t *engine, const qymera_rule_t *rule, qymera_validation_result_t *result) {
    if (!engine || !rule || !result) return QYMERA_ERR_INVALID_ARG;
    
    memset(result, 0, sizeof(qymera_validation_result_t));
    result->valid = true;
    
    // Basic validation
    if (rule->rule_id[0] == '\0') {
        result->valid = false;
        snprintf(result->errors[result->error_count++], sizeof(result->errors[0]), "rule_id is required");
    }
    
    if (rule->name[0] == '\0') {
        result->valid = false;
        snprintf(result->errors[result->error_count++], sizeof(result->errors[0]), "name is required");
    }
    
    if (rule->condition_count == 0 && rule->trigger.operator_ == QYMERA_OP_NONE) {
        result->valid = false;
        snprintf(result->errors[result->error_count++], sizeof(result->errors[0]), "at least one condition or trigger required");
    }
    
    if (rule->action_count == 0) {
        result->valid = false;
        snprintf(result->errors[result->error_count++], sizeof(result->errors[0]), "at least one action required");
    }
    
    if (rule->condition_count > QYMERA_MAX_CONDITIONS) {
        result->valid = false;
        snprintf(result->errors[result->error_count++], sizeof(result->errors[0]), "too many conditions (max %d)", QYMERA_MAX_CONDITIONS);
    }
    
    if (rule->action_count > QYMERA_MAX_ACTIONS) {
        result->valid = false;
        snprintf(result->errors[result->error_count++], sizeof(result->errors[0]), "too many actions (max %d)", QYMERA_MAX_ACTIONS);
    }
    
    // Validate each condition
    for (uint8_t i = 0; i < rule->condition_count; i++) {
        const qymera_condition_t *c = &rule->conditions[i];
        if (c->entity.device_id[0] == '\0' || c->entity.entity_id[0] == '\0') {
            result->valid = false;
            snprintf(result->errors[result->error_count++], sizeof(result->errors[0]), "condition %d: entity reference required", i);
        }
        if (c->operator_ == QYMERA_OP_NONE) {
            result->valid = false;
            snprintf(result->errors[result->error_count++], sizeof(result->errors[0]), "condition %d: operator required", i);
        }
    }
    
    // Validate trigger
    if (rule->trigger.operator_ != QYMERA_OP_NONE) {
        if (rule->trigger.entity.device_id[0] == '\0' || rule->trigger.entity.entity_id[0] == '\0') {
            result->valid = false;
            snprintf(result->errors[result->error_count++], sizeof(result->errors[0]), "trigger: entity reference required");
        }
    }
    
    // Validate actions
    for (uint8_t i = 0; i < rule->action_count; i++) {
        const qymera_action_t *a = &rule->actions[i];
        if (a->entity.device_id[0] == '\0' || a->entity.entity_id[0] == '\0') {
            result->valid = false;
            snprintf(result->errors[result->error_count++], sizeof(result->errors[0]), "action %d: entity reference required", i);
        }
        if (a->action == QYMERA_ACTION_NONE) {
            result->valid = false;
            snprintf(result->errors[result->error_count++], sizeof(result->errors[0]), "action %d: action type required", i);
        }
    }
    
    return QYMERA_OK;
}

qymera_err_t qymera_rule_engine_compile(qymera_rule_engine_t *engine, const qymera_rule_t *rule, qymera_compiled_rule_t *compiled) {
    if (!engine || !rule || !compiled) return QYMERA_ERR_INVALID_ARG;
    
    // In a full implementation, this would compile to bytecode/IR
    // For now, just copy the rule and mark as compiled
    memcpy(&compiled->rule, rule, sizeof(qymera_rule_t));
    memset(&compiled->state, 0, sizeof(qymera_rule_state_t));
    compiled->compiled = true;
    compiled->checksum = 0;  // Would compute actual checksum
    
    return QYMERA_OK;
}

qymera_err_t qymera_rule_engine_load(qymera_rule_engine_t *engine, const qymera_compiled_rule_t *compiled, uint16_t *slot_idx) {
    if (!engine || !compiled || !slot_idx) return QYMERA_ERR_INVALID_ARG;
    if (engine->loaded_count >= engine->max_rules) return QYMERA_ERR_NO_SPACE;
    
    // Find free slot
    uint16_t idx = engine->loaded_count;
    for (size_t i = 0; i < engine->max_rules; i++) {
        if (!engine->rules[i].compiled) {
            idx = (uint16_t)i;
            break;
        }
    }
    
    engine->rules[idx] = *compiled;
    engine->loaded_count++;
    *slot_idx = idx;
    
    // Subscribe to trigger entity
    qymera_rule_engine_subscribe_entity(engine, &compiled->rule.trigger.entity, idx);
    
    // Subscribe to condition entities
    for (uint8_t c = 0; c < compiled->rule.condition_count; c++) {
        qymera_rule_engine_subscribe_entity(engine, &compiled->rule.conditions[c].entity, idx);
    }
    
    // Set up timer for interval trigger
    if (compiled->rule.trigger.condition_type == 3) {  // INTERVAL
        uint32_t interval = compiled->rule.trigger.duration_ms;
        if (interval > 0) {
            uint32_t now_ms = qymera_system_get_uptime_ms();
            uint32_t expires_at = now_ms + interval;
            int16_t timer_idx = qymera_timer_wheel_add(&engine->timer_wheel, expires_at, idx, 1);
            engine->rules[idx].state.interval_timer_idx = timer_idx;
        }
    }
    
    if (engine->log) {
        qymera_log_automation(engine->log, "rule_engine", "Loaded rule %s at slot %d", compiled->rule.rule_id, idx);
    }
    
    return QYMERA_OK;
}

qymera_err_t qymera_rule_engine_unload(qymera_rule_engine_t *engine, uint16_t slot_idx) {
    if (!engine || slot_idx >= engine->max_rules) return QYMERA_ERR_INVALID_ARG;
    if (!engine->rules[slot_idx].compiled) return QYMERA_ERR_NOT_FOUND;
    
    // Unsubscribe from entities
    qymera_compiled_rule_t *cr = &engine->rules[slot_idx];
    qymera_rule_engine_unsubscribe_entity(engine, &cr->rule.trigger.entity, slot_idx);
    for (uint8_t c = 0; c < cr->rule.condition_count; c++) {
        qymera_rule_engine_unsubscribe_entity(engine, &cr->rule.conditions[c].entity, slot_idx);
    }
    
    // Remove timers
    qymera_timer_wheel_remove(&engine->timer_wheel, cr->state.cooldown_timer_idx);
    qymera_timer_wheel_remove(&engine->timer_wheel, cr->state.interval_timer_idx);
    qymera_timer_wheel_remove(&engine->timer_wheel, cr->state.sustained_timer_idx);
    qymera_timer_wheel_remove(&engine->timer_wheel, cr->state.delay_timer_idx);
    
    memset(&engine->rules[slot_idx], 0, sizeof(qymera_compiled_rule_t));
    engine->loaded_count--;
    return QYMERA_OK;
}

qymera_err_t qymera_rule_engine_set_enabled(qymera_rule_engine_t *engine, uint16_t slot_idx, bool enabled) {
    if (!engine || slot_idx >= engine->max_rules) return QYMERA_ERR_INVALID_ARG;
    if (!engine->rules[slot_idx].compiled) return QYMERA_ERR_NOT_FOUND;
    
    engine->rules[slot_idx].rule.enabled = enabled;
    return QYMERA_OK;
}

size_t qymera_rule_engine_evaluate(qymera_rule_engine_t *engine, const qymera_event_t *event) {
    if (!engine || !event) return 0;
    
    size_t evaluated = 0;
    uint32_t now_ms = qymera_system_get_uptime_ms();
    
    // Process timers first
    qymera_timer_wheel_tick(&engine->timer_wheel, engine, now_ms);
    
    // Find rules subscribed to this entity
    if (event->type == QYMERA_EVENT_SENSOR_CHANGED || event->type == QYMERA_EVENT_DEVICE_ONLINE || event->type == QYMERA_EVENT_DEVICE_OFFLINE) {
        // Find entity index to get subscribers
        qymera_entity_ref_t ref = {0};
        strncpy(ref.device_id, event->device_id, QYMERA_DEVICE_ID_LEN - 1);
        strncpy(ref.entity_id, event->entity_id, QYMERA_ENTITY_ID_LEN - 1);
        
        uint16_t entity_idx;
        if (qymera_registry_find_entity_by_ref(engine->registry, &ref, &entity_idx) == QYMERA_OK) {
            if (entity_idx < engine->max_entities) {
                qymera_entity_subscribers_t *subs = &engine->entity_subscribers[entity_idx];
                for (uint8_t i = 0; i < subs->count; i++) {
                    uint16_t slot_idx = subs->rule_slots[i];
                    qymera_compiled_rule_t *cr = &engine->rules[slot_idx];
                    if (!cr->compiled || !cr->rule.enabled) continue;
                    
                    uint32_t now_ms = qymera_system_get_uptime_ms();
                    
                    // Check cooldown
                    if (cr->state.in_cooldown) {
                        if (now_ms - cr->state.last_action >= cr->rule.cooldown_ms) {
                            cr->state.in_cooldown = false;
                        } else {
                            continue;
                        }
                    }
                    
                    // Check hourly rate limit
                    if (cr->rule.max_activations_per_hour > 0) {
                        if (now_ms - cr->state.hourly_window_start >= 3600000) {
                            cr->state.hourly_activations = 0;
                            cr->state.hourly_window_start = now_ms;
                        }
                        if (cr->state.hourly_activations >= cr->rule.max_activations_per_hour) {
                            continue;
                        }
                    }
                    
                    // Evaluate trigger
                    bool triggered = false;
                    
                    if (event->type == QYMERA_EVENT_SENSOR_CHANGED) {
                        if (strcmp(event->device_id, cr->rule.trigger.entity.device_id) == 0 &&
                            strcmp(event->entity_id, cr->rule.trigger.entity.entity_id) == 0) {
                            
                            qymera_entity_value_t value = event->payload.sensor_changed.value;
                            if (evaluate_condition(&cr->rule.trigger, &value)) {
                                if (evaluate_sustained(&cr->rule.trigger, &cr->state, now_ms)) {
                                    triggered = true;
                                }
                            } else {
                                cr->state.sustained_start = 0;
                            }
                        }
                    
                    // Evaluate additional conditions (AND logic)
                    if (triggered) {
                        if (evaluate_all_conditions(engine, &cr->rule, &cr->state, now_ms)) {
                            // Execute actions
                            qymera_rule_engine_execute_actions(engine, cr, event);
                            
                            cr->state.last_triggered = now_ms;
                            cr->state.last_action = now_ms;
                            cr->state.activation_count++;
                            cr->state.hourly_activations++;
                            cr->state.in_cooldown = (cr->rule.cooldown_ms > 0);
                            
                            // Set up cooldown timer
                            if (cr->rule.cooldown_ms > 0) {
                                uint32_t expires_at = qymera_system_get_uptime_ms() + cr->rule.cooldown_ms;
                                int16_t timer_idx = qymera_timer_wheel_add(&engine->timer_wheel, expires_at, (uint16_t)(cr - engine->rules), 0);
                                cr->state.cooldown_timer_idx = timer_idx;
                            }
                            
                            // Emit automation triggered event
                            qymera_event_t auto_event;
                            qymera_event_make_automation_triggered(&auto_event, cr->rule.rule_id, cr->rule.revision, true);
                            qymera_event_bus_publish(engine->event_bus, &auto_event);
                            
                            if (engine->log) {
                                qymera_log_automation(engine->log, "rule_engine", "Rule %s fired", cr->rule.rule_id);
                            }
                            
                            evaluated++;
                        }
                    }
                }
            }
        }
    }
    
    return evaluated;
}

size_t qymera_rule_engine_tick(qymera_rule_engine_t *engine, uint32_t now_ms) {
    if (!engine) return 0;
    
    size_t evaluated = 0;
    
    // Process timers
    qymera_timer_wheel_tick(&engine->timer_wheel, engine, now_ms);
    
    // Handle interval rules
    for (size_t i = 0; i < engine->max_rules; i++) {
        qymera_compiled_rule_t *cr = &engine->rules[i];
        if (!cr->compiled || !cr->rule.enabled) continue;
        
        // Check interval trigger
        if (cr->rule.trigger.condition_type == 3) {  // INTERVAL
            uint32_t interval = cr->rule.trigger.duration_ms;
            if (interval > 0 && now_ms - cr->state.last_triggered >= interval) {
                // Create synthetic event for interval
                qymera_event_t event;
                memset(&event, 0, sizeof(event));
                event.type = QYMERA_EVENT_SCHEDULE_ALARM;
                event.timestamp = qymera_timestamp_now();
                
                qymera_rule_engine_execute_actions(engine, cr, &event);
                
                cr->state.last_triggered = now_ms;
                cr->state.last_action = now_ms;
                cr->state.activation_count++;
                evaluated++;
            }
        }
    }
    
    return evaluated;
}

qymera_err_t qymera_rule_engine_get(qymera_rule_engine_t *engine, uint16_t slot_idx, qymera_compiled_rule_t *compiled) {
    if (!engine || !compiled || slot_idx >= engine->max_rules) return QYMERA_ERR_INVALID_ARG;
    if (!engine->rules[slot_idx].compiled) return QYMERA_ERR_NOT_FOUND;
    
    *compiled = engine->rules[slot_idx];
    return QYMERA_OK;
}

size_t qymera_rule_engine_list(qymera_rule_engine_t *engine, qymera_rule_list_cb_t callback, void *context) {
    if (!engine || !callback) return 0;
    
    size_t listed = 0;
    for (size_t i = 0; i < engine->max_rules; i++) {
        if (engine->rules[i].compiled) {
            if (!callback((uint16_t)i, &engine->rules[i], context)) break;
            listed++;
        }
    }
    return listed;
}

qymera_err_t qymera_rule_engine_execute_actions(qymera_rule_engine_t *engine, qymera_compiled_rule_t *compiled, const qymera_event_t *event) {
    if (!engine || !compiled) return QYMERA_ERR_INVALID_ARG;
    
    // Execute actions through the control API
    // For now, emit actuator.changed events which will be handled by the control subsystem
    for (uint8_t i = 0; i < compiled->rule.action_count; i++) {
        const qymera_action_t *action = &compiled->rule.actions[i];
        
        qymera_entity_value_t value = {0};
        value.valid = true;
        value.numeric_value = action->value_f;
        value.bool_value = (action->value_u32 != 0);
        value.timestamp = qymera_timestamp_now();
        
        qymera_event_t act_event;
        qymera_event_make_actuator_changed(&act_event, action->entity.device_id, action->entity.entity_id, &value);
        qymera_event_bus_publish(engine->event_bus, &act_event);
        // Control GPIO if this is a relay action and pin is mapped
        // Look up entity in registry by reference
        uint16_t entity_idx;
        if (qymera_registry_find_entity(engine->registry, action->entity.device_id, action->entity.entity_id, &entity_idx) == QYMERA_OK) {
            qymera_entity_t found_entity;
            qymera_err_t _err = qymera_registry_get_entity(engine->registry, entity_idx, &found_entity);
            if (_err == QYMERA_OK && found_entity.gpio_pin >= 0) {
                qymera_gpio_write(found_entity.gpio_pin, action->value_u32 != 0 ? QYMERA_GPIO_HIGH : QYMERA_GPIO_LOW);
            }
        }
        
        if (engine->log) {
            qymera_log_action(engine->log, "rule_engine", "Action: %s -> %s = %.2f", 
                             compiled->rule.rule_id, action->entity.entity_id, action->value_f);
        }
    }
    
    // Emit automation completed event
    qymera_event_t complete_event;
    qymera_event_t trigger_event;
    qymera_event_make_automation_triggered(&trigger_event, compiled->rule.rule_id, compiled->rule.revision, true);
    // Would create proper completed event
    
    return QYMERA_OK;
}

qymera_err_t qymera_rule_engine_dry_run(qymera_rule_engine_t *engine, const qymera_rule_t *rule, const qymera_event_t *event, bool *fired) {
    if (!engine || !rule || !event || !fired) return QYMERA_ERR_INVALID_ARG;
    
    *fired = false;
    
    // Simplified dry-run: just check if trigger matches
    if (event->type == QYMERA_EVENT_SENSOR_CHANGED) {
        if (strcmp(event->device_id, rule->trigger.entity.device_id) == 0 &&
            strcmp(event->entity_id, rule->trigger.entity.entity_id) == 0) {
            
            if (evaluate_condition(&rule->trigger, &event->payload.sensor_changed.value)) {
                *fired = true;
            }
        }
    }
    
    return QYMERA_OK;
}

/* Subscription management */
qymera_err_t qymera_rule_engine_subscribe_entity(qymera_rule_engine_t *engine, const qymera_entity_ref_t *entity_ref, uint16_t rule_slot) {
    if (!engine || !entity_ref || rule_slot >= engine->max_rules) return QYMERA_ERR_INVALID_ARG;
    
    uint16_t entity_idx;
    qymera_err_t err = qymera_registry_find_entity_by_ref(engine->registry, entity_ref, &entity_idx);
    if (err != QYMERA_OK) return err;
    
    if (entity_idx >= engine->max_entities) return QYMERA_ERR_INVALID_ARG;
    
    qymera_entity_subscribers_t *subs = &engine->entity_subscribers[entity_idx];
    if (subs->count >= QYMERA_MAX_SUBSCRIPTIONS_PER_ENTITY) return QYMERA_ERR_NO_SPACE;
    
    // Check if already subscribed
    for (uint8_t i = 0; i < subs->count; i++) {
        if (subs->rule_slots[i] == rule_slot) return QYMERA_OK;
    }
    
    subs->rule_slots[subs->count++] = rule_slot;
    return QYMERA_OK;
}

qymera_err_t qymera_rule_engine_unsubscribe_entity(qymera_rule_engine_t *engine, const qymera_entity_ref_t *entity_ref, uint16_t rule_slot) {
    if (!engine || !entity_ref || rule_slot >= engine->max_rules) return QYMERA_ERR_INVALID_ARG;
    
    uint16_t entity_idx;
    qymera_err_t err = qymera_registry_find_entity_by_ref(engine->registry, entity_ref, &entity_idx);
    if (err != QYMERA_OK) return err;
    
    if (entity_idx >= engine->max_entities) return QYMERA_ERR_INVALID_ARG;
    
    qymera_entity_subscribers_t *subs = &engine->entity_subscribers[entity_idx];
    for (uint8_t i = 0; i < subs->count; i++) {
        if (subs->rule_slots[i] == rule_slot) {
            // Remove by shifting
            for (uint8_t j = i; j < subs->count - 1; j++) {
                subs->rule_slots[j] = subs->rule_slots[j + 1];
            }
            subs->count--;
            break;
        }
    }
    
    return QYMERA_OK;
}

/* Timer wheel implementation */
void qymera_timer_wheel_init(qymera_timer_wheel_t *wheel) {
    if (!wheel) return;
    memset(wheel, 0, sizeof(qymera_timer_wheel_t));
    wheel->current_tick = qymera_system_get_uptime_ms() / QYMERA_TIMER_WHEEL_RESOLUTION_MS;
}

int16_t qymera_timer_wheel_add(qymera_timer_wheel_t *wheel, uint32_t expires_at, uint16_t rule_slot, uint8_t timer_type) {
    if (!wheel) return -1;
    
    uint32_t expires_tick = expires_at / QYMERA_TIMER_WHEEL_RESOLUTION_MS;
    uint32_t current_tick = wheel->current_tick;
    uint32_t delta_ticks = (expires_tick >= current_tick) ? (expires_tick - current_tick) : (expires_tick + QYMERA_TIMER_WHEEL_SLOTS - current_tick);
    
    // Find free slot
    int16_t idx = -1;
    for (int i = 0; i < QYMERA_TIMER_WHEEL_SLOTS; i++) {
        int slot_idx = (current_tick + i) % QYMERA_TIMER_WHEEL_SLOTS;
        if (!wheel->slots[slot_idx].active) {
            idx = slot_idx;
            break;
        }
    }
    
    if (idx == -1) return -1;  // Wheel full
    
    wheel->slots[idx].expires_at = expires_at;
    wheel->slots[idx].rule_slot = rule_slot;
    wheel->slots[idx].timer_type = timer_type;
    wheel->slots[idx].active = true;
    
    return idx;
}

void qymera_timer_wheel_remove(qymera_timer_wheel_t *wheel, int16_t timer_idx) {
    if (!wheel || timer_idx < 0 || timer_idx >= QYMERA_TIMER_WHEEL_SLOTS) return;
    wheel->slots[timer_idx].active = false;
    wheel->slots[timer_idx].rule_slot = 0;
    wheel->slots[timer_idx].timer_type = 0;
    wheel->slots[timer_idx].expires_at = 0;
}

void qymera_timer_wheel_tick(qymera_timer_wheel_t *wheel, qymera_rule_engine_t *engine, uint32_t now_ms) {
    if (!wheel || !engine) return;
    
    uint32_t current_tick = now_ms / QYMERA_TIMER_WHEEL_RESOLUTION_MS;
    if (current_tick == wheel->current_tick) return;
    
    wheel->current_tick = current_tick;
    uint32_t slot = current_tick % QYMERA_TIMER_WHEEL_SLOTS;
    
    // Process expired timers in this slot
    if (wheel->slots[slot].active && wheel->slots[slot].expires_at <= now_ms) {
        qymera_compiled_rule_t *cr = &engine->rules[wheel->slots[slot].rule_slot];
        uint8_t timer_type = wheel->slots[slot].timer_type;
        
        // Handle timer based on type
        switch (timer_type) {
            case 0: // cooldown
                cr->state.in_cooldown = false;
                cr->state.cooldown_timer_idx = -1;
                break;
            case 1: // interval
                cr->state.interval_timer_idx = -1;
                // Re-arm interval timer
                {
                    uint32_t interval = cr->rule.trigger.duration_ms;
                    if (interval > 0) {
                        uint32_t expires_at = now_ms + interval;
                        int16_t timer_idx = qymera_timer_wheel_add(&engine->timer_wheel, expires_at, (uint16_t)(cr - engine->rules), 1);
                        cr->state.interval_timer_idx = timer_idx;
                    }
                }
                // Trigger interval rule
                qymera_event_t event;
                memset(&event, 0, sizeof(event));
                event.type = QYMERA_EVENT_SCHEDULE_ALARM;
                event.timestamp = qymera_timestamp_now();
                qymera_rule_engine_execute_actions(engine, cr, &event);
                cr->state.last_triggered = now_ms;
                cr->state.last_action = now_ms;
                cr->state.activation_count++;
                break;
            case 2: // sustained window
                cr->state.sustained_timer_idx = -1;
                // Sustained condition expired - would trigger evaluation
                break;
            case 3: // delay
                cr->state.delay_timer_idx = -1;
                // Execute delayed actions
                qymera_event_t delay_event;
                memset(&delay_event, 0, sizeof(delay_event));
                delay_event.type = QYMERA_EVENT_SCHEDULE_ALARM;
                delay_event.timestamp = qymera_timestamp_now();
                qymera_rule_engine_execute_actions(engine, cr, &delay_event);
                break;
        }
        
        wheel->slots[slot].active = false;
    }
    }
}
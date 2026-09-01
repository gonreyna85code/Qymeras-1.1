/**
 * Qymera Dashboard - Deterministic Skill API implementation.
 *
 * Deterministic, bounded, caller-agnostic surface over Registry / Rule Engine /
 * Control API. No natural-language parsing, no inference, no direct GPIO/UDP.
 */
#include "qymera_skill.h"
#include "qymera_log.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* =========================
 * Internal helpers
 * ========================= */

static void out_clear(qymera_skill_output_t *o) {
    memset(o, 0, sizeof(*o));
    o->ok = false;
    o->data[0] = '\0';
}

/* Append a formatted JSON fragment to the bounded output buffer. */
static void out_add(qymera_skill_output_t *o, const char *fmt, ...) {
    if (!o || o->truncated) return;
    char tmp[256];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (n < 0) { o->truncated = true; return; }
    if ((size_t)n >= sizeof(tmp)) n = (int)sizeof(tmp) - 1;
    if (o->data_len + (size_t)n < QYMERA_SKILL_OUTPUT_SIZE) {
        memcpy(o->data + o->data_len, tmp, (size_t)n);
        o->data_len += (size_t)n;
        o->data[o->data_len] = '\0';
    } else {
        o->truncated = true;
        o->data_len = QYMERA_SKILL_OUTPUT_SIZE - 1;
        o->data[o->data_len] = '\0';
    }
}

static void out_error(qymera_skill_output_t *o, const char *code, const char *msg) {
    o->ok = false;
    o->truncated = false;
    o->data_len = 0;
    o->data[0] = '\0';
    snprintf(o->error_code, sizeof(o->error_code), "%s", code);
    snprintf(o->message, sizeof(o->message), "%s", msg);
}

static void out_ok(qymera_skill_output_t *o) {
    o->ok = true;
    o->error_code[0] = '\0';
    o->message[0] = '\0';
}

static void set_err_from(qymera_skill_output_t *o, qymera_err_t e, const char *def_code,
                         const char *def_msg) {
    switch (e) {
        case QYMERA_ERR_NOT_FOUND: out_error(o, "ENTITY_NOT_FOUND", def_msg); break;
        case QYMERA_ERR_NO_SPACE:  out_error(o, "NO_SPACE", "pending command table full"); break;
        case QYMERA_ERR_NETWORK:   out_error(o, "DEVICE_OFFLINE", "command transport unavailable"); break;
        case QYMERA_ERR_TIMEOUT:   out_error(o, "COMMAND_TIMEOUT", "command timed out"); break;
        case QYMERA_ERR_INVALID_CAPABILITY: out_error(o, "INVALID_CAPABILITY", def_msg); break;
        case QYMERA_ERR_INVALID_ARG: out_error(o, "INVALID_VALUE", def_msg); break;
        default: out_error(o, def_code, def_msg); break;
    }
}

/* =========================
 * Stable string mappings
 * ========================= */

const char *qymera_skill_cmd_status_str(qymera_cmd_status_t status) {
    static const char *names[] = {
        "REQUESTED", "DISPATCHED", "WAITING_ACK", "ACKED",
        "STATE_CONFIRMED", "FAILED", "TIMEOUT"
    };
    if ((int)status < 0 || (int)status >= (int)(sizeof(names) / sizeof(names[0])))
        return "UNKNOWN";
    return names[status];
}

const char *qymera_skill_reliability_str(uint8_t observed_reliability, qymera_cmd_status_t status) {
    switch (status) {
        case QYMERA_CMD_STATE_CONFIRMED: return "CONFIRMED";
        case QYMERA_CMD_ACKED:
        case QYMERA_CMD_WAITING_ACK:
        case QYMERA_CMD_DISPATCHED:
        case QYMERA_CMD_REQUESTED: return "PENDING";
        case QYMERA_CMD_FAILED:
        case QYMERA_CMD_TIMEOUT: return "FAILED";
        default: break;
    }
    if (observed_reliability == 1 || observed_reliability == 2) return "STALE";
    return "UNKNOWN";
}

static const char *entity_type_str(qymera_entity_type_t t) {
    static const char *names[] = {
        "none", "sensor.temperature", "sensor.humidity", "sensor.luminosity",
        "sensor.pressure", "sensor.level", "sensor.airq", "sensor.rain",
        "sensor.contact", "sensor.generic", "actuator.relay", "actuator.dimmer",
        "virtual.digital", "virtual.analog", "inference.result", "time"
    };
    if ((int)t < 0 || (int)t >= (int)(sizeof(names) / sizeof(names[0]))) return "none";
    return names[t];
}

static const char *capability_str(qymera_capability_t c) {
    switch (c) {
        case QYMERA_CAP_SENSOR_NUMERIC: return "sensor.numeric";
        case QYMERA_CAP_SENSOR_DIGITAL: return "sensor.digital";
        case QYMERA_CAP_ACTUATOR_RELAY: return "actuator.relay";
        case QYMERA_CAP_ACTUATOR_DIMMER: return "actuator.dimmer";
        case QYMERA_CAP_ACTUATOR_GENERIC: return "actuator.generic";
        case QYMERA_CAP_INFERENCE_RESULT: return "inference.result";
        case QYMERA_CAP_TIME_SOURCE: return "time";
        default: return "none";
    }
}

static const char *operator_str(qymera_operator_t o) {
    switch (o) {
        case QYMERA_OP_GT: return "GT";
        case QYMERA_OP_LT: return "LT";
        case QYMERA_OP_GE: return "GE";
        case QYMERA_OP_LE: return "LE";
        case QYMERA_OP_EQ: return "EQ";
        case QYMERA_OP_NE: return "NE";
        case QYMERA_OP_IN_RANGE: return "IN_RANGE";
        case QYMERA_OP_OUT_RANGE: return "OUT_RANGE";
        default: return "NONE";
    }
}

static const char *action_str(qymera_action_type_t a) {
    switch (a) {
        case QYMERA_ACTION_SET_BOOL: return "SET_BOOL";
        case QYMERA_ACTION_SET_LEVEL: return "SET_LEVEL";
        case QYMERA_ACTION_SET_VALUE: return "SET_VALUE";
        case QYMERA_ACTION_TOGGLE: return "TOGGLE";
        case QYMERA_ACTION_PULSE: return "PULSE";
        case QYMERA_ACTION_FADE: return "FADE";
        default: return "NONE";
    }
}

static const char *device_role_str(uint8_t role) {
    switch (role) {
        case 0: return "dashboard";
        case 1: return "remote";
        case 2: return "provisioning";
        default: return "unknown";
    }
}

static const char *device_state_str(uint8_t st) {
    switch (st) {
        case 0: return "operational";
        case 1: return "offline";
        case 2: return "degraded";
        case 3: return "provisioning";
        default: return "unknown";
    }
}

static bool entity_has_cap(const qymera_entity_t *e, qymera_capability_t cap) {
    for (uint8_t i = 0; i < e->capability_count && i < 4; i++) {
        if (e->capabilities[i] == cap) return true;
    }
    return false;
}

static bool entity_is_relay(const qymera_entity_t *e) {
    return entity_has_cap(e, QYMERA_CAP_ACTUATOR_RELAY);
}

static bool entity_is_dimmer(const qymera_entity_t *e) {
    return entity_has_cap(e, QYMERA_CAP_ACTUATOR_DIMMER);
}

/* =========================
 * Fixed, compile-time Skill registry
 * ========================= */

static const qymera_skill_entry_t skills[QYMERA_MAX_SKILLS] = {
    { QYMERA_SKILL_DEVICES_LIST,    { "list_devices",     "1.0", "List all devices",                 "qymera.device.list.v1",    QYMERA_PERM_READ } },
    { QYMERA_SKILL_ENTITIES_LIST,   { "list_entities",    "1.0", "List all entities across devices", "qymera.entity.list.v1",    QYMERA_PERM_READ } },
    { QYMERA_SKILL_ENTITY_STATE_GET,{ "get_entity_state", "1.0", "Get observed+desired entity state","qymera.entity.state.v1",  QYMERA_PERM_READ } },
    { QYMERA_SKILL_ENTITY_INFO_GET, { "get_entity_info",  "1.0", "Get entity metadata and capabilities","qymera.entity.info.v1", QYMERA_PERM_READ } },
    { QYMERA_SKILL_RELAY_SET,       { "set_relay",        "1.0", "Set a relay actuator (bool)",      "qymera.control.relay.v1",  QYMERA_PERM_CONTROL } },
    { QYMERA_SKILL_DIMMER_SET,      { "set_dimmer",       "1.0", "Set a dimmer level (0-100)",      "qymera.control.dimmer.v1", QYMERA_PERM_CONTROL } },
    { QYMERA_SKILL_RULES_LIST,      { "list_rules",       "1.0", "List automation rules",           "qymera.rule.list.v1",      QYMERA_PERM_RULE_READ } },
    { QYMERA_SKILL_RULE_GET,        { "get_rule",         "1.0", "Get a rule's definition + state", "qymera.rule.get.v1",       QYMERA_PERM_RULE_READ } },
    { QYMERA_SKILL_RULE_CREATE,     { "create_rule",      "1.0", "Create a rule from structured input","qymera.rule.create.v1",  QYMERA_PERM_RULE_WRITE } },
    { QYMERA_SKILL_RULE_UPDATE,     { "update_rule",      "1.0", "Update an existing rule",         "qymera.rule.update.v1",    QYMERA_PERM_RULE_WRITE } },
    { QYMERA_SKILL_RULE_DELETE,     { "delete_rule",      "1.0", "Delete a rule",                   "qymera.rule.delete.v1",    QYMERA_PERM_RULE_WRITE } },
    { QYMERA_SKILL_RULE_ENABLE,     { "enable_rule",      "1.0", "Enable a rule",                   "qymera.rule.enable.v1",    QYMERA_PERM_RULE_WRITE } },
    { QYMERA_SKILL_RULE_DISABLE,    { "disable_rule",     "1.0", "Disable a rule",                  "qymera.rule.disable.v1",   QYMERA_PERM_RULE_WRITE } },
};

size_t qymera_skill_registry_count(void) {
    return QYMERA_MAX_SKILLS;
}

qymera_skill_id_t qymera_skill_registry_get(size_t idx, const qymera_skill_entry_t **entry) {
    if (idx >= QYMERA_MAX_SKILLS || !entry) return QYMERA_SKILL_DEVICES_LIST;
    *entry = &skills[idx];
    return skills[idx].id;
}

qymera_skill_id_t qymera_skill_lookup(const char *name) {
    if (!name) return QYMERA_SKILL_DEVICES_LIST;
    for (size_t i = 0; i < QYMERA_MAX_SKILLS; i++) {
        if (strcmp(skills[i].meta.name, name) == 0) return skills[i].id;
    }
    return (qymera_skill_id_t)-1;
}

qymera_err_t qymera_skill_context_init(qymera_skill_context_t *ctx,
                                       qymera_registry_t *registry,
                                       qymera_rule_engine_t *rule_engine,
                                       qymera_control_context_t *control,
                                       qymera_storage_t *storage,
                                       qymera_log_t *log) {
    if (!ctx) return QYMERA_ERR_INVALID_ARG;
    memset(ctx, 0, sizeof(*ctx));
    ctx->registry = registry;
    ctx->rule_engine = rule_engine;
    ctx->control = control;
    ctx->storage = storage;
    ctx->log = log;
    return QYMERA_OK;
}

/* =========================
 * Registry-backed read handlers
 * ========================= */

static bool device_list_cb(uint16_t idx, const qymera_device_t *d, void *context) {
    (void)idx;
    qymera_skill_output_t *o = context;
    out_add(o, "%s{\"device_id\":\"%s\",\"name\":\"%s\",\"model\":\"%s\",\"role\":\"%s\",\"online\":%s,\"state\":\"%s\",\"location\":\"%s\"}",
            (o->data_len > 1 ? "," : ""),
            d->device_id, d->name, d->model, device_role_str(d->role),
            d->online ? "true" : "false", device_state_str(d->state), d->location);
    return true;
}

static qymera_err_t skill_list_devices(qymera_skill_context_t *ctx, qymera_skill_output_t *o) {
    out_add(o, "[");
    qymera_registry_iterate_devices(ctx->registry, device_list_cb, o);
    out_add(o, "]");
    out_ok(o);
    return QYMERA_OK;
}

static bool entity_list_cb(uint16_t idx, const qymera_entity_t *e, void *context) {
    (void)idx;
    qymera_skill_output_t *o = context;
    out_add(o, "%s{\"device_id\":\"%s\",\"entity_id\":\"%s\",\"name\":\"%s\",\"type\":\"%s\",\"capabilities\":[",
            (o->data_len > 1 ? "," : ""), e->device_id, e->entity_id, e->name, entity_type_str(e->type));
    for (uint8_t i = 0; i < e->capability_count && i < 4; i++) {
        out_add(o, "%s\"%s\"", (i ? "," : ""), capability_str(e->capabilities[i]));
    }
    bool i_relay = entity_is_relay(e), i_dimmer = entity_is_dimmer(e);
    if (i_relay)      out_add(o, "],\"unit\":\"%s\",\"current\":%s,\"desired\":%s,\"cmd_status\":\"%s\"}",
                              e->unit, e->value.bool_value ? "true" : "false",
                              e->desired.bool_value ? "true" : "false",
                              qymera_skill_cmd_status_str(e->cmd_status));
    else if (i_dimmer) out_add(o, "],\"unit\":\"%s\",\"current\":%g,\"desired\":%g,\"cmd_status\":\"%s\"}",
                              e->unit, (double)e->value.numeric_value, (double)e->desired.numeric_value,
                              qymera_skill_cmd_status_str(e->cmd_status));
    else              out_add(o, "],\"unit\":\"%s\",\"current\":%g,\"desired\":%g,\"cmd_status\":\"%s\"}",
                              e->unit, (double)e->value.numeric_value, (double)e->desired.numeric_value,
                              qymera_skill_cmd_status_str(e->cmd_status));
    return true;
}

static qymera_err_t skill_list_entities(qymera_skill_context_t *ctx, qymera_skill_output_t *o) {
    out_add(o, "[");
    size_t dc = qymera_registry_device_count(ctx->registry);
    for (size_t i = 0; i < dc; i++) {
        qymera_device_t d;
        if (qymera_registry_get_device(ctx->registry, (uint16_t)i, &d) == QYMERA_OK) {
            qymera_registry_iterate_device_entities(ctx->registry, (uint16_t)i, entity_list_cb, o);
        }
    }
    out_add(o, "]");
    out_ok(o);
    return QYMERA_OK;
}

static qymera_err_t lookup_entity(qymera_skill_context_t *ctx, const qymera_skill_input_t *in,
                                  uint16_t *eidx, qymera_entity_t *entity) {
    if (!in->device_id[0] || !in->entity_id[0]) {
        return QYMERA_ERR_INVALID_ARG;
    }
    uint16_t idx;
    qymera_err_t err = qymera_registry_find_entity(ctx->registry, in->device_id, in->entity_id, &idx);
    if (err != QYMERA_OK) return QYMERA_ERR_NOT_FOUND;
    err = qymera_registry_get_entity(ctx->registry, idx, entity);
    if (err != QYMERA_OK) return QYMERA_ERR_NOT_FOUND;
    *eidx = idx;
    return QYMERA_OK;
}

static qymera_err_t skill_get_entity_state(qymera_skill_context_t *ctx, const qymera_skill_input_t *in,
                                           qymera_skill_output_t *o) {
    uint16_t eidx; qymera_entity_t e;
    qymera_err_t err = lookup_entity(ctx, in, &eidx, &e);
    (void)eidx;
    if (err != QYMERA_OK) { out_error(o, "ENTITY_NOT_FOUND", "entity not found"); return QYMERA_OK; }

    bool is_relay = entity_is_relay(&e), is_dimmer = entity_is_dimmer(&e);
    if (is_relay) {
        out_add(o, "{\"device_id\":\"%s\",\"entity_id\":\"%s\",\"observed\":%s,\"desired\":%s,\"status\":\"%s\",\"reliability\":\"%s\",\"timestamp\":%u}",
                e.device_id, e.entity_id,
                e.value.bool_value ? "true" : "false", e.desired.bool_value ? "true" : "false",
                qymera_skill_cmd_status_str(e.cmd_status),
                qymera_skill_reliability_str(e.value.reliability, e.cmd_status),
                (unsigned)e.last_updated.seconds);
    } else if (is_dimmer) {
        out_add(o, "{\"device_id\":\"%s\",\"entity_id\":\"%s\",\"observed\":%g,\"desired\":%g,\"status\":\"%s\",\"reliability\":\"%s\",\"timestamp\":%u}",
                e.device_id, e.entity_id,
                (double)e.value.numeric_value, (double)e.desired.numeric_value,
                qymera_skill_cmd_status_str(e.cmd_status),
                qymera_skill_reliability_str(e.value.reliability, e.cmd_status),
                (unsigned)e.last_updated.seconds);
    } else {
        out_add(o, "{\"device_id\":\"%s\",\"entity_id\":\"%s\",\"observed\":%g,\"desired\":%g,\"status\":\"%s\",\"reliability\":\"%s\",\"timestamp\":%u}",
                e.device_id, e.entity_id,
                (double)e.value.numeric_value, (double)e.desired.numeric_value,
                qymera_skill_cmd_status_str(e.cmd_status),
                qymera_skill_reliability_str(e.value.reliability, e.cmd_status),
                (unsigned)e.last_updated.seconds);
    }
    out_ok(o);
    return QYMERA_OK;
}

static qymera_err_t skill_get_entity_info(qymera_skill_context_t *ctx, const qymera_skill_input_t *in,
                                          qymera_skill_output_t *o) {
    uint16_t eidx; qymera_entity_t e;
    qymera_err_t err = lookup_entity(ctx, in, &eidx, &e);
    (void)eidx;
    if (err != QYMERA_OK) { out_error(o, "ENTITY_NOT_FOUND", "entity not found"); return QYMERA_OK; }

    out_add(o, "{\"device_id\":\"%s\",\"entity_id\":\"%s\",\"name\":\"%s\",\"type\":\"%s\",\"capabilities\":[",
            e.device_id, e.entity_id, e.name, entity_type_str(e.type));
    for (uint8_t i = 0; i < e.capability_count && i < 4; i++) {
        out_add(o, "%s\"%s\"", (i ? "," : ""), capability_str(e.capabilities[i]));
    }
    out_add(o, "],\"unit\":\"%s\",\"native_min\":%g,\"native_max\":%g,\"calibration_min\":%g,\"calibration_max\":%g,\"correction\":%g,\"persist_state\":%s,\"protected\":%s,\"last_updated\":%u}",
            e.unit,
            (double)e.native_min, (double)e.native_max,
            (double)e.calibration_min, (double)e.calibration_max, (double)e.correction,
            e.persist_state ? "true" : "false", e.protected_actuator ? "true" : "false",
            (unsigned)e.last_updated.seconds);
    out_ok(o);
    return QYMERA_OK;
}

/* =========================
 * Control handlers (through Control API only)
 * ========================= */

static void append_dispatch_result(qymera_skill_output_t *o, const qymera_entity_t *e,
                                   const char *device_id, const char *entity_id) {
    bool is_relay = entity_is_relay(e), is_dimmer = entity_is_dimmer(e);
    if (is_relay) {
        out_add(o, "{\"device_id\":\"%s\",\"entity_id\":\"%s\",\"requested\":%s,\"observed\":%s,\"desired\":%s,\"status\":\"%s\",\"reliability\":\"%s\"}",
                device_id, entity_id,
                e->desired.bool_value ? "true" : "false",
                e->value.bool_value ? "true" : "false",
                e->desired.bool_value ? "true" : "false",
                qymera_skill_cmd_status_str(e->cmd_status),
                qymera_skill_reliability_str(e->value.reliability, e->cmd_status));
    } else if (is_dimmer) {
        out_add(o, "{\"device_id\":\"%s\",\"entity_id\":\"%s\",\"requested\":%g,\"observed\":%g,\"desired\":%g,\"status\":\"%s\",\"reliability\":\"%s\"}",
                device_id, entity_id, (double)e->desired.numeric_value,
                (double)e->value.numeric_value, (double)e->desired.numeric_value,
                qymera_skill_cmd_status_str(e->cmd_status),
                qymera_skill_reliability_str(e->value.reliability, e->cmd_status));
    } else {
        out_add(o, "{\"device_id\":\"%s\",\"entity_id\":\"%s\",\"requested\":%g,\"observed\":%g,\"desired\":%g,\"status\":\"%s\",\"reliability\":\"%s\"}",
                device_id, entity_id, (double)e->desired.numeric_value,
                (double)e->value.numeric_value, (double)e->desired.numeric_value,
                qymera_skill_cmd_status_str(e->cmd_status),
                qymera_skill_reliability_str(e->value.reliability, e->cmd_status));
    }
}

static qymera_err_t skill_set_relay(qymera_skill_context_t *ctx, const qymera_skill_input_t *in,
                                    qymera_skill_output_t *o) {
    uint16_t eidx; qymera_entity_t e;
    qymera_err_t err = lookup_entity(ctx, in, &eidx, &e);
    if (err != QYMERA_OK) { out_error(o, "ENTITY_NOT_FOUND", "entity not found"); return QYMERA_OK; }
    if (!entity_is_relay(&e)) { out_error(o, "INVALID_CAPABILITY", "target is not a relay actuator"); return QYMERA_OK; }

    qymera_entity_ref_t ref;
    strncpy(ref.device_id, in->device_id, sizeof(ref.device_id) - 1);
    strncpy(ref.entity_id, in->entity_id, sizeof(ref.entity_id) - 1);
    err = qymera_control_set_relay(ctx->control, &ref, in->value, false);
    if (err != QYMERA_OK) { set_err_from(o, err, "COMMAND_FAILED", "relay command failed"); return QYMERA_OK; }

    qymera_registry_get_entity(ctx->registry, eidx, &e);
    append_dispatch_result(o, &e, in->device_id, in->entity_id);
    out_ok(o);
    return QYMERA_OK;
}

static qymera_err_t skill_set_dimmer(qymera_skill_context_t *ctx, const qymera_skill_input_t *in,
                                     qymera_skill_output_t *o) {
    if (in->level > 100) { out_error(o, "INVALID_VALUE", "dimmer level must be 0-100"); return QYMERA_OK; }
    uint16_t eidx; qymera_entity_t e;
    qymera_err_t err = lookup_entity(ctx, in, &eidx, &e);
    if (err != QYMERA_OK) { out_error(o, "ENTITY_NOT_FOUND", "entity not found"); return QYMERA_OK; }
    if (!entity_is_dimmer(&e)) { out_error(o, "INVALID_CAPABILITY", "target is not a dimmer actuator"); return QYMERA_OK; }

    qymera_entity_ref_t ref;
    strncpy(ref.device_id, in->device_id, sizeof(ref.device_id) - 1);
    strncpy(ref.entity_id, in->entity_id, sizeof(ref.entity_id) - 1);
    err = qymera_control_set_dimmer(ctx->control, &ref, in->level, false);
    if (err != QYMERA_OK) { set_err_from(o, err, "COMMAND_FAILED", "dimmer command failed"); return QYMERA_OK; }

    qymera_registry_get_entity(ctx->registry, eidx, &e);
    append_dispatch_result(o, &e, in->device_id, in->entity_id);
    out_ok(o);
    return QYMERA_OK;
}

/* =========================
 * Rule skills
 * ========================= */

typedef struct {
    uint16_t slot;
    qymera_compiled_rule_t compiled;
    bool found;
} rule_lookup_t;

static bool rule_find_cb(uint16_t slot_idx, const qymera_compiled_rule_t *rule, void *context) {
    rule_lookup_t *rl = context;
    if (strcmp(rule->rule.rule_id, rl->compiled.rule.rule_id) == 0) {
        rl->slot = slot_idx;
        rl->compiled = *rule;
        rl->found = true;
        return false;
    }
    return true;
}

static rule_lookup_t find_rule(qymera_skill_context_t *ctx, const char *rule_id) {
    rule_lookup_t rl;
    memset(&rl, 0, sizeof(rl));
    strncpy(rl.compiled.rule.rule_id, rule_id, sizeof(rl.compiled.rule.rule_id) - 1);
    qymera_rule_engine_list(ctx->rule_engine, rule_find_cb, &rl);
    return rl;
}

static bool op_valid(qymera_operator_t op) {
    return op >= QYMERA_OP_GT && op <= QYMERA_OP_OUT_RANGE;
}

static qymera_capability_t action_required_cap(qymera_action_type_t a) {
    switch (a) {
        case QYMERA_ACTION_SET_BOOL:
        case QYMERA_ACTION_TOGGLE:
        case QYMERA_ACTION_PULSE: return QYMERA_CAP_ACTUATOR_RELAY;
        case QYMERA_ACTION_SET_LEVEL:
        case QYMERA_ACTION_SET_VALUE:
        case QYMERA_ACTION_FADE: return QYMERA_CAP_ACTUATOR_DIMMER;
        default: return QYMERA_CAP_NONE;
    }
}

/* Validate every entity reference exists and every action is capability-safe.
 * Returns the first failure code via out_error style strings. */
static const char *validate_rule_refs(qymera_skill_context_t *ctx, const qymera_rule_t *r,
                                      char *err_code, size_t err_code_sz) {
    uint16_t idx;
    qymera_entity_t e;

    if (r->trigger.operator_ != QYMERA_OP_NONE) {
        if (qymera_registry_find_entity_by_ref(ctx->registry, &r->trigger.entity, &idx) != QYMERA_OK) {
            snprintf(err_code, err_code_sz, "ENTITY_NOT_FOUND");
            return "trigger references a nonexistent entity";
        }
        if (!op_valid(r->trigger.operator_)) { snprintf(err_code, err_code_sz, "RULE_INVALID"); return "invalid trigger operator"; }
    }
    for (uint8_t i = 0; i < r->condition_count; i++) {
        const qymera_condition_t *c = &r->conditions[i];
        if (qymera_registry_find_entity_by_ref(ctx->registry, &c->entity, &idx) != QYMERA_OK) {
            snprintf(err_code, err_code_sz, "ENTITY_NOT_FOUND");
            return "condition references a nonexistent entity";
        }
        if (!op_valid(c->operator_)) { snprintf(err_code, err_code_sz, "RULE_INVALID"); return "invalid condition operator"; }
    }
    for (uint8_t i = 0; i < r->action_count; i++) {
        const qymera_action_t *a = &r->actions[i];
        if (qymera_registry_find_entity_by_ref(ctx->registry, &a->entity, &idx) != QYMERA_OK) {
            snprintf(err_code, err_code_sz, "ENTITY_NOT_FOUND");
            return "action references a nonexistent entity";
        }
        qymera_capability_t cap = action_required_cap(a->action);
        if (cap == QYMERA_CAP_NONE) { snprintf(err_code, err_code_sz, "RULE_INVALID"); return "invalid action type"; }
        if (qymera_registry_get_entity(ctx->registry, idx, &e) != QYMERA_OK) {
            snprintf(err_code, err_code_sz, "ENTITY_NOT_FOUND");
            return "action entity unavailable";
        }
        if (!entity_has_cap(&e, cap)) {
            snprintf(err_code, err_code_sz, "RULE_INVALID");
            return "action incompatible with target capability";
        }
    }
    return NULL;
}

static void emit_rule_ref(char *dst, size_t dst_sz, const qymera_entity_ref_t *ref) {
    snprintf(dst, dst_sz, "{\"device_id\":\"%s\",\"entity_id\":\"%s\"}", ref->device_id, ref->entity_id);
}

static void emit_trigger_condition(qymera_skill_output_t *o, const qymera_condition_t *c) {
    char ref[96];
    emit_rule_ref(ref, sizeof(ref), &c->entity);
    const char *sep = o->data_len > 1 ? "," : "";
    out_add(o, "%s{\"entity\":%s,\"operator\":\"%s\",\"threshold\":%g,\"threshold_high\":%g,\"duration_ms\":%u,\"negate\":%s}",
            sep, ref, operator_str(c->operator_), (double)c->threshold, (double)c->threshold_high,
            (unsigned)c->duration_ms, c->negate ? "true" : "false");
}

static void emit_action(qymera_skill_output_t *o, const qymera_action_t *a) {
    char ref[96];
    emit_rule_ref(ref, sizeof(ref), &a->entity);
    const char *sep = o->data_len > 1 ? "," : "";
    out_add(o, "%s{\"entity\":%s,\"action\":\"%s\",\"value\":%g,\"duration_ms\":%u}",
            sep, ref, action_str(a->action), (double)a->value_f, (unsigned)a->duration_ms);
}

static bool list_rules_cb(uint16_t idx, const qymera_compiled_rule_t *cr, void *context);

static qymera_err_t skill_list_rules(qymera_skill_context_t *ctx, qymera_skill_output_t *o) {
    out_add(o, "[");
    qymera_rule_engine_list(ctx->rule_engine, list_rules_cb, o);
    out_add(o, "]");
    out_ok(o);
    return QYMERA_OK;
}

static qymera_err_t skill_get_rule(qymera_skill_context_t *ctx, const qymera_skill_input_t *in,
                                   qymera_skill_output_t *o) {
    if (!in->rule_id[0]) { out_error(o, "INVALID_VALUE", "rule_id required"); return QYMERA_OK; }
    rule_lookup_t rl = find_rule(ctx, in->rule_id);
    if (!rl.found) { out_error(o, "RULE_INVALID", "rule not found"); return QYMERA_OK; }
    const qymera_rule_t *r = &rl.compiled.rule;
    const qymera_rule_state_t *st = &rl.compiled.state;

    out_add(o, "{\"rule_id\":\"%s\",\"name\":\"%s\",\"enabled\":%s,\"revision\":%u,\"priority\":%u,\"cooldown_ms\":%u,\"max_activations_per_hour\":%u,\"created_ts\":%u,\"updated_ts\":%u,\"state\":{\"activation_count\":%u,\"last_triggered\":%u},\"trigger\":[",
            r->rule_id, r->name, r->enabled ? "true" : "false", (unsigned)r->revision,
            (unsigned)r->priority, (unsigned)r->cooldown_ms, (unsigned)r->max_activations_per_hour,
            (unsigned)r->created_ts, (unsigned)r->updated_ts,
            (unsigned)st->activation_count, (unsigned)st->last_triggered);
    if (r->trigger.operator_ != QYMERA_OP_NONE) emit_trigger_condition(o, &r->trigger);
    out_add(o, "],\"conditions\":[");
    for (uint8_t i = 0; i < r->condition_count; i++) emit_trigger_condition(o, &r->conditions[i]);
    out_add(o, "],\"actions\":[");
    for (uint8_t i = 0; i < r->action_count; i++) emit_action(o, &r->actions[i]);
    out_add(o, "]}");
    out_ok(o);
    return QYMERA_OK;
}

static uint32_t s_rule_seq = 1;
static void gen_rule_id(char *dst, size_t sz) {
    snprintf(dst, sz, "rule_%u", (unsigned)s_rule_seq++);
}

static qymera_err_t persist_rule(qymera_skill_context_t *ctx, qymera_compiled_rule_t *compiled,
                                 bool enabled) {
    qymera_rule_meta_t meta;
    memset(&meta, 0, sizeof(meta));
    strncpy(meta.rule_id, compiled->rule.rule_id, sizeof(meta.rule_id) - 1);
    meta.revision = compiled->rule.revision;
    meta.created_ts = compiled->rule.created_ts;
    meta.updated_ts = compiled->rule.updated_ts;
    meta.enabled = enabled;
    meta.compiled_size = sizeof(qymera_compiled_rule_t);
    meta.checksum = compiled->checksum;
    return qymera_storage_save_rule(ctx->storage, compiled->rule.rule_id, compiled,
                                    sizeof(qymera_compiled_rule_t), &meta);
}

static qymera_err_t skill_create_rule(qymera_skill_context_t *ctx, const qymera_skill_input_t *in,
                                      qymera_skill_output_t *o) {
    if (!in->name[0]) { out_error(o, "INVALID_VALUE", "rule name required"); return QYMERA_OK; }

    qymera_rule_t r;
    memset(&r, 0, sizeof(r));
    if (in->rule_id[0]) strncpy(r.rule_id, in->rule_id, sizeof(r.rule_id) - 1);
    else                gen_rule_id(r.rule_id, sizeof(r.rule_id));
    strncpy(r.name, in->name, sizeof(r.name) - 1);
    r.enabled = true;
    r.revision = 1;
    r.created_ts = qymera_timestamp_now().seconds;
    r.updated_ts = r.created_ts;

    r.trigger = in->rule.trigger;
    r.condition_count = in->rule.condition_count < QYMERA_MAX_CONDITIONS ? in->rule.condition_count : QYMERA_MAX_CONDITIONS;
    for (uint8_t i = 0; i < r.condition_count; i++) r.conditions[i] = in->rule.conditions[i];
    r.action_count = in->rule.action_count < QYMERA_MAX_ACTIONS ? in->rule.action_count : QYMERA_MAX_ACTIONS;
    for (uint8_t i = 0; i < r.action_count; i++) r.actions[i] = in->rule.actions[i];
    r.cooldown_ms = in->rule.cooldown_ms;
    r.max_activations_per_hour = in->rule.max_activations_per_hour;
    r.priority = in->rule.priority;

    char err_code[QYMERA_SKILL_ERROR_CODE_LEN];
    const char *ref_err = validate_rule_refs(ctx, &r, err_code, sizeof(err_code));
    if (ref_err) { out_error(o, err_code, ref_err); return QYMERA_OK; }

    qymera_validation_result_t vres;
    if (qymera_rule_engine_validate(ctx->rule_engine, &r, &vres) != QYMERA_OK || !vres.valid) {
        out_error(o, "RULE_INVALID", vres.error_count ? vres.errors[0] : "rule validation failed");
        return QYMERA_OK;
    }

    qymera_compiled_rule_t compiled;
    qymera_err_t err = qymera_rule_engine_compile(ctx->rule_engine, &r, &compiled);
    if (err != QYMERA_OK) { out_error(o, "RULE_INVALID", "rule compile failed"); return QYMERA_OK; }

    uint16_t slot;
    err = qymera_rule_engine_load(ctx->rule_engine, &compiled, &slot);
    if (err != QYMERA_OK) { out_error(o, "NO_SPACE", "rule table full"); return QYMERA_OK; }

    err = persist_rule(ctx, &compiled, r.enabled);
    if (err != QYMERA_OK) { out_error(o, "RULE_INVALID", "rule persist failed"); return QYMERA_OK; }

    out_add(o, "{\"rule_id\":\"%s\",\"revision\":1,\"enabled\":true,\"activated\":true,\"slot\":%u}",
            r.rule_id, (unsigned)slot);
    out_ok(o);
    return QYMERA_OK;
}

static qymera_err_t skill_update_rule(qymera_skill_context_t *ctx, const qymera_skill_input_t *in,
                                      qymera_skill_output_t *o) {
    if (!in->rule_id[0]) { out_error(o, "INVALID_VALUE", "rule_id required"); return QYMERA_OK; }
    if (!in->name[0]) { out_error(o, "INVALID_VALUE", "rule name required"); return QYMERA_OK; }

    rule_lookup_t rl = find_rule(ctx, in->rule_id);
    if (!rl.found) { out_error(o, "RULE_INVALID", "rule not found"); return QYMERA_OK; }

    qymera_rule_t r;
    memset(&r, 0, sizeof(r));
    strncpy(r.rule_id, in->rule_id, sizeof(r.rule_id) - 1);
    strncpy(r.name, in->name, sizeof(r.name) - 1);
    r.enabled = rl.compiled.rule.enabled;
    r.revision = rl.compiled.rule.revision + 1;
    r.created_ts = rl.compiled.rule.created_ts;
    r.updated_ts = qymera_timestamp_now().seconds;

    r.trigger = in->rule.trigger;
    r.condition_count = in->rule.condition_count < QYMERA_MAX_CONDITIONS ? in->rule.condition_count : QYMERA_MAX_CONDITIONS;
    for (uint8_t i = 0; i < r.condition_count; i++) r.conditions[i] = in->rule.conditions[i];
    r.action_count = in->rule.action_count < QYMERA_MAX_ACTIONS ? in->rule.action_count : QYMERA_MAX_ACTIONS;
    for (uint8_t i = 0; i < r.action_count; i++) r.actions[i] = in->rule.actions[i];
    r.cooldown_ms = in->rule.cooldown_ms;
    r.max_activations_per_hour = in->rule.max_activations_per_hour;
    r.priority = in->rule.priority;

    char err_code[QYMERA_SKILL_ERROR_CODE_LEN];
    const char *ref_err = validate_rule_refs(ctx, &r, err_code, sizeof(err_code));
    if (ref_err) { out_error(o, err_code, ref_err); return QYMERA_OK; }

    qymera_validation_result_t vres;
    if (qymera_rule_engine_validate(ctx->rule_engine, &r, &vres) != QYMERA_OK || !vres.valid) {
        out_error(o, "RULE_INVALID", vres.error_count ? vres.errors[0] : "rule validation failed");
        return QYMERA_OK;
    }

    qymera_compiled_rule_t compiled;
    if (qymera_rule_engine_compile(ctx->rule_engine, &r, &compiled) != QYMERA_OK) {
        out_error(o, "RULE_INVALID", "rule compile failed"); return QYMERA_OK;
    }

    qymera_rule_engine_unload(ctx->rule_engine, rl.slot);

    uint16_t slot;
    if (qymera_rule_engine_load(ctx->rule_engine, &compiled, &slot) != QYMERA_OK) {
        out_error(o, "NO_SPACE", "rule table full"); return QYMERA_OK;
    }
    if (persist_rule(ctx, &compiled, r.enabled) != QYMERA_OK) {
        out_error(o, "RULE_INVALID", "rule persist failed"); return QYMERA_OK;
    }

    out_add(o, "{\"rule_id\":\"%s\",\"revision\":%u,\"enabled\":%s,\"updated\":true}",
            r.rule_id, (unsigned)r.revision, r.enabled ? "true" : "false");
    out_ok(o);
    return QYMERA_OK;
}

static qymera_err_t skill_delete_rule(qymera_skill_context_t *ctx, const qymera_skill_input_t *in,
                                      qymera_skill_output_t *o) {
    if (!in->rule_id[0]) { out_error(o, "INVALID_VALUE", "rule_id required"); return QYMERA_OK; }
    rule_lookup_t rl = find_rule(ctx, in->rule_id);
    if (!rl.found) { out_error(o, "RULE_INVALID", "rule not found"); return QYMERA_OK; }
    qymera_rule_engine_unload(ctx->rule_engine, rl.slot);
    qymera_storage_delete_rule(ctx->storage, in->rule_id);
    out_add(o, "{\"rule_id\":\"%s\",\"deleted\":true}", in->rule_id);
    out_ok(o);
    return QYMERA_OK;
}

static qymera_err_t skill_set_rule_enabled(qymera_skill_context_t *ctx, const qymera_skill_input_t *in,
                                           bool enabled, qymera_skill_output_t *o) {
    if (!in->rule_id[0]) { out_error(o, "INVALID_VALUE", "rule_id required"); return QYMERA_OK; }
    rule_lookup_t rl = find_rule(ctx, in->rule_id);
    if (!rl.found) { out_error(o, "RULE_INVALID", "rule not found"); return QYMERA_OK; }
    qymera_rule_engine_set_enabled(ctx->rule_engine, rl.slot, enabled);
    /* Persist the new enabled flag. */
    qymera_compiled_rule_t compiled = rl.compiled;
    compiled.rule.enabled = enabled;
    if (persist_rule(ctx, &compiled, enabled) != QYMERA_OK) {
        out_error(o, "RULE_INVALID", "rule persist failed"); return QYMERA_OK;
    }
    out_add(o, "{\"rule_id\":\"%s\",\"enabled\":%s}", in->rule_id, enabled ? "true" : "false");
    out_ok(o);
    return QYMERA_OK;
}

/* =========================
 * Dispatcher
 * ========================= */

static bool list_rules_cb(uint16_t idx, const qymera_compiled_rule_t *cr, void *context) {
    (void)idx;
    qymera_skill_output_t *o = context;
    const char *sep = o->data_len > 1 ? "," : "";
    out_add(o, "%s{\"rule_id\":\"%s\",\"name\":\"%s\",\"enabled\":%s,\"revision\":%u}",
            sep, cr->rule.rule_id, cr->rule.name, cr->rule.enabled ? "true" : "false",
            (unsigned)cr->rule.revision);
    return true;
}

qymera_err_t qymera_skill_execute(qymera_skill_context_t *ctx,
                                  const char *skill_name,
                                  const qymera_skill_input_t *input,
                                  qymera_skill_output_t *output,
                                  uint32_t permission_mask) {
    if (!ctx || !output || !skill_name) return QYMERA_ERR_INVALID_ARG;
    out_clear(output);

    qymera_skill_id_t id = qymera_skill_lookup(skill_name);
    if (id == (qymera_skill_id_t)-1) {
        out_error(output, "SKILL_NOT_FOUND", "unknown skill name");
        return QYMERA_OK;
    }
    const qymera_skill_entry_t *entry = &skills[id];
    if ((permission_mask & entry->meta.permissions) != entry->meta.permissions) {
        out_error(output, "PERMISSION_DENIED", "insufficient permission for this skill");
        return QYMERA_OK;
    }

    static const qymera_skill_input_t empty_input;
    if (!input) input = &empty_input;

    switch (id) {
        case QYMERA_SKILL_DEVICES_LIST:    return skill_list_devices(ctx, output);
        case QYMERA_SKILL_ENTITIES_LIST:   return skill_list_entities(ctx, output);
        case QYMERA_SKILL_ENTITY_STATE_GET:return skill_get_entity_state(ctx, input, output);
        case QYMERA_SKILL_ENTITY_INFO_GET: return skill_get_entity_info(ctx, input, output);
        case QYMERA_SKILL_RELAY_SET:       return skill_set_relay(ctx, input, output);
        case QYMERA_SKILL_DIMMER_SET:      return skill_set_dimmer(ctx, input, output);
        case QYMERA_SKILL_RULES_LIST:      return skill_list_rules(ctx, output);
        case QYMERA_SKILL_RULE_GET:        return skill_get_rule(ctx, input, output);
        case QYMERA_SKILL_RULE_CREATE:     return skill_create_rule(ctx, input, output);
        case QYMERA_SKILL_RULE_UPDATE:     return skill_update_rule(ctx, input, output);
        case QYMERA_SKILL_RULE_DELETE:     return skill_delete_rule(ctx, input, output);
        case QYMERA_SKILL_RULE_ENABLE:     return skill_set_rule_enabled(ctx, input, true, output);
        case QYMERA_SKILL_RULE_DISABLE:    return skill_set_rule_enabled(ctx, input, false, output);
        default: out_error(output, "SKILL_NOT_FOUND", "unknown skill");
    }
    return QYMERA_OK;
}

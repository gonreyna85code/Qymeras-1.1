/**
 * Qymera Dashboard - Deterministic Skill API implementation.
 *
 * Deterministic, bounded, caller-agnostic surface over Registry / Rule Engine /
 * Control API / Storage. No natural-language parsing, no inference, no direct
 * GPIO/UDP — the Skill layer never touches the transport or hardware directly.
 *
 * Hardened machine-protocol guarantees (Phase 3B):
 *   - Every success payload is exactly one valid JSON value (object or array).
 *   - Every string inserted into JSON output is escaped (quotes, backslash,
 *     control characters, \n \r \t \b \f, and other controls as \uXXXX).
 *   - If a result would exceed the output buffer it becomes an
 *     OUTPUT_TOO_LARGE error (ok=false); it is never truncated malformed JSON.
 *   - Rule mutations are transactional: runtime and durable storage always
 *     agree; on any failure the previous state remains intact.
 */
#include "qymera_skill.h"
#include "qymera_log.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* =========================
 * Output builder
 *
 * State machine over the bounded output buffer: accumulating JSON while OK,
 * then resolving to either OK (ok=true, valid JSON in data) or ERROR
 * (ok=false, error fields set, data cleared). Overflow is resolved to
 * OUTPUT_TOO_LARGE, never left as truncated JSON.
 * ========================= */

static void out_clear(qymera_skill_output_t *o) {
    memset(o, 0, sizeof(*o));
    o->ok = false;
    o->truncated = false;
    o->data[0] = '\0';
}

/* Append a formatted JSON fragment to the bounded output buffer.
 * On overflow sets `truncated` and stops appending. */
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

/* Append a JSON string literal (with surrounding quotes) to the buffer,
 * escaping every byte that JSON requires. Never emits an unescaped string. */
static void out_str_json(qymera_skill_output_t *o, const char *s) {
    if (!s) s = "";
    out_add(o, "\"");
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        unsigned char c = *p++;
        switch (c) {
            case '"':  out_add(o, "\\\""); break;
            case '\\': out_add(o, "\\\\"); break;
            case '/':  out_add(o, "\\/");  break;
            case '\b': out_add(o, "\\b");  break;
            case '\f': out_add(o, "\\f");  break;
            case '\n': out_add(o, "\\n");  break;
            case '\r': out_add(o, "\\r");  break;
            case '\t': out_add(o, "\\t");  break;
            default:
                if (c < 0x20) out_add(o, "\\u%04x", (unsigned)c);
                else          out_add(o, "%c", (int)c);
        }
    }
    out_add(o, "\"");
}

/* Declare a failure. Clears any partial JSON and sets stable error fields. */
static void out_error(qymera_skill_output_t *o, const char *code, const char *msg) {
    o->ok = false;
    o->truncated = false;
    o->data_len = 0;
    o->data[0] = '\0';
    snprintf(o->error_code, sizeof(o->error_code), "%s", code);
    snprintf(o->message, sizeof(o->message), "%s", msg);
}

/* Declare success. If overflow happened, resolve to OUTPUT_TOO_LARGE instead
 * of returning truncated/malformed JSON. */
static void out_ok(qymera_skill_output_t *o) {
    if (o->truncated) {
        o->ok = false;
        o->truncated = false;
        o->data_len = 0;
        o->data[0] = '\0';
        snprintf(o->error_code, sizeof(o->error_code), QYMERA_SKILL_ERR_OUTPUT_TOO_LARGE);
        snprintf(o->message, sizeof(o->message), "result exceeds skill output limit");
        return;
    }
    o->ok = true;
    o->error_code[0] = '\0';
    o->message[0] = '\0';
}

/* Map an underlying qymera_err_t to a stable machine code (never raw enums or
 * arbitrary internal strings). */
static void set_err_from(qymera_skill_output_t *o, qymera_err_t e, const char *def_code,
                         const char *def_msg) {
    switch (e) {
        case QYMERA_ERR_NOT_FOUND:   out_error(o, QYMERA_SKILL_ERR_ENTITY_NOT_FOUND, def_msg); break;
        case QYMERA_ERR_NO_SPACE:    out_error(o, QYMERA_SKILL_ERR_NO_SPACE, "no space available"); break;
        case QYMERA_ERR_NETWORK:     out_error(o, QYMERA_SKILL_ERR_DEVICE_OFFLINE, "command transport unavailable"); break;
        case QYMERA_ERR_TIMEOUT:     out_error(o, QYMERA_SKILL_ERR_COMMAND_TIMEOUT, "command timed out"); break;
        case QYMERA_ERR_INVALID_CAPABILITY: out_error(o, QYMERA_SKILL_ERR_INVALID_CAPABILITY, def_msg); break;
        case QYMERA_ERR_INVALID_ARG: out_error(o, QYMERA_SKILL_ERR_INVALID_INPUT, def_msg); break;
        case QYMERA_ERR_STORAGE:     out_error(o, QYMERA_SKILL_ERR_STORAGE_ERROR, def_msg); break;
        default:                     out_error(o, def_code, def_msg); break;
    }
}

/* Null-dependency gate. Returns true (and emits a stable error) if any required
 * dependency is missing, so handlers never dereference NULL. */
static bool dep_required(qymera_skill_context_t *ctx, qymera_skill_output_t *o,
                         bool need_registry, bool need_rule, bool need_control,
                         bool need_storage) {
    const char *which = NULL;
    if (need_registry && !ctx->registry)      which = "registry";
    else if (need_rule && !ctx->rule_engine)  which = "rule engine";
    else if (need_control && !ctx->control)   which = "control";
    else if (need_storage && !ctx->storage)   which = "storage";
    if (which) {
        out_error(o, QYMERA_SKILL_ERR_DEPENDENCY_MISSING, which);
        return true;
    }
    return false;
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
    if (idx >= QYMERA_MAX_SKILLS || !entry) return (qymera_skill_id_t)-1;
    *entry = &skills[idx];
    return skills[idx].id;
}

qymera_skill_id_t qymera_skill_lookup(const char *name) {
    if (!name) return (qymera_skill_id_t)-1;
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
    if (o->data_len > 1) out_add(o, ",");
    out_add(o, "{\"device_id\":");
    out_str_json(o, d->device_id);
    out_add(o, ",\"name\":");
    out_str_json(o, d->name);
    out_add(o, ",\"model\":");
    out_str_json(o, d->model);
    out_add(o, ",\"role\":");
    out_str_json(o, device_role_str(d->role));
    out_add(o, ",\"online\":%s,\"state\":",
            d->online ? "true" : "false");
    out_str_json(o, device_state_str(d->state));
    out_add(o, ",\"location\":");
    out_str_json(o, d->location);
    out_add(o, "}");
    return true;
}

static qymera_err_t skill_list_devices(qymera_skill_context_t *ctx, qymera_skill_output_t *o) {
    if (dep_required(ctx, o, true, false, false, false)) return QYMERA_OK;
    out_add(o, "[");
    qymera_registry_iterate_devices(ctx->registry, device_list_cb, o);
    out_add(o, "]");
    out_ok(o);
    return QYMERA_OK;
}

static bool entity_list_cb(uint16_t idx, const qymera_entity_t *e, void *context) {
    (void)idx;
    qymera_skill_output_t *o = context;
    if (o->data_len > 1) out_add(o, ",");
    out_add(o, "{\"device_id\":");
    out_str_json(o, e->device_id);
    out_add(o, ",\"entity_id\":");
    out_str_json(o, e->entity_id);
    out_add(o, ",\"name\":");
    out_str_json(o, e->name);
    out_add(o, ",\"type\":");
    out_str_json(o, entity_type_str(e->type));
    out_add(o, ",\"capabilities\":[");
    for (uint8_t i = 0; i < e->capability_count && i < 4; i++) {
        if (i) out_add(o, ",");
        out_str_json(o, capability_str(e->capabilities[i]));
    }
    out_add(o, "],\"unit\":");
    out_str_json(o, e->unit);
    bool is_relay = entity_is_relay(e), is_dimmer = entity_is_dimmer(e);
    if (is_relay) {
        out_add(o, ",\"current\":%s,\"desired\":%s,\"cmd_status\":",
                e->value.bool_value ? "true" : "false",
                e->desired.bool_value ? "true" : "false");
        out_str_json(o, qymera_skill_cmd_status_str(e->cmd_status));
        out_add(o, ",\"reliability\":");
        out_str_json(o, qymera_skill_reliability_str(e->value.reliability, e->cmd_status));
    } else {
        out_add(o, ",\"current\":%g,\"desired\":%g,\"cmd_status\":",
                (double)e->value.numeric_value, (double)e->desired.numeric_value);
        out_str_json(o, qymera_skill_cmd_status_str(e->cmd_status));
        out_add(o, ",\"reliability\":");
        out_str_json(o, qymera_skill_reliability_str(e->value.reliability, e->cmd_status));
    }
    out_add(o, "}");
    return true;
}

static qymera_err_t skill_list_entities(qymera_skill_context_t *ctx, qymera_skill_output_t *o) {
    if (dep_required(ctx, o, true, false, false, false)) return QYMERA_OK;
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
    if (dep_required(ctx, o, true, false, false, false)) return QYMERA_OK;
    uint16_t eidx; qymera_entity_t e;
    qymera_err_t err = lookup_entity(ctx, in, &eidx, &e);
    (void)eidx;
    if (err != QYMERA_OK) { out_error(o, QYMERA_SKILL_ERR_ENTITY_NOT_FOUND, "entity not found"); return QYMERA_OK; }

    out_add(o, "{\"device_id\":");
    out_str_json(o, e.device_id);
    out_add(o, ",\"entity_id\":");
    out_str_json(o, e.entity_id);
    if (entity_is_relay(&e)) {
        out_add(o, ",\"observed\":%s,\"desired\":%s",
                e.value.bool_value ? "true" : "false", e.desired.bool_value ? "true" : "false");
    } else {
        out_add(o, ",\"observed\":%g,\"desired\":%g",
                (double)e.value.numeric_value, (double)e.desired.numeric_value);
    }
    out_add(o, ",\"status\":");
    out_str_json(o, qymera_skill_cmd_status_str(e.cmd_status));
    out_add(o, ",\"reliability\":");
    out_str_json(o, qymera_skill_reliability_str(e.value.reliability, e.cmd_status));
    out_add(o, ",\"timestamp\":%u}", (unsigned)e.last_updated.seconds);
    out_ok(o);
    return QYMERA_OK;
}

static qymera_err_t skill_get_entity_info(qymera_skill_context_t *ctx, const qymera_skill_input_t *in,
                                          qymera_skill_output_t *o) {
    if (dep_required(ctx, o, true, false, false, false)) return QYMERA_OK;
    uint16_t eidx; qymera_entity_t e;
    qymera_err_t err = lookup_entity(ctx, in, &eidx, &e);
    (void)eidx;
    if (err != QYMERA_OK) { out_error(o, QYMERA_SKILL_ERR_ENTITY_NOT_FOUND, "entity not found"); return QYMERA_OK; }

    out_add(o, "{\"device_id\":");
    out_str_json(o, e.device_id);
    out_add(o, ",\"entity_id\":");
    out_str_json(o, e.entity_id);
    out_add(o, ",\"name\":");
    out_str_json(o, e.name);
    out_add(o, ",\"type\":");
    out_str_json(o, entity_type_str(e.type));
    out_add(o, ",\"capabilities\":[");
    for (uint8_t i = 0; i < e.capability_count && i < 4; i++) {
        if (i) out_add(o, ",");
        out_str_json(o, capability_str(e.capabilities[i]));
    }
    out_add(o, "],\"unit\":");
    out_str_json(o, e.unit);
    out_add(o, ",\"native_min\":%g,\"native_max\":%g,\"calibration_min\":%g,\"calibration_max\":%g,\"correction\":%g,\"persist_state\":%s,\"protected\":%s,\"last_updated\":%u}",
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
    (void)is_dimmer;
    out_add(o, "{\"device_id\":");
    out_str_json(o, device_id);
    out_add(o, ",\"entity_id\":");
    out_str_json(o, entity_id);
    if (is_relay) {
        out_add(o, ",\"requested\":%s,\"observed\":%s,\"desired\":%s",
                e->desired.bool_value ? "true" : "false",
                e->value.bool_value ? "true" : "false",
                e->desired.bool_value ? "true" : "false");
    } else {
        out_add(o, ",\"requested\":%g,\"observed\":%g,\"desired\":%g",
                (double)e->desired.numeric_value,
                (double)e->value.numeric_value,
                (double)e->desired.numeric_value);
    }
    out_add(o, ",\"status\":");
    out_str_json(o, qymera_skill_cmd_status_str(e->cmd_status));
    out_add(o, ",\"reliability\":");
    out_str_json(o, qymera_skill_reliability_str(e->value.reliability, e->cmd_status));
    out_add(o, "}");
}

static qymera_err_t skill_set_relay(qymera_skill_context_t *ctx, const qymera_skill_input_t *in,
                                    qymera_skill_output_t *o) {
    if (dep_required(ctx, o, true, false, true, false)) return QYMERA_OK;
    uint16_t eidx; qymera_entity_t e;
    qymera_err_t err = lookup_entity(ctx, in, &eidx, &e);
    if (err != QYMERA_OK) { out_error(o, QYMERA_SKILL_ERR_ENTITY_NOT_FOUND, "entity not found"); return QYMERA_OK; }
    if (!entity_is_relay(&e)) { out_error(o, QYMERA_SKILL_ERR_INVALID_CAPABILITY, "target is not a relay actuator"); return QYMERA_OK; }

    qymera_entity_ref_t ref;
    strncpy(ref.device_id, in->device_id, sizeof(ref.device_id) - 1);
    ref.device_id[sizeof(ref.device_id) - 1] = '\0';
    strncpy(ref.entity_id, in->entity_id, sizeof(ref.entity_id) - 1);
    ref.entity_id[sizeof(ref.entity_id) - 1] = '\0';
    err = qymera_control_set_relay(ctx->control, &ref, in->value, false);
    if (err != QYMERA_OK) { set_err_from(o, err, QYMERA_SKILL_ERR_STORAGE_ERROR, "relay command failed"); return QYMERA_OK; }

    qymera_registry_get_entity(ctx->registry, eidx, &e);
    append_dispatch_result(o, &e, in->device_id, in->entity_id);
    out_ok(o);
    return QYMERA_OK;
}

static qymera_err_t skill_set_dimmer(qymera_skill_context_t *ctx, const qymera_skill_input_t *in,
                                     qymera_skill_output_t *o) {
    if (in->level > 100) { out_error(o, QYMERA_SKILL_ERR_INVALID_VALUE, "dimmer level must be 0-100"); return QYMERA_OK; }
    if (dep_required(ctx, o, true, false, true, false)) return QYMERA_OK;
    uint16_t eidx; qymera_entity_t e;
    qymera_err_t err = lookup_entity(ctx, in, &eidx, &e);
    if (err != QYMERA_OK) { out_error(o, QYMERA_SKILL_ERR_ENTITY_NOT_FOUND, "entity not found"); return QYMERA_OK; }
    if (!entity_is_dimmer(&e)) { out_error(o, QYMERA_SKILL_ERR_INVALID_CAPABILITY, "target is not a dimmer actuator"); return QYMERA_OK; }

    qymera_entity_ref_t ref;
    strncpy(ref.device_id, in->device_id, sizeof(ref.device_id) - 1);
    ref.device_id[sizeof(ref.device_id) - 1] = '\0';
    strncpy(ref.entity_id, in->entity_id, sizeof(ref.entity_id) - 1);
    ref.entity_id[sizeof(ref.entity_id) - 1] = '\0';
    err = qymera_control_set_dimmer(ctx->control, &ref, in->level, false);
    if (err != QYMERA_OK) { set_err_from(o, err, QYMERA_SKILL_ERR_STORAGE_ERROR, "dimmer command failed"); return QYMERA_OK; }

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
    rl.compiled.rule.rule_id[sizeof(rl.compiled.rule.rule_id) - 1] = '\0';
    qymera_rule_engine_list(ctx->rule_engine, rule_find_cb, &rl);
    return rl;
}

/* Collision scan: does a rule id already exist either loaded in the engine or
 * persisted in storage (so IDs remain unique across reboot)? */
typedef struct {
    char id[QYMERA_RULE_ID_LEN];
    bool found;
} rid_scan_t;

static bool rid_scan_cb(uint16_t idx, const qymera_compiled_rule_t *rule, void *context) {
    (void)idx;
    rid_scan_t *sc = context;
    if (strcmp(rule->rule.rule_id, sc->id) == 0) { sc->found = true; return false; }
    return true;
}

static bool rule_id_exists(qymera_skill_context_t *ctx, const char *rid) {
    rid_scan_t sc;
    memset(&sc, 0, sizeof(sc));
    strncpy(sc.id, rid, sizeof(sc.id) - 1);
    sc.id[sizeof(sc.id) - 1] = '\0';
    qymera_rule_engine_list(ctx->rule_engine, rid_scan_cb, &sc);
    if (sc.found) return true;
    if (ctx->storage) {
        qymera_rules_index_t idx;
        if (qymera_storage_load_rules_index(ctx->storage, &idx) == QYMERA_OK) {
            for (uint32_t i = 0; i < idx.count; i++) {
                if (strcmp(idx.rules[i].rule_id, rid) == 0) return true;
            }
        }
    }
    return false;
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
 * Returns the first failure and writes its stable machine code into err_code. */
static const char *validate_rule_refs(qymera_skill_context_t *ctx, const qymera_rule_t *r,
                                      char *err_code, size_t err_code_sz) {
    uint16_t idx;
    qymera_entity_t e;

    if (r->trigger.operator_ != QYMERA_OP_NONE) {
        if (qymera_registry_find_entity_by_ref(ctx->registry, &r->trigger.entity, &idx) != QYMERA_OK) {
            snprintf(err_code, err_code_sz, "%s", QYMERA_SKILL_ERR_ENTITY_NOT_FOUND);
            return "trigger references a nonexistent entity";
        }
        if (!op_valid(r->trigger.operator_)) { snprintf(err_code, err_code_sz, "%s", QYMERA_SKILL_ERR_RULE_INVALID); return "invalid trigger operator"; }
    }
    for (uint8_t i = 0; i < r->condition_count; i++) {
        const qymera_condition_t *c = &r->conditions[i];
        if (qymera_registry_find_entity_by_ref(ctx->registry, &c->entity, &idx) != QYMERA_OK) {
            snprintf(err_code, err_code_sz, "%s", QYMERA_SKILL_ERR_ENTITY_NOT_FOUND);
            return "condition references a nonexistent entity";
        }
        if (!op_valid(c->operator_)) { snprintf(err_code, err_code_sz, "%s", QYMERA_SKILL_ERR_RULE_INVALID); return "invalid condition operator"; }
    }
    for (uint8_t i = 0; i < r->action_count; i++) {
        const qymera_action_t *a = &r->actions[i];
        if (qymera_registry_find_entity_by_ref(ctx->registry, &a->entity, &idx) != QYMERA_OK) {
            snprintf(err_code, err_code_sz, "%s", QYMERA_SKILL_ERR_ENTITY_NOT_FOUND);
            return "action references a nonexistent entity";
        }
        qymera_capability_t cap = action_required_cap(a->action);
        if (cap == QYMERA_CAP_NONE) { snprintf(err_code, err_code_sz, "%s", QYMERA_SKILL_ERR_RULE_INVALID); return "invalid action type"; }
        if (qymera_registry_get_entity(ctx->registry, idx, &e) != QYMERA_OK) {
            snprintf(err_code, err_code_sz, "%s", QYMERA_SKILL_ERR_ENTITY_NOT_FOUND);
            return "action entity unavailable";
        }
        if (!entity_has_cap(&e, cap)) {
            snprintf(err_code, err_code_sz, "%s", QYMERA_SKILL_ERR_RULE_INVALID);
            return "action incompatible with target capability";
        }
    }
    return NULL;
}

/* JSON-escape an entity reference and append it as a JSON object. */
static void emit_ref_json(qymera_skill_output_t *o, const qymera_entity_ref_t *ref) {
    out_add(o, "{\"device_id\":");
    out_str_json(o, ref->device_id);
    out_add(o, ",\"entity_id\":");
    out_str_json(o, ref->entity_id);
    out_add(o, "}");
}

static void emit_trigger_condition(qymera_skill_output_t *o, const qymera_condition_t *c) {
    const char *sep = o->data_len > 1 ? "," : "";
    out_add(o, "%s{\"entity\":", sep);
    emit_ref_json(o, &c->entity);
    out_add(o, ",\"operator\":");
    out_str_json(o, operator_str(c->operator_));
    out_add(o, ",\"threshold\":%g,\"threshold_high\":%g,\"duration_ms\":%u,\"negate\":%s}",
            (double)c->threshold, (double)c->threshold_high,
            (unsigned)c->duration_ms, c->negate ? "true" : "false");
}

static void emit_action(qymera_skill_output_t *o, const qymera_action_t *a) {
    const char *sep = o->data_len > 1 ? "," : "";
    out_add(o, "%s{\"entity\":", sep);
    emit_ref_json(o, &a->entity);
    out_add(o, ",\"action\":");
    out_str_json(o, action_str(a->action));
    out_add(o, ",\"value\":%g,\"duration_ms\":%u}",
            (double)a->value_f, (unsigned)a->duration_ms);
}

static bool list_rules_cb(uint16_t idx, const qymera_compiled_rule_t *cr, void *context) {
    (void)idx;
    qymera_skill_output_t *o = context;
    if (o->data_len > 1) out_add(o, ",");
    out_add(o, "{\"rule_id\":");
    out_str_json(o, cr->rule.rule_id);
    out_add(o, ",\"name\":");
    out_str_json(o, cr->rule.name);
    out_add(o, ",\"enabled\":%s,\"revision\":%u}",
            cr->rule.enabled ? "true" : "false", (unsigned)cr->rule.revision);
    return true;
}

static qymera_err_t skill_list_rules(qymera_skill_context_t *ctx, qymera_skill_output_t *o) {
    if (dep_required(ctx, o, false, true, false, false)) return QYMERA_OK;
    out_add(o, "[");
    qymera_rule_engine_list(ctx->rule_engine, list_rules_cb, o);
    out_add(o, "]");
    out_ok(o);
    return QYMERA_OK;
}

static qymera_err_t skill_get_rule(qymera_skill_context_t *ctx, const qymera_skill_input_t *in,
                                   qymera_skill_output_t *o) {
    if (dep_required(ctx, o, false, true, false, false)) return QYMERA_OK;
    if (!in->rule_id[0]) { out_error(o, QYMERA_SKILL_ERR_INVALID_VALUE, "rule_id required"); return QYMERA_OK; }
    rule_lookup_t rl = find_rule(ctx, in->rule_id);
    if (!rl.found) { out_error(o, QYMERA_SKILL_ERR_RULE_INVALID, "rule not found"); return QYMERA_OK; }
    const qymera_rule_t *r = &rl.compiled.rule;
    const qymera_rule_state_t *st = &rl.compiled.state;

    out_add(o, "{\"rule_id\":");
    out_str_json(o, r->rule_id);
    out_add(o, ",\"name\":");
    out_str_json(o, r->name);
    out_add(o, ",\"enabled\":%s,\"revision\":%u,\"priority\":%u,\"cooldown_ms\":%u,\"max_activations_per_hour\":%u,\"created_ts\":%u,\"updated_ts\":%u,\"state\":{\"activation_count\":%u,\"last_triggered\":%u},\"trigger\":[",
            r->enabled ? "true" : "false", (unsigned)r->revision,
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
static void gen_rule_id(char *dst, size_t sz, uint32_t seq) {
    snprintf(dst, sz, "rule_%u", (unsigned)seq);
}

static qymera_err_t persist_rule(qymera_skill_context_t *ctx, qymera_compiled_rule_t *compiled,
                                 bool enabled) {
    qymera_rule_meta_t meta;
    memset(&meta, 0, sizeof(meta));
    strncpy(meta.rule_id, compiled->rule.rule_id, sizeof(meta.rule_id) - 1);
    meta.rule_id[sizeof(meta.rule_id) - 1] = '\0';
    meta.revision = compiled->rule.revision;
    meta.created_ts = compiled->rule.created_ts;
    meta.updated_ts = compiled->rule.updated_ts;
    meta.enabled = enabled;
    meta.compiled_size = sizeof(qymera_compiled_rule_t);
    meta.checksum = compiled->checksum;
    return qymera_storage_save_rule(ctx->storage, compiled->rule.rule_id, compiled,
                                    sizeof(qymera_compiled_rule_t), &meta);
}

/* Build a rule from structured input for create/update. The caller provides
 * rule_id (pre-selected), name, and the authoring body fields from `in`. */
static void build_rule(qymera_rule_t *r, const char *rule_id, const char *name,
                       bool enabled, uint32_t revision, uint32_t created_ts, uint32_t updated_ts,
                       const qymera_skill_input_t *in) {
    memset(r, 0, sizeof(*r));
    strncpy(r->rule_id, rule_id, sizeof(r->rule_id) - 1);
    r->rule_id[sizeof(r->rule_id) - 1] = '\0';
    strncpy(r->name, name, sizeof(r->name) - 1);
    r->name[sizeof(r->name) - 1] = '\0';
    r->enabled = enabled;
    r->revision = revision;
    r->created_ts = created_ts;
    r->updated_ts = updated_ts;

    r->trigger = in->rule.trigger;
    r->condition_count = in->rule.condition_count < QYMERA_MAX_CONDITIONS ? in->rule.condition_count : QYMERA_MAX_CONDITIONS;
    for (uint8_t i = 0; i < r->condition_count; i++) r->conditions[i] = in->rule.conditions[i];
    r->action_count = in->rule.action_count < QYMERA_MAX_ACTIONS ? in->rule.action_count : QYMERA_MAX_ACTIONS;
    for (uint8_t i = 0; i < r->action_count; i++) r->actions[i] = in->rule.actions[i];
    r->cooldown_ms = in->rule.cooldown_ms;
    r->max_activations_per_hour = in->rule.max_activations_per_hour;
    r->priority = in->rule.priority;
}

/* Shared prepare step used by both create and update. Returns a qymera_err_t
 * and emits the matching stable error into `o` on failure. On success the
 * caller receives a valid compiled rule in `out`. */
static qymera_err_t prepare_compiled(qymera_skill_context_t *ctx, const qymera_rule_t *r,
                                     qymera_compiled_rule_t *out, qymera_skill_output_t *o) {
    char err_code[QYMERA_SKILL_ERROR_CODE_LEN];
    const char *ref_err = validate_rule_refs(ctx, r, err_code, sizeof(err_code));
    if (ref_err) { out_error(o, err_code, ref_err); return QYMERA_ERR_INVALID_ARG; }

    qymera_validation_result_t vres;
    if (qymera_rule_engine_validate(ctx->rule_engine, r, &vres) != QYMERA_OK || !vres.valid) {
        out_error(o, QYMERA_SKILL_ERR_RULE_INVALID,
                  vres.error_count ? vres.errors[0] : "rule validation failed");
        return QYMERA_ERR_INVALID_ARG;
    }
    if (qymera_rule_engine_compile(ctx->rule_engine, r, out) != QYMERA_OK) {
        out_error(o, QYMERA_SKILL_ERR_RULE_INVALID, "rule compile failed");
        return QYMERA_ERR_INVALID_ARG;
    }
    return QYMERA_OK;
}

static qymera_err_t skill_create_rule(qymera_skill_context_t *ctx, const qymera_skill_input_t *in,
                                      qymera_skill_output_t *o) {
    if (dep_required(ctx, o, true, true, false, true)) return QYMERA_OK;
    if (!in->name[0]) { out_error(o, QYMERA_SKILL_ERR_INVALID_VALUE, "rule name required"); return QYMERA_OK; }

    char rule_id[QYMERA_RULE_ID_LEN];
    memset(rule_id, 0, sizeof(rule_id));
    if (in->rule_id[0]) {
        strncpy(rule_id, in->rule_id, sizeof(rule_id) - 1);
        rule_id[sizeof(rule_id) - 1] = '\0';
        if (rule_id_exists(ctx, rule_id)) {
            out_error(o, QYMERA_SKILL_ERR_RULE_CONFLICT, "rule id already exists");
            return QYMERA_OK;
        }
    } else {
        uint32_t guard = 0;
        do {
            gen_rule_id(rule_id, sizeof(rule_id), s_rule_seq);
            s_rule_seq++;
        } while (rule_id_exists(ctx, rule_id) && ++guard < 256);
        if (guard >= 256) { out_error(o, QYMERA_SKILL_ERR_NO_SPACE, "could not allocate a unique rule id"); return QYMERA_OK; }
    }

    uint32_t now = qymera_timestamp_now().seconds;
    qymera_rule_t r;
    build_rule(&r, rule_id, in->name, true, 1, now, now, in);

    qymera_compiled_rule_t compiled;
    if (prepare_compiled(ctx, &r, &compiled, o) != QYMERA_OK) return QYMERA_OK;

    /* Transactional: persist FIRST (durable), then activate. Never leave an
     * active rule that is not durable. */
    if (persist_rule(ctx, &compiled, true) != QYMERA_OK) {
        out_error(o, QYMERA_SKILL_ERR_STORAGE_ERROR, "rule persist failed");
        return QYMERA_OK;
    }
    uint16_t slot;
    if (qymera_rule_engine_load(ctx->rule_engine, &compiled, &slot) != QYMERA_OK) {
        qymera_storage_delete_rule(ctx->storage, rule_id); /* rollback durable rule */
        out_error(o, QYMERA_SKILL_ERR_NO_SPACE, "rule table full");
        return QYMERA_OK;
    }

    out_add(o, "{\"rule_id\":");
    out_str_json(o, rule_id);
    out_add(o, ",\"revision\":1,\"enabled\":true,\"activated\":true,\"slot\":%u}", (unsigned)slot);
    out_ok(o);
    return QYMERA_OK;
}

static qymera_err_t skill_update_rule(qymera_skill_context_t *ctx, const qymera_skill_input_t *in,
                                      qymera_skill_output_t *o) {
    if (dep_required(ctx, o, true, true, false, true)) return QYMERA_OK;
    if (!in->rule_id[0]) { out_error(o, QYMERA_SKILL_ERR_INVALID_VALUE, "rule_id required"); return QYMERA_OK; }
    if (!in->name[0]) { out_error(o, QYMERA_SKILL_ERR_INVALID_VALUE, "rule name required"); return QYMERA_OK; }

    rule_lookup_t rl = find_rule(ctx, in->rule_id);
    if (!rl.found) { out_error(o, QYMERA_SKILL_ERR_RULE_INVALID, "rule not found"); return QYMERA_OK; }

    uint32_t now = qymera_timestamp_now().seconds;
    qymera_rule_t r;
    build_rule(&r, in->rule_id, in->name, rl.compiled.rule.enabled,
               rl.compiled.rule.revision + 1, rl.compiled.rule.created_ts, now, in);

    qymera_compiled_rule_t compiled;
    if (prepare_compiled(ctx, &r, &compiled, o) != QYMERA_OK) return QYMERA_OK;

    /* Transactional update: old rule stays active while we prepare + persist
     * the replacement, then we activate the new rule and finally remove the
     * old runtime state. On any failure the old rule remains intact. */
    if (persist_rule(ctx, &compiled, r.enabled) != QYMERA_OK) {
        out_error(o, QYMERA_SKILL_ERR_STORAGE_ERROR, "rule persist failed");
        return QYMERA_OK; /* old rule intact */
    }

    uint16_t new_slot;
    if (qymera_rule_engine_load(ctx->rule_engine, &compiled, &new_slot) != QYMERA_OK) {
        qymera_storage_delete_rule(ctx->storage, in->rule_id); /* restore durable old definition */
        out_error(o, QYMERA_SKILL_ERR_NO_SPACE, "rule table full");
        return QYMERA_OK; /* old rule intact (persisted replacement rolled back) */
    }

    /* Old rule still active alongside replacement until we drop it. Removing
     * old runtime state fully cleans subscriptions/timers/state. */
    qymera_rule_engine_unload(ctx->rule_engine, rl.slot);

    out_add(o, "{\"rule_id\":");
    out_str_json(o, in->rule_id);
    out_add(o, ",\"revision\":%u,\"enabled\":%s,\"updated\":true}",
            (unsigned)r.revision, r.enabled ? "true" : "false");
    out_ok(o);
    return QYMERA_OK;
}

static qymera_err_t skill_delete_rule(qymera_skill_context_t *ctx, const qymera_skill_input_t *in,
                                      qymera_skill_output_t *o) {
    if (dep_required(ctx, o, false, true, false, true)) return QYMERA_OK;
    if (!in->rule_id[0]) { out_error(o, QYMERA_SKILL_ERR_INVALID_VALUE, "rule_id required"); return QYMERA_OK; }
    rule_lookup_t rl = find_rule(ctx, in->rule_id);
    if (!rl.found) { out_error(o, QYMERA_SKILL_ERR_RULE_INVALID, "rule not found"); return QYMERA_OK; }

    /* Remove durable first; only drop runtime after storage confirms, so the
     * invariant "runtime active <-> durable exists" is preserved on failure. */
    if (qymera_storage_delete_rule(ctx->storage, in->rule_id) != QYMERA_OK) {
        out_error(o, QYMERA_SKILL_ERR_STORAGE_ERROR, "rule delete persist failed");
        return QYMERA_OK; /* runtime untouched */
    }
    qymera_rule_engine_unload(ctx->rule_engine, rl.slot); /* cleans subs/timers/state */

    out_add(o, "{\"rule_id\":");
    out_str_json(o, in->rule_id);
    out_add(o, ",\"deleted\":true}");
    out_ok(o);
    return QYMERA_OK;
}

static qymera_err_t skill_set_rule_enabled(qymera_skill_context_t *ctx, const qymera_skill_input_t *in,
                                           bool enabled, qymera_skill_output_t *o) {
    if (dep_required(ctx, o, false, true, false, true)) return QYMERA_OK;
    if (!in->rule_id[0]) { out_error(o, QYMERA_SKILL_ERR_INVALID_VALUE, "rule_id required"); return QYMERA_OK; }
    rule_lookup_t rl = find_rule(ctx, in->rule_id);
    if (!rl.found) { out_error(o, QYMERA_SKILL_ERR_RULE_INVALID, "rule not found"); return QYMERA_OK; }

    /* Persist the new enabled flag FIRST; only change runtime after storage
     * confirms, so a persist failure preserves the previous runtime state. */
    qymera_compiled_rule_t compiled = rl.compiled;
    compiled.rule.enabled = enabled;
    if (persist_rule(ctx, &compiled, enabled) != QYMERA_OK) {
        out_error(o, QYMERA_SKILL_ERR_STORAGE_ERROR, "rule persist failed");
        return QYMERA_OK; /* runtime state preserved */
    }
    qymera_rule_engine_set_enabled(ctx->rule_engine, rl.slot, enabled);

    out_add(o, "{\"rule_id\":");
    out_str_json(o, in->rule_id);
    out_add(o, ",\"enabled\":%s}", enabled ? "true" : "false");
    out_ok(o);
    return QYMERA_OK;
}

/* =========================
 * Dispatcher
 * ========================= */

qymera_err_t qymera_skill_execute(qymera_skill_context_t *ctx,
                                  const char *skill_name,
                                  const qymera_skill_input_t *input,
                                  qymera_skill_output_t *output,
                                  uint32_t permission_mask) {
    if (!ctx || !output || !skill_name) return QYMERA_ERR_INVALID_ARG;
    out_clear(output);

    qymera_skill_id_t id = qymera_skill_lookup(skill_name);
    if (id == (qymera_skill_id_t)-1) {
        out_error(output, QYMERA_SKILL_ERR_SKILL_NOT_FOUND, "unknown skill name");
        return QYMERA_OK;
    }
    const qymera_skill_entry_t *entry = &skills[id];
    if ((permission_mask & entry->meta.permissions) != entry->meta.permissions) {
        out_error(output, QYMERA_SKILL_ERR_PERMISSION_DENIED, "insufficient permission for this skill");
        return QYMERA_OK;
    }

    /* Deterministic input: zero-initialize every field so the Skill layer never
     * depends on uninitialized bool/uint8/struct values from a partial caller. */
    qymera_skill_input_t in_local;
    memset(&in_local, 0, sizeof(in_local));
    if (input) in_local = *input;
    input = &in_local;

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
        default: out_error(output, QYMERA_SKILL_ERR_SKILL_NOT_FOUND, "unknown skill");
    }
    return QYMERA_OK;
}

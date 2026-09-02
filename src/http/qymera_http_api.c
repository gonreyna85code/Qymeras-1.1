/**
 * Qymera Dashboard - HTTP API implementation
 *
 * All operations are funneled through qymera_skill_execute(). The HTTP
 * layer performs only HTTP / JSON schema validation, then forwards to
 * the Skill layer, which then validates and calls the deterministic
 * runtime (Registry, Rule Engine, Control API, Storage). The HTTP layer
 * NEVER calls GPIO, UDP, Registry mutation, Rule Engine mutation,
 * or storage directly.
 *
 * JSON parsing notes (Phase 3D.1):
 *  - Field lookup is done by KEY inside the document; the parser never
 *    starts reading a typed value from the beginning of the document.
 *  - "value":false / "level":0 are real value reads and are distinct from
 *    an absent field (MISSING) and from a present-but-wrong-type field
 *    (TYPE). Silent "missing -> 0" fallbacks are not used.
 *  - Rule bodies are fully parsed: trigger + actions + cooldown/priority/
 *    max_activations_per_hour populate the structured qymera_rule_t that
 *    build_rule() consumes. No partial/placeholder parsing.
 */

#include "qymera_http_api.h"
#include "qymera_skill.h"
#include "qymera_log.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* =========================
 * Internal: JSON scanner
 * ========================= */

/* A located JSON value: pointer to the value start + its kind. */
typedef enum {
    QJV_NULL, QJV_BOOL, QJV_NUM, QJV_STR, QJV_OBJ, QJV_ARR
} qjv_kind_t;

typedef struct {
    const char *p;
    qjv_kind_t kind;
} qjv_val_t;

static const char *skip_ws(const char *p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}

/* Parse a JSON string at *p; escapes are decoded into out. Returns the
 * pointer just past the closing quote, or NULL on malformed input. */
static const char *parse_string(const char *p, char *out, size_t out_sz) {
    if (*p != '"') return NULL;
    p++;
    size_t i = 0;
    while (*p && *p != '"') {
        if (i >= out_sz - 1) return NULL; /* overflow (bounded) */
        if (*p == '\\') {
            p++;
            if (!*p) return NULL;
            switch (*p) {
                case '"': out[i++] = '"'; break;
                case '\\': out[i++] = '\\'; break;
                case '/': out[i++] = '/'; break;
                case 'b': out[i++] = '\b'; break;
                case 'f': out[i++] = '\f'; break;
                case 'n': out[i++] = '\n'; break;
                case 'r': out[i++] = '\r'; break;
                case 't': out[i++] = '\t'; break;
                case 'u': {
                    /* \uXXXX: we only need to validate the 4 hex digits and
                     * store a replacement char; no surrogate handling. */
                    if (!p[1] || !p[2] || !p[3] || !p[4]) return NULL;
                    for (int k = 1; k <= 4; k++) {
                        char c = p[k];
                        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                              (c >= 'A' && c <= 'F')))
                            return NULL;
                    }
                    p += 4;
                    out[i++] = '?';
                    break;
                }
                default:
                    return NULL; /* invalid escape */
            }
            p++;
        } else {
            out[i++] = *p++;
        }
    }
    if (*p != '"') return NULL;
    out[i] = '\0';
    return p + 1;
}

/* Parse a JSON number at *p into *out. Returns pointer past the number or
 * NULL on malformed input. Accepts the JSON grammar subset:
 * -? (0|[1-9][0-9]*) (.[0-9]+)? ([eE][+-]?[0-9]+)?  */
static const char *parse_number(const char *p, double *out) {
    const char *start = p;
    if (*p == '-') p++;
    if (*p == '0') {
        p++;
    } else if (*p >= '1' && *p <= '9') {
        while (*p >= '0' && *p <= '9') p++;
    } else {
        return NULL;
    }
    if (*p == '.') {
        p++;
        if (!(*p >= '0' && *p <= '9')) return NULL;
        while (*p >= '0' && *p <= '9') p++;
    }
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-') p++;
        if (!(*p >= '0' && *p <= '9')) return NULL;
        while (*p >= '0' && *p <= '9') p++;
    }
    char tmp[32];
    size_t len = (size_t)(p - start);
    if (len == 0 || len >= sizeof(tmp)) return NULL;
    memcpy(tmp, start, len);
    tmp[len] = '\0';
    *out = atof(tmp);
    return p;
}

static const char *parse_bool(const char *p, bool *out) {
    if (strncmp(p, "true", 4) == 0) { *out = true; return p + 4; }
    if (strncmp(p, "false", 5) == 0) { *out = false; return p + 5; }
    return NULL;
}

static const char *skip_value(const char *p, qjv_kind_t *kind);

/* Skip a JSON object value starting at '{'. */
static const char *skip_object(const char *p) {
    p = skip_ws(p + 1);
    if (*p == '}') return p + 1;
    for (;;) {
        p = skip_ws(p);
        if (*p != '"') return NULL;
        char key[64];
        p = parse_string(p, key, sizeof(key));
        if (!p) return NULL;
        p = skip_ws(p);
        if (*p != ':') return NULL;
        p = skip_ws(p + 1);
        p = skip_value(p, NULL);
        if (!p) return NULL;
        p = skip_ws(p);
        if (*p == '}') return p + 1;
        if (*p != ',') return NULL;
        p++;
    }
}

/* Skip a JSON array value starting at '['. */
static const char *skip_array(const char *p) {
    p = skip_ws(p + 1);
    if (*p == ']') return p + 1;
    for (;;) {
        p = skip_ws(p);
        p = skip_value(p, NULL);
        if (!p) return NULL;
        p = skip_ws(p);
        if (*p == ']') return p + 1;
        if (*p != ',') return NULL;
        p++;
    }
}

/* Skip a JSON string value starting at '"' without decoding into a buffer. */
static const char *skip_string(const char *p) {
    if (*p != '"') return NULL;
    p++;
    while (*p && *p != '"') {
        if (*p == '\\') {
            p++;
            if (!*p) return NULL;
            if (*p == 'u') {
                if (!p[1] || !p[2] || !p[3] || !p[4]) return NULL;
                for (int k = 1; k <= 4; k++) {
                    char c = p[k];
                    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                          (c >= 'A' && c <= 'F')))
                        return NULL;
                }
                p += 4;
            }
        }
        p++;
    }
    if (*p != '"') return NULL;
    return p + 1;
}

/* Validate and skip any JSON value; optionally record its kind. */
static const char *skip_value(const char *p, qjv_kind_t *kind) {
    p = skip_ws(p);
    switch (*p) {
        case '{': { if (kind) *kind = QJV_OBJ; return skip_object(p); }
        case '[': { if (kind) *kind = QJV_ARR; return skip_array(p); }
        case '"': { if (kind) *kind = QJV_STR; return skip_string(p); }
        case 't': case 'f': {
            bool b;
            const char *e = parse_bool(p, &b);
            if (!e) return NULL;
            if (kind) *kind = QJV_BOOL;
            return e;
        }
        case 'n': {
            if (strncmp(p, "null", 4) != 0) return NULL;
            if (kind) *kind = QJV_NULL;
            return p + 4;
        }
        case '-': case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9': {
            double d;
            const char *e = parse_number(p, &d);
            if (!e) return NULL;
            if (kind) *kind = QJV_NUM;
            return e;
        }
        default:
            return NULL;
    }
}

/* Validate that the whole body is one well-formed JSON value with nothing
 * trailing after it. */
static bool json_wellformed(const char *body) {
    if (!body) return false;
    qjv_kind_t kind;
    const char *e = skip_value(body, &kind);
    if (!e) return false;
    e = skip_ws(e);
    return *e == '\0';
}

/* Locate the start of `key`'s value inside the object value that starts at
 * `obj`, which must point exactly at '{'. Returns a pointer to the start of
 * the value (right after the member's ':') or NULL if the key is not a member
 * of this object. On success *kind is the JSON kind of the located value. */
static const char *object_find_start(const char *obj, const char *key, qjv_kind_t *kind) {
    const char *p = skip_ws(obj + 1);
    if (*p == '}') return NULL;
    size_t keylen = strlen(key);
    for (;;) {
        p = skip_ws(p);
        if (*p != '"') return NULL;
        if (strncmp(p + 1, key, keylen) == 0 && p[1 + keylen] == '"') {
            char member[64];
            const char *after = parse_string(p, member, sizeof(member));
            if (!after) return NULL;
            after = skip_ws(after);
            if (*after != ':') return NULL;
            const char *val = skip_ws(after + 1);
            qjv_kind_t vk;
            if (!skip_value(val, &vk)) return NULL; /* structurally valid value */
            if (kind) *kind = vk;
            return val;
        }
        char member[64];
        p = parse_string(p, member, sizeof(member));
        if (!p) return NULL;
        p = skip_ws(p);
        if (*p != ':') return NULL;
        p = skip_value(skip_ws(p + 1), NULL);
        if (!p) return NULL;
        p = skip_ws(p);
        if (*p == '}') return NULL;
        if (*p != ',') return NULL;
        p++;
    }
}

/* Locate the value of `key` inside the array value starting at '[' at index
 * `idx`. Returns a pointer to that element's value start (already dereferenced
 * past the '[') or NULL if out of range. */
static const char *array_nth(const char *arr, size_t idx, qjv_kind_t *kind) {
    const char *p = skip_ws(arr + 1);
    if (*p == ']') return NULL;
    size_t i = 0;
    for (;;) {
        const char *vstart = skip_ws(p);
        qjv_kind_t vk;
        const char *vend = skip_value(vstart, &vk);
        if (vend == NULL) return NULL;
        if (i == idx) {
            if (kind) *kind = vk;
            return vstart;
        }
        p = skip_ws(vend);
        if (*p == ']') return NULL;
        if (*p != ',') return NULL;
        p++;
        i++;
    }
}

/* Navigate a dotted path from a value start, e.g. path "entity.entity_id"
 * inside the trigger object, or "0.value" inside actions array. Returns the
 * value start at the end of the path (leaves kind unused for arrays). */
static const char *path_navigate(const char *val, qjv_kind_t root_kind,
                                 const char *path, qjv_kind_t *out_kind) {
    const char *p = skip_ws(val);
    qjv_kind_t kind = root_kind;
    const char *base = p;
    const char *seg = path;
    for (;;) {
        const char *dot = strchr(seg, '.');
        size_t seglen = dot ? (size_t)(dot - seg) : strlen(seg);
        char key[16];
        if (seglen >= sizeof(key)) return NULL;
        memcpy(key, seg, seglen);
        key[seglen] = '\0';
        if (kind == QJV_OBJ) {
            base = object_find_start(base, key, &kind);
            if (!base) return NULL;
        } else if (kind == QJV_ARR && key[0] >= '0' && key[0] <= '9') {
            long idx = strtol(key, NULL, 10);
            if (idx < 0) return NULL;
            base = array_nth(base, (size_t)idx, &kind);
            if (!base) return NULL;
        } else {
            return NULL;
        }
        if (!dot) break;
        seg = dot + 1;
    }
    if (out_kind) *out_kind = kind;
    return base;
}

/* =========================
 * Internal: typed readers at a located JSON value
 * ========================= */

static const char *value_as_string(const char *v, char *out, size_t sz) {
    if (!v) return NULL;
    return parse_string(skip_ws(v), out, sz);
}

static bool value_as_bool(const char *v, bool *out) {
    const char *e = parse_bool(skip_ws(v), out);
    return e != NULL;
}

static bool value_as_number(const char *v, double *out) {
    const char *e = parse_number(skip_ws(v), out);
    return e != NULL;
}

/* =========================
 * Field presence helpers
 * ========================= */

/* Get an optionally-present typed field at the top level of the document.
 * start must point at '{'. kind_out receives the field's JSON kind (sets it
 * to QJV_NULL when absent). Returns NULL when absent. */
static const char *field_at(const char *doc, const char *key, qjv_kind_t *kind_out) {
    qjv_kind_t k;
    const char *v = object_find_start(doc, key, &k);
    if (!v) { if (kind_out) *kind_out = QJV_NULL; return NULL; }
    if (kind_out) *kind_out = k;
    return v;
}

/* =========================
 * Operator / action string mapping
 * ========================= */

static qymera_operator_t operator_from_str(const char *s) {
    if (!s) return QYMERA_OP_NONE;
    if (strcmp(s, "GT") == 0) return QYMERA_OP_GT;
    if (strcmp(s, "LT") == 0) return QYMERA_OP_LT;
    if (strcmp(s, "GE") == 0) return QYMERA_OP_GE;
    if (strcmp(s, "LE") == 0) return QYMERA_OP_LE;
    if (strcmp(s, "EQ") == 0) return QYMERA_OP_EQ;
    if (strcmp(s, "NE") == 0) return QYMERA_OP_NE;
    if (strcmp(s, "IN_RANGE") == 0) return QYMERA_OP_IN_RANGE;
    if (strcmp(s, "OUT_RANGE") == 0) return QYMERA_OP_OUT_RANGE;
    return QYMERA_OP_NONE;
}

static qymera_action_type_t action_from_str(const char *s) {
    if (!s) return QYMERA_ACTION_NONE;
    if (strcmp(s, "SET_BOOL") == 0) return QYMERA_ACTION_SET_BOOL;
    if (strcmp(s, "SET_LEVEL") == 0) return QYMERA_ACTION_SET_LEVEL;
    if (strcmp(s, "SET_VALUE") == 0) return QYMERA_ACTION_SET_VALUE;
    if (strcmp(s, "TOGGLE") == 0) return QYMERA_ACTION_TOGGLE;
    if (strcmp(s, "PULSE") == 0) return QYMERA_ACTION_PULSE;
    if (strcmp(s, "FADE") == 0) return QYMERA_ACTION_FADE;
    return QYMERA_ACTION_NONE;
}

/* =========================
 * Internal: parse simple input from JSON body
 * ========================= */

qymera_http_parse_result_t qymera_http_api_parse_simple_input(
    const char *json_body, qymera_skill_input_t *out, const char *control_field) {
    if (!json_body || !out) return QYMERA_HTTP_PARSE_BAD_JSON;
    memset(out, 0, sizeof(qymera_skill_input_t));
    if (!json_wellformed(json_body)) return QYMERA_HTTP_PARSE_BAD_JSON;

    qjv_kind_t k;

    /* Optional string fields (validated by the Skill layer downstream). */
    const char *v = field_at(json_body, "device_id", &k);
    if (v && k != QJV_STR) return QYMERA_HTTP_PARSE_TYPE;
    if (v) (void)value_as_string(v, out->device_id, sizeof(out->device_id));

    v = field_at(json_body, "entity_id", &k);
    if (v && k != QJV_STR) return QYMERA_HTTP_PARSE_TYPE;
    if (v) (void)value_as_string(v, out->entity_id, sizeof(out->entity_id));

    /* Required typed control field. */
    if (control_field && strcmp(control_field, "value") == 0) {
        v = field_at(json_body, "value", &k);
        if (!v) return QYMERA_HTTP_PARSE_MISSING;
        if (k != QJV_BOOL) return QYMERA_HTTP_PARSE_TYPE;
        bool b;
        (void)value_as_bool(v, &b);
        out->value = b;
    } else if (control_field && strcmp(control_field, "level") == 0) {
        v = field_at(json_body, "level", &k);
        if (!v) return QYMERA_HTTP_PARSE_MISSING;
        if (k != QJV_NUM) return QYMERA_HTTP_PARSE_TYPE;
        double n;
        (void)value_as_number(v, &n);
        out->level = (uint8_t)n;
    }

    return QYMERA_HTTP_PARSE_OK;
}

/* =========================
 * Internal: parse rule JSON body
 * ========================= */

qymera_http_parse_result_t qymera_http_api_parse_rule_input(
    const char *json_body, qymera_skill_input_t *out) {
    if (!json_body || !out) return QYMERA_HTTP_PARSE_BAD_JSON;
    memset(out, 0, sizeof(qymera_skill_input_t));
    if (!json_wellformed(json_body)) return QYMERA_HTTP_PARSE_BAD_JSON;

    qjv_kind_t k;
    qjv_kind_t ek;

    /* name (required string). */
    const char *v = field_at(json_body, "name", &k);
    if (!v) return QYMERA_HTTP_PARSE_MISSING;
    if (k != QJV_STR) return QYMERA_HTTP_PARSE_TYPE;
    (void)value_as_string(v, out->name, sizeof(out->name));

    /* rule_id (optional string). */
    v = field_at(json_body, "rule_id", &k);
    if (v) {
        if (k != QJV_STR) return QYMERA_HTTP_PARSE_TYPE;
        (void)value_as_string(v, out->rule_id, sizeof(out->rule_id));
    }

    /* enabled (optional bool). */
    v = field_at(json_body, "enabled", &k);
    if (v) {
        if (k != QJV_BOOL) return QYMERA_HTTP_PARSE_TYPE;
        (void)value_as_bool(v, &out->enabled);
    }

    /* trigger (optional; some rules may be triggerless, validated downstream).
     * Accepts either an object or a one-element array (get_rule emits an
     * array). */
    v = field_at(json_body, "trigger", &k);
    if (v) {
        const char *trig_obj = v;
        qjv_kind_t tk = k;
        if (k == QJV_ARR) {
            trig_obj = array_nth(v, 0, &tk);
            if (!trig_obj) { v = NULL; } else if (tk != QJV_OBJ) return QYMERA_HTTP_PARSE_TYPE;
        } else if (k != QJV_OBJ) {
            return QYMERA_HTTP_PARSE_TYPE;
        }
        if (v) {
        qymera_condition_t *trig = &out->rule.trigger;

        const char *ent = path_navigate(trig_obj, QJV_OBJ, "entity", &ek);
        if (ent) {
            if (ek != QJV_OBJ) return QYMERA_HTTP_PARSE_TYPE;
            const char *f = path_navigate(ent, QJV_OBJ, "device_id", &k);
            if (f) { if (k != QJV_STR) return QYMERA_HTTP_PARSE_TYPE; (void)value_as_string(f, trig->entity.device_id, sizeof(trig->entity.device_id)); }
            f = path_navigate(ent, QJV_OBJ, "entity_id", &k);
            if (f) { if (k != QJV_STR) return QYMERA_HTTP_PARSE_TYPE; (void)value_as_string(f, trig->entity.entity_id, sizeof(trig->entity.entity_id)); }
        }

        const char *op = path_navigate(trig_obj, QJV_OBJ, "operator", &k);
        if (!op) op = path_navigate(trig_obj, QJV_OBJ, "operator_", &k); /* GUI legacy alias */
        if (op) {
            char opstr[16];
            if (k != QJV_STR) return QYMERA_HTTP_PARSE_TYPE;
            (void)value_as_string(op, opstr, sizeof(opstr));
            trig->operator_ = operator_from_str(opstr);
            if (trig->operator_ == QYMERA_OP_NONE) return QYMERA_HTTP_PARSE_TYPE;
        }

        const char *f = path_navigate(trig_obj, QJV_OBJ, "threshold", &k);
        if (f) {
            if (k != QJV_NUM) return QYMERA_HTTP_PARSE_TYPE;
            double n;
            (void)value_as_number(f, &n);
            trig->threshold = (float)n;
        }
        }
    }

    /* conditions (optional array). */
    v = field_at(json_body, "conditions", &k);
    if (v) {
        if (k != QJV_ARR) return QYMERA_HTTP_PARSE_TYPE;
        for (size_t i = 0; i < QYMERA_MAX_CONDITIONS; i++) {
            const char *item = array_nth((const char *)v, i, &ek);
            if (!item) break;
            if (ek != QJV_OBJ) return QYMERA_HTTP_PARSE_TYPE;
            qymera_condition_t *c = &out->rule.conditions[i];
            /* Reuse the same parsing shape as trigger. */
            const char *ent = path_navigate(item, QJV_OBJ, "entity", &k);
            if (ent) {
                if (k != QJV_OBJ) return QYMERA_HTTP_PARSE_TYPE;
                const char *f = path_navigate(ent, QJV_OBJ, "device_id", &k);
                if (f) { if (k != QJV_STR) return QYMERA_HTTP_PARSE_TYPE; (void)value_as_string(f, c->entity.device_id, sizeof(c->entity.device_id)); }
                f = path_navigate(ent, QJV_OBJ, "entity_id", &k);
                if (f) { if (k != QJV_STR) return QYMERA_HTTP_PARSE_TYPE; (void)value_as_string(f, c->entity.entity_id, sizeof(c->entity.entity_id)); }
            }
            const char *op = path_navigate(item, QJV_OBJ, "operator", &k);
            if (!op) op = path_navigate(item, QJV_OBJ, "operator_", &k);
            if (op) {
                char opstr[16];
                if (k != QJV_STR) return QYMERA_HTTP_PARSE_TYPE;
                (void)value_as_string(op, opstr, sizeof(opstr));
                c->operator_ = operator_from_str(opstr);
            }
            const char *f = path_navigate(item, QJV_OBJ, "threshold", &k);
            if (f) {
                if (k != QJV_NUM) return QYMERA_HTTP_PARSE_TYPE;
                double n;
                (void)value_as_number(f, &n);
                c->threshold = (float)n;
            }
            out->rule.condition_count = (uint8_t)(i + 1);
        }
    }

    /* actions (required array with at least one entry). */
    v = field_at(json_body, "actions", &k);
    if (!v) return QYMERA_HTTP_PARSE_MISSING;
    if (k != QJV_ARR) return QYMERA_HTTP_PARSE_TYPE;
    size_t n_actions = 0;
    for (size_t i = 0; i < QYMERA_MAX_ACTIONS; i++) {
        const char *item = array_nth(v, i, &ek);
        if (!item) break;
        if (ek != QJV_OBJ) return QYMERA_HTTP_PARSE_TYPE;
        qymera_action_t *a = &out->rule.actions[i];

        const char *ent = path_navigate(item, QJV_OBJ, "entity", &k);
        if (!ent) return QYMERA_HTTP_PARSE_MISSING;
        if (k != QJV_OBJ) return QYMERA_HTTP_PARSE_TYPE;
        const char *f = path_navigate(ent, QJV_OBJ, "device_id", &k);
        if (f) { if (k != QJV_STR) return QYMERA_HTTP_PARSE_TYPE; (void)value_as_string(f, a->entity.device_id, sizeof(a->entity.device_id)); }
        f = path_navigate(ent, QJV_OBJ, "entity_id", &k);
        if (f) { if (k != QJV_STR) return QYMERA_HTTP_PARSE_TYPE; (void)value_as_string(f, a->entity.entity_id, sizeof(a->entity.entity_id)); }

        const char *act = path_navigate(item, QJV_OBJ, "action", &k);
        if (!act) return QYMERA_HTTP_PARSE_MISSING;
        if (k != QJV_STR) return QYMERA_HTTP_PARSE_TYPE;
        char actstr[16];
        (void)value_as_string(act, actstr, sizeof(actstr));
        a->action = action_from_str(actstr);
        if (a->action == QYMERA_ACTION_NONE) return QYMERA_HTTP_PARSE_TYPE;

        /* value: accept "value" (float or bool) and/or "value_u32" (integer). */
        const char *val = path_navigate(item, QJV_OBJ, "value", &k);
        if (val) {
            if (k == QJV_NUM) {
                double n;
                (void)value_as_number(val, &n);
                a->value_f = (float)n;
                a->value_u32 = (uint32_t)n;
            } else if (k == QJV_BOOL) {
                bool b;
                (void)value_as_bool(val, &b);
                a->value_u32 = b ? 1u : 0u;
                a->value_f = b ? 1.0f : 0.0f;
            } else {
                return QYMERA_HTTP_PARSE_TYPE;
            }
        }
        val = path_navigate(item, QJV_OBJ, "value_u32", &k);
        if (val) {
            if (k != QJV_NUM) return QYMERA_HTTP_PARSE_TYPE;
            double n;
            (void)value_as_number(val, &n);
            a->value_u32 = (uint32_t)n;
            /* Keep value_f in sync for numeric actions. */
            if (a->action == QYMERA_ACTION_SET_LEVEL ||
                a->action == QYMERA_ACTION_SET_VALUE)
                a->value_f = (float)n;
        }

        f = path_navigate(item, QJV_OBJ, "duration_ms", &k);
        if (f) {
            if (k != QJV_NUM) return QYMERA_HTTP_PARSE_TYPE;
            double n;
            (void)value_as_number(f, &n);
            a->duration_ms = (uint32_t)n;
        }
        n_actions = i + 1;
    }
    if (n_actions == 0) return QYMERA_HTTP_PARSE_MISSING;
    out->rule.action_count = (uint8_t)n_actions;

    /* cooldown_ms / priority / max_activations_per_hour (optional numbers). */
    v = field_at(json_body, "cooldown_ms", &k);
    if (v) {
        if (k != QJV_NUM) return QYMERA_HTTP_PARSE_TYPE;
        double n;
        (void)value_as_number(v, &n);
        out->rule.cooldown_ms = (uint32_t)n;
    }
    v = field_at(json_body, "priority", &k);
    if (v) {
        if (k != QJV_NUM) return QYMERA_HTTP_PARSE_TYPE;
        double n;
        (void)value_as_number(v, &n);
        out->rule.priority = (uint8_t)n;
    }
    v = field_at(json_body, "max_activations_per_hour", &k);
    if (v) {
        if (k != QJV_NUM) return QYMERA_HTTP_PARSE_TYPE;
        double n;
        (void)value_as_number(v, &n);
        out->rule.max_activations_per_hour = (uint32_t)n;
    }

    return QYMERA_HTTP_PARSE_OK;
}

/* =========================
 * HTTP error code mapping
 * ========================= */

uint16_t qymera_http_api_map_error_to_status(const char *error_code) {
    if (!error_code) return 500;
    if (strcmp(error_code, QYMERA_SKILL_ERR_SKILL_NOT_FOUND) == 0) return 404;
    if (strcmp(error_code, QYMERA_SKILL_ERR_ENTITY_NOT_FOUND) == 0) return 404;
    if (strcmp(error_code, QYMERA_SKILL_ERR_RULE_CONFLICT) == 0) return 409;
    if (strcmp(error_code, QYMERA_SKILL_ERR_RULE_INVALID) == 0) return 400;
    if (strcmp(error_code, QYMERA_SKILL_ERR_INVALID_VALUE) == 0) return 400;
    if (strcmp(error_code, QYMERA_SKILL_ERR_INVALID_INPUT) == 0) return 400;
    if (strcmp(error_code, QYMERA_SKILL_ERR_INVALID_CAPABILITY) == 0) return 422;
    if (strcmp(error_code, QYMERA_SKILL_ERR_PERMISSION_DENIED) == 0) return 403;
    if (strcmp(error_code, QYMERA_SKILL_ERR_DEVICE_OFFLINE) == 0) return 503;
    if (strcmp(error_code, QYMERA_SKILL_ERR_COMMAND_TIMEOUT) == 0) return 504;
    if (strcmp(error_code, QYMERA_SKILL_ERR_NO_SPACE) == 0) return 507;
    if (strcmp(error_code, QYMERA_SKILL_ERR_STORAGE_ERROR) == 0) return 500;
    if (strcmp(error_code, QYMERA_SKILL_ERR_DEPENDENCY_MISSING) == 0) return 503;
    return 500; /* fallback */
}

/* =========================
 * Result / error serialization
 * ========================= */

void qymera_http_api_serialize_result(qymera_skill_output_t *output,
                                     qymera_http_api_result_t *result) {
    if (!output || !result) return;
    if (output->ok) {
        result->ok = true;
        if (output->data_len > 0 && output->data_len < QYMERA_SKILL_OUTPUT_SIZE) {
            memcpy(result->data, output->data, output->data_len);
            result->data[output->data_len] = '\0';
            result->data_len = output->data_len;
        } else {
            result->data[0] = '\0';
            result->data_len = 0;
        }
        result->error_code[0] = '\0';
        result->message[0] = '\0';
    } else {
        result->ok = false;
        result->data[0] = '\0';
        result->data_len = 0;
        /* Copy stable error codes from the skill output */
        snprintf(result->error_code, sizeof(result->error_code),
                 "%s", output->error_code);
        snprintf(result->message, sizeof(result->message), "%s", output->message);
    }
}

void qymera_http_api_serialize_error(qymera_skill_output_t *output,
                                    qymera_http_api_error_t *error) {
    if (!output || !error) return;
    error->ok = false;
    snprintf(error->error_code, sizeof(error->error_code),
             "%s", output->error_code);
    snprintf(error->message, sizeof(error->message), "%s", output->message);
}

/* =========================
 * Result sending helpers
 * ========================= */

void qymera_http_api_send_result(httpd_req_t *req, qymera_http_api_result_t *result) {
    if (!req || !result) return;
    httpd_resp_set_type(req, "application/json");
    if (result->ok) {
        char buf[QYMERA_SKILL_OUTPUT_SIZE + 64];
        if (result->data_len > 0) {
            snprintf(buf, sizeof(buf), "{\"ok\":true,\"data\":%.*s}",
                     (int)result->data_len, result->data);
        } else {
            snprintf(buf, sizeof(buf), "{\"ok\":true,\"data\":{}}");
        }
        httpd_resp_send(req, buf, strlen(buf));
    } else {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "{\"ok\":false,\"error\":{\"code\":\"%s\",\"message\":\"%s\"}}",
                 result->error_code, result->message);
        httpd_resp_send(req, buf, strlen(buf));
    }
}

void qymera_http_api_send_error(httpd_req_t *req, qymera_http_api_error_t *error) {
    if (!req || !error) return;
    httpd_resp_set_type(req, "application/json");
    char buf[256];
    snprintf(buf, sizeof(buf),
             "{\"ok\":false,\"error\":{\"code\":\"%s\",\"message\":\"%s\"}}",
             error->error_code, error->message);
    httpd_resp_send(req, buf, strlen(buf));
}
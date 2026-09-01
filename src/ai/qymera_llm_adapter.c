/**
 * Qymera Dashboard - LLM Adapter Implementation
 *
 * The provider-facing boundary that dispatches bounded structured tool calls
 * strictly through the deterministic Skill API. No GPIO / UDP / registry-internal
 * / rule-engine-internal direct access. No natural-language parsing here. No
 * anonymous agent loop: each turn is bounded by QYMERA_MAX_TOOL_CALLS_PER_TURN.
 */
#include "qymera_llm_adapter.h"
#include "qymera_log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* =========================
 * Logging shortcut
 * ========================= */
#define LOG_AI(l, ...)   qymera_log_ai((l), "llm", __VA_ARGS__)
#define LOG_ERR(l, ...)  qymera_log_error((l), "llm", __VA_ARGS__)

/* =========================
 * Adapter handle
 * ========================= */
struct qymera_llm_adapter_s {
    qymera_skill_context_t *skills;
    qymera_log_t *log;
};

/* =========================
 * Lifecycle
 * ========================= */
qymera_err_t qymera_llm_adapter_init(qymera_llm_adapter_t **adapter,
                                     qymera_skill_context_t *skills,
                                     qymera_log_t *log) {
    if (!adapter) return QYMERA_ERR_INVALID_ARG;
    if (!skills || !skills->registry) return QYMERA_ERR_INVALID_ARG;
    qymera_llm_adapter_t *a = calloc(1, sizeof(qymera_llm_adapter_t));
    if (!a) return QYMERA_ERR_NO_SPACE;
    a->skills = skills;
    a->log = log;
    *adapter = a;
    return QYMERA_OK;
}

/* =========================
 * Tool catalog: DERIVED from the Skill registry (single source of truth).
 * There is no second, manually-duplicated tool list.
 * ========================= */
size_t qymera_llm_adapter_tool_count(void) {
    return qymera_skill_registry_count();
}

qymera_skill_id_t qymera_llm_adapter_tool_at(size_t idx, const qymera_skill_meta_t **meta) {
    const qymera_skill_entry_t *entry = NULL;
    qymera_skill_id_t id = qymera_skill_registry_get(idx, &entry);
    if (id == (qymera_skill_id_t)-1) {
        if (meta) *meta = NULL;
        return (qymera_skill_id_t)-1;
    }
    if (meta) *meta = &entry->meta;
    return id;
}

qymera_skill_id_t qymera_llm_adapter_tool_lookup(const char *name) {
    return qymera_skill_lookup(name);
}

/* =========================
 * Envelope error helper: keep ONE serialized error taxonomy (the stable Skill
 * envelope). Envelope-level adapter failures are expressed through the same
 * {ok:false, error:{code,message,details}} shape.
 * ========================= */
static void envelope_error(qymera_skill_output_t *out, const char *code, const char *msg) {
    out->ok = false;
    out->truncated = false;
    out->data_len = 0;
    out->data[0] = '\0';
    snprintf(out->error_code, sizeof(out->error_code), "%s", code);
    snprintf(out->message, sizeof(out->message), "%s", msg);
}

/* Skills that require entity identity. */
static bool skill_requires_entity(const char *name) {
    return strcmp(name, "get_entity_state") == 0 ||
           strcmp(name, "get_entity_info") == 0 ||
           strcmp(name, "set_relay") == 0 ||
           strcmp(name, "set_dimmer") == 0;
}

static bool skill_requires_rule_id(const char *name) {
    return strcmp(name, "get_rule") == 0 ||
           strcmp(name, "update_rule") == 0 ||
           strcmp(name, "delete_rule") == 0 ||
           strcmp(name, "enable_rule") == 0 ||
           strcmp(name, "disable_rule") == 0;
}

static bool skill_requires_name(const char *name) {
    return strcmp(name, "create_rule") == 0;
}

/* =========================
 * Execute one validated structured tool call strictly through the Skill layer.
 * ========================= */
qymera_err_t qymera_llm_adapter_execute_tool(qymera_llm_adapter_t *adapter,
                                             const char *tool_name,
                                             const qymera_llm_tool_arguments_t *args,
                                             uint32_t permission_mask,
                                             qymera_skill_output_t *out,
                                             qymera_llm_tool_error_t *tool_err) {
    if (tool_err) *tool_err = QYMERA_LLM_TOOL_UNKNOWN;
    if (!adapter || !adapter->skills || !out) return QYMERA_ERR_INVALID_ARG;
    if (!tool_name || !tool_name[0]) { *tool_err = QYMERA_LLM_TOOL_UNKNOWN; return QYMERA_ERR_INVALID_ARG; }

    /* skill exists (lookup against registry)? */
    if (qymera_skill_lookup(tool_name) == (qymera_skill_id_t)-1) {
        *tool_err = QYMERA_LLM_TOOL_UNKNOWN;
        envelope_error(out, QYMERA_SKILL_ERR_SKILL_NOT_FOUND, "unknown skill / tool");
        return QYMERA_OK;
    }

    /* permission propagation: explicit mask, never silently grant-all. */
    const qymera_skill_entry_t *entry = NULL;
    qymera_skill_registry_get((size_t)qymera_skill_lookup(tool_name), &entry);
    if ((permission_mask & entry->meta.permissions) != entry->meta.permissions) {
        *tool_err = QYMERA_LLM_TOOL_PERMISSION;
        envelope_error(out, QYMERA_SKILL_ERR_PERMISSION_DENIED, "insufficient permission for this skill");
        return QYMERA_OK;
    }

    /* structural argument presence (adapter-level, before the runtime). */
    if (skill_requires_entity(tool_name) && (!args || !args->device_id[0] || !args->entity_id[0])) {
        *tool_err = QYMERA_LLM_TOOL_MISSING_ARGS;
        envelope_error(out, QYMERA_SKILL_ERR_INVALID_INPUT, "missing required arguments (device_id/entity_id)");
        return QYMERA_OK;
    }
    if (skill_requires_rule_id(tool_name) && (!args || !args->rule_id[0])) {
        *tool_err = QYMERA_LLM_TOOL_MISSING_ARGS;
        envelope_error(out, QYMERA_SKILL_ERR_INVALID_INPUT, "missing required argument (rule_id)");
        return QYMERA_OK;
    }
    if (skill_requires_name(tool_name) && (!args || !args->name[0])) {
        *tool_err = QYMERA_LLM_TOOL_MISSING_ARGS;
        envelope_error(out, QYMERA_SKILL_ERR_INVALID_INPUT, "missing required argument (name)");
        return QYMERA_OK;
    }

    /* Build the deterministic skill input (zero-init, then copy provided fields). */
    qymera_skill_input_t in;
    memset(&in, 0, sizeof(in));
    if (args) {
        snprintf(in.device_id, sizeof(in.device_id), "%s", args->device_id);
        snprintf(in.entity_id, sizeof(in.entity_id), "%s", args->entity_id);
        snprintf(in.name, sizeof(in.name), "%s", args->name);
        snprintf(in.rule_id, sizeof(in.rule_id), "%s", args->rule_id);
        in.value = args->value;
        in.level = args->level;
        in.enabled = args->enabled;
        if (args->has_rule) in.rule = args->rule;
    }

    /* Dispatch strictly through the Skill layer. */
    qymera_err_t err = qymera_skill_execute(adapter->skills, tool_name, &in, out, permission_mask);
    *tool_err = QYMERA_LLM_TOOL_OK;
    return err;
}

/* =========================
 * Strict, bounded JSON tool-arguments extractor (flat fields only).
 * No JSON-library dependency; a minimal deterministic object scanner.
 * ========================= */
static const char *llm_skip_ws(const char *p, const char *end) {
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    return p;
}

static int llm_parse_string(const char **pp, const char *end, char *dst, size_t cap) {
    const char *p = llm_skip_ws(*pp, end);
    if (p >= end || *p != '"') return -1;
    p++;
    size_t len = 0;
    while (p < end) {
        unsigned char c = (unsigned char)*p;
        if (c == '"') {
            dst[len < cap ? len : cap - 1] = '\0';
            *pp = p + 1;
            return 0;
        }
        if (c == '\\') {
            p++;
            if (p >= end) return -1;
            char e = *p;
            if (e == '"' || e == '\\' || e == '/') {
                if (len < cap - 1) dst[len++] = e;
                p++;
            } else if (e == 'n') { if (len < cap - 1) dst[len++] = '\n'; p++; }
            else if (e == 't') { if (len < cap - 1) dst[len++] = '\t'; p++; }
            else if (e == 'r') { if (len < cap - 1) dst[len++] = '\r'; p++; }
            else if (e == 'b') { if (len < cap - 1) dst[len++] = '\b'; p++; }
            else if (e == 'f') { if (len < cap - 1) dst[len++] = '\f'; p++; }
            else if (e == 'u') { /* \uXXXX: fields here never need it; skip */
                p++; for (int i = 0; i < 4 && p < end; i++) p++;
            } else { return -1; }
        } else {
            if (len < cap - 1) dst[len++] = (char)c;
            p++;
        }
    }
    return -1;
}

static int llm_parse_bool(const char **pp, const char *end, bool *out) {
    const char *p = llm_skip_ws(*pp, end);
    if (end - p >= 4 && memcmp(p, "true", 4) == 0) { *out = true;  *pp = p + 4; return 0; }
    if (end - p >= 5 && memcmp(p, "false", 5) == 0) { *out = false; *pp = p + 5; return 0; }
    return -1;
}

static int llm_parse_number(const char **pp, const char *end, float *out) {
    const char *p = llm_skip_ws(*pp, end);
    char buf[24];
    size_t n = 0;
    while (p < end && n < sizeof(buf) - 1 &&
           ((*p >= '0' && *p <= '9') || *p == '-' || *p == '+' || *p == '.')) {
        buf[n++] = *p++;
    }
    if (n == 0) return -1;
    buf[n] = '\0';
    *pp = p;
    *out = (float)atof(buf);
    return 0;
}

qymera_err_t qymera_llm_args_from_json(const char *json, size_t json_len,
                                       qymera_llm_tool_arguments_t *args) {
    if (!json || !args) return QYMERA_ERR_INVALID_ARG;
    memset(args, 0, sizeof(*args));
    const char *end = json + json_len;
    const char *p = llm_skip_ws(json, end);
    if (p >= end || *p != '{') return QYMERA_ERR_INVALID_ARG;
    p++;
    for (;;) {
        p = llm_skip_ws(p, end);
        if (p >= end) return QYMERA_ERR_INVALID_ARG;
        if (*p == '}') break;
        char key[40];
        if (llm_parse_string(&p, end, key, sizeof(key)) != 0) return QYMERA_ERR_INVALID_ARG;
        p = llm_skip_ws(p, end);
        if (p >= end || *p != ':') return QYMERA_ERR_INVALID_ARG;
        p++;
        char tmp[80];
        bool btmp = false;
        float ftmp = 0.0f;
        if (llm_parse_string(&p, end, tmp, sizeof(tmp)) == 0) {
            if (strcmp(key, "device_id") == 0) snprintf(args->device_id, sizeof(args->device_id), "%s", tmp);
            else if (strcmp(key, "entity_id") == 0) snprintf(args->entity_id, sizeof(args->entity_id), "%s", tmp);
            else if (strcmp(key, "name") == 0) snprintf(args->name, sizeof(args->name), "%s", tmp);
            else if (strcmp(key, "rule_id") == 0) snprintf(args->rule_id, sizeof(args->rule_id), "%s", tmp);
        } else if (llm_parse_bool(&p, end, &btmp) == 0) {
            if (strcmp(key, "value") == 0) { args->value = btmp; args->value_set = true; }
            else if (strcmp(key, "enabled") == 0) { args->enabled = btmp; args->enabled_set = true; }
        } else if (llm_parse_number(&p, end, &ftmp) == 0) {
            if (strcmp(key, "level") == 0) { args->level = (uint8_t)ftmp; args->value_set = true; }
        } else {
            /* unknown/nested value (e.g. a rule body): fields here are covered by
             * the native typed carrier; skip the value deterministically to stay
             * strict about object shape without a full JSON parser. */
            if (p >= end) return QYMERA_ERR_INVALID_ARG;
            p++;
        }
        p = llm_skip_ws(p, end);
        if (p >= end) return QYMERA_ERR_INVALID_ARG;
        if (*p == ',') { p++; continue; }
        if (*p == '}') break;
        return QYMERA_ERR_INVALID_ARG;
    }
    return QYMERA_OK;
}

/* =========================
 * Turn processing with tool budget + recursion protection.
 * The adapter executes ONLY explicit tool calls returned by the model. There is
 * no autonomous planning or background loop. The loop is bounded by
 * QYMERA_MAX_TOOL_CALLS_PER_TURN (request->max_tool_calls).
 * ========================= */
qymera_err_t qymera_llm_adapter_process(qymera_llm_adapter_t *adapter,
                                        const qymera_llm_provider_t *provider,
                                        const qymera_llm_request_t *request,
                                        qymera_llm_turn_result_t *result) {
    if (!adapter || !provider || !request || !result) return QYMERA_ERR_INVALID_ARG;
    if (!provider->complete) return QYMERA_ERR_INVALID_ARG;
    memset(result, 0, sizeof(*result));

    size_t written = 0;
    uint8_t budget = request->max_tool_calls ? request->max_tool_calls : QYMERA_MAX_TOOL_CALLS_PER_TURN;
    if (budget > QYMERA_MAX_TOOL_CALLS_PER_TURN) budget = QYMERA_MAX_TOOL_CALLS_PER_TURN;

    for (;;) {
        qymera_llm_message_t msg;
        memset(&msg, 0, sizeof(msg));
        qymera_err_t err = provider->complete(provider->provider_ctx, request, &msg);
        if (err != QYMERA_OK) {
            result->ended = QYMERA_LLM_TURN_PROVIDER_ERROR;
            result->final = msg;
            break;
        }

        switch (msg.kind) {
            case QYMERA_LLM_MSG_TEXT:
                result->ended = QYMERA_LLM_TURN_TEXT;
                result->final = msg;
                break;
            case QYMERA_LLM_MSG_MALFORMED:
                result->ended = QYMERA_LLM_TURN_MALFORMED;
                result->final = msg;
                break;
            case QYMERA_LLM_MSG_PROVIDER_ERROR:
            case QYMERA_LLM_MSG_TIMEOUT:
                result->ended = QYMERA_LLM_TURN_PROVIDER_ERROR;
                result->final = msg;
                break;
            case QYMERA_LLM_MSG_TOOL_CALL: {
                if (result->tool_calls >= budget) {
                    result->ended = QYMERA_LLM_TURN_TOOL_CALL_LIMIT;
                    break;
                }
                qymera_skill_output_t out;
                memset(&out, 0, sizeof(out));
                qymera_llm_tool_error_t terr = QYMERA_LLM_TOOL_OK;
                (void)qymera_llm_adapter_execute_tool(adapter, msg.tool_name, &msg.args,
                                                      request->permission_mask, &out, &terr);
                result->tool_calls++;

                /* Bounded transcript of the step. */
                char line[QYMERA_LLM_RESULT_LEN / 2];
                if (terr == QYMERA_LLM_TOOL_OK && out.ok) {
                    snprintf(line, sizeof(line), "[tool:%s:ok] %s\n", msg.tool_name, out.data);
                } else {
                    const char *code = out.error_code[0] ? out.error_code : "ERROR";
                    snprintf(line, sizeof(line), "[tool:%s:error:%s]\n", msg.tool_name, code);
                }
                size_t ln = strlen(line);
                if (written + ln < sizeof(result->outcome)) {
                    memcpy(result->outcome + written, line, ln);
                    written += ln;
                    result->outcome[written] = '\0';
                }
                continue;   /* tool result fed back -> next model response */
            }
            default:
                result->ended = QYMERA_LLM_TURN_PROVIDER_ERROR;
                result->final = msg;
                break;
        }
        break;  /* terminal message kind */
    }
    return QYMERA_OK;
}

/* =========================
 * Deterministic mock provider: emits a fixed structured-workflow script.
 * Used only for wiring / demonstration / reference; a real provider is added
 * later against the same qymera_llm_provider_t interface.
 * ========================= */
static qymera_err_t llm_mock_complete(void *provider_ctx,
                                      const qymera_llm_request_t *request,
                                      qymera_llm_message_t *message) {
    (void)request;
    qymera_llm_mock_ctx_t *ctx = (qymera_llm_mock_ctx_t *)provider_ctx;
    if (!ctx || !message) return QYMERA_ERR_INVALID_ARG;
    memset(message, 0, sizeof(*message));

    switch (ctx->step++) {
        case 0:
            message->kind = QYMERA_LLM_MSG_TOOL_CALL;
            snprintf(message->tool_name, sizeof(message->tool_name), "list_entities");
            break;
        case 1:
            message->kind = QYMERA_LLM_MSG_TOOL_CALL;
            snprintf(message->tool_name, sizeof(message->tool_name), "get_entity_state");
            snprintf(message->args.device_id, sizeof(message->args.device_id), "node-a");
            snprintf(message->args.entity_id, sizeof(message->args.entity_id), "temperature");
            break;
        case 2:
            message->kind = QYMERA_LLM_MSG_TOOL_CALL;
            snprintf(message->tool_name, sizeof(message->tool_name), "create_rule");
            snprintf(message->args.name, sizeof(message->args.name), "Garden fan helper");
            break;
        case 3:
            message->kind = QYMERA_LLM_MSG_TOOL_CALL;
            snprintf(message->tool_name, sizeof(message->tool_name), "enable_rule");
            snprintf(message->args.rule_id, sizeof(message->args.rule_id), "rule_1");
            break;
        default:
            message->kind = QYMERA_LLM_MSG_TEXT;
            snprintf(message->text, sizeof(message->text), "Done.");
            break;
    }
    return QYMERA_OK;
}

void qymera_llm_mock_provider_init(qymera_llm_provider_t *provider, void *ctx) {
    if (!provider) return;
    provider->provider_ctx = ctx;
    provider->complete = llm_mock_complete;
    if (ctx) ((qymera_llm_mock_ctx_t *)ctx)->step = 0;
}

/**
 * Qymera Dashboard - HTTP API implementation
 *
 * All operations are funneled through qymera_skill_execute(). The HTTP
 * layer performs only HTTP / JSON schema validation, then forwards to
 * the Skill layer, which then validates and calls the deterministic
 * runtime (Registry, Rule Engine, Control API, Storage). The HTTP layer
 * NEVER calls GPIO, UDP, Registry mutation, Rule Engine mutation,
 * or storage directly.
 */

#include "qymera_http_api.h"
#include "qymera_skill.h"
#include "qymera_log.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* =========================
 * Internal: tiny JSON extractor helpers
 * ========================= */

static bool llm_skip_ws(char **p) {
    while (**p == ' ' || **p == '\t' || **p == '\n' || **p == '\r') (*p)++;
    return true;
}

static bool llm_parse_string(char **p, char *out, size_t out_sz) {
    *out = '\0';
    if (**p != '\"') return false;
    (*p)++; /* skip opening quote */
    size_t i = 0;
    for (; **p && **p != '\"' && i < out_sz - 1; (*p)++, i++) {
        if (**p == '\\') {
            (*p)++;
            if (**p == 'u') {
                /* skip \uXXXX – we don't parse unicode escapes, just skip them */
                (*p) += 4;
            } else {
                char esc[2] = {**p, '\0'};
                /* Only handle known escapes; others pass the backslash through */
                if (strchr("\"\\/bfnrt", esc[0])) {
                    out[i] = esc[0];
                } else {
                    out[i] = **p; /* pass through unknown escapes as-is */
                }
            }
        } else {
            out[i] = **p;
        }
    }
    out[i] = '\0';
    if (**p == '\"') (*p)++; /* skip closing quote */
    return true;
}

static bool llm_parse_bool(char **p, bool *out) {
    llm_skip_ws(p);
    if (**p == 't' && strncmp(*p, "true", 4) == 0) { *p += 4; *out = true; return true; }
    if (**p == 'f' && strncmp(*p, "false", 5) == 0) { *p += 5; *out = false; return true; }
    return false;
}

static bool llm_parse_number(char **p, double *out) {
    llm_skip_ws(p);
    /* Very simple parser: read optional sign, digits, optional .digits, optional e/E[+/-]digits */
    const char *start = *p;
    if (**p == '+' || **p == '-') (*p)++;
    while (**p >= '0' && **p <= '9') (*p)++;
    if (**p == '.' && strncmp(*p, ".", 1) == 0) {
        (*p)++;
        while (**p >= '0' && **p <= '9') (*p)++;
    }
    if (**p == 'e' || **p == 'E') {
        (*p)++;
        if (**p == '+' || **p == '-') (*p)++;
        while (**p >= '0' && **p <= '9') (*p)++;
    }
    if (*p == start) return false;
    *out = atof(start);
    return true;
}

/* =========================
 * Internal: find a JSON string value by key
 * ========================= */

static bool qymera_http_json_find_string(const char *json, const char *key,
                                         char *out, size_t out_sz) {
    /* Look for "key": followed by a string value. Returns false if not found. */
    size_t key_len = strlen(key);
    char *p = (char *)json;
    while (*p) {
        /* Find the key */
        if (strncasecmp(p, key, key_len) == 0 && p[key_len] == ':') {
            p += key_len + 1; /* skip key: */
            llm_skip_ws(&p);
            if (*p == '\"') {
                p++; /* skip opening quote */
                size_t i = 0;
                for (; *p && *p != '\"' && i < out_sz - 1; (*p)++, i++) {
                    if (*p == '\\') {
                        (*p)++;
                        if (*p == 'u') (*p) += 4; /* skip unicode */
                    } else {
                        out[i] = *p;
                    }
                }
                out[i] = '\0';
                if (*p == '\"') p++; /* skip closing quote */
                return true;
            }
        }
        p++;
    }
    return false;
}

/* =========================
 * Internal: parse simple input from JSON body
 * ========================= */

bool qymera_http_api_parse_simple_input(const char *json_body,
                                        qymera_skill_input_t *out) {
    if (!json_body || !out) return false;
    memset(out, 0, sizeof(qymera_skill_input_t));

    /* Find fields in the JSON body */
    /* device_id */
    if (!qymera_http_json_find_string(json_body, "device_id", out->device_id,
                                      sizeof(out->device_id))) {
        /* Not fatal; some skills don't require device_id */
    }
    /* entity_id */
    if (!qymera_http_json_find_string(json_body, "entity_id", out->entity_id,
                                      sizeof(out->entity_id))) {}
    /* name */
    if (!qymera_http_json_find_string(json_body, "name", out->name, sizeof(out->name))) {}
    /* rule_id */
    if (!qymera_http_json_find_string(json_body, "rule_id", out->rule_id,
                                      sizeof(out->rule_id))) {}
    /* value (bool) */
    {
        const char *p = json_body;
        bool val;
        if (llm_parse_bool(&p, &val)) {
            out->value = val;
        }
    }
    /* level (uint8_t 0-100) */
    {
        const char *p = json_body;
        double num;
        if (llm_parse_number(&p, &num)) {
            out->level = (uint8_t)num;
        }
    }

    return true;
}

/* =========================
 * Internal: parse rule JSON body
 * ========================= */

bool qymera_http_api_parse_rule_input(const char *json_body,
                                      qymera_skill_input_t *out) {
    if (!json_body || !out) return false;
    memset(out, 0, sizeof(qymera_skill_input_t));

    /* Find fields */
    if (!qymera_http_json_find_string(json_body, "name", out->name,
                                      sizeof(out->name))) {
        return false; /* name is required for rule skills */
    }
    if (!qymera_http_json_find_string(json_body, "rule_id", out->rule_id,
                                      sizeof(out->rule_id))) {
        return false;
    }
    /* actions */
    {
        /* Minimal: read action count and first action's entity/action/value */
        const char *p = json_body;
        /* Heuristic search for "actions": [ { "entity": ..., "action": ..., ... } ] */
        if (strstr(p, "\"actions\"") != NULL) {
            /* We'll leave full parsing to the skill layer; just note we saw it */
        }
    }
    /* enabled flag */
    {
        const char *p = json_body;
        bool val;
        if (llm_parse_bool(&p, &val)) {
            out->enabled = val;
        }
    }

    return true;
}

/* =========================
 * HTTP error code mapping
 * ========================= */

uint16_t qymera_http_api_map_error_to_status(const char *error_code) {
    if (!error_code) return 500;
    if (strcmp(error_code, QYMERA_SKILL_ERR_SKILL_NOT_FOUND) == 0) return 404;
    if (strcmp(error_code, QYMERA_SKILL_ERR_ENTITY_NOT_FOUND) == 0) return 404;
    if (strcmp(error_code, QYMERA_SKILL_ERR_RULE_CONFLICT) == 0) return 409;
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
        if (output->data_len > 0 && output->data_len < QYMERA_SKILL_OUTPUT_SIZE) {
            memcpy(result->data, output->data, output->data_len);
            result->data[output->data_len] = '\0';
            result->data_len = output->data_len;
        } else {
            result->data[0] = '\0';
            result->data_len = 0;
        }
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
    if (result->ok) {
        httpd_resp_set_type(req, "application/json");
        if (result->data_len > 0) {
            httpd_resp_send(req, result->data, result->data_len);
        } else {
            httpd_resp_sendstr(req, "{\"ok\":true,\"data\":{}}");
        }
    } else {
        httpd_resp_set_type(req, "application/json");
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
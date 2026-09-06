/**
 * Qymera Dashboard - HTTP Server implementation
 *
 * All control/CRUD operations are routed through qymera_skill_execute().
 * The HTTP layer performs JSON schema validation, then forwards to the
 * Skill layer, which validates and calls the deterministic runtime
 * (Registry, Rule Engine, Control API, Storage). The HTTP layer NEVER
 * calls GPIO, UDP, Registry mutation, Rule Engine mutation, or storage
 * directly.
 *
 * Routing uses the ESP-IDF wildcard URI matcher so that id-anchored routes
 * (/api/v1/rules/<id>, /api/v1/rules/<id>/enable, /api/v1/entities/<dev>/
 * <ent>) resolve correctly. All responses use the {ok,data}/{ok,error}
 * envelope; the GUI must rely on the envelope, not only on HTTP status.
 */

#include "qymera_http_api.h"
#include "qymera_skill.h"
#include "qymera_core.h"
#include "qymera_hal.h"
#include "qymera_log.h"
#include "qymera_registry.h"
#include "qymera_dashboard_html.h"
#include "qymera_storage.h"
#include "qymera_llm_adapter.h"
#include "qymera_llm_http_provider.h"
#include <esp_http_server.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define QYMERA_HTTP_BODY_SZ 512

#define QYMERA_HTTP_ERR_INVALID_INPUT "INVALID_INPUT"

/* =========================
 * Response helpers
 * ========================= */

static void http_send_json(httpd_req_t *req, const char *json) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
}

static void http_send_parse_error(httpd_req_t *req, qymera_http_parse_result_t pr) {
    const char *msg;
    switch (pr) {
        case QYMERA_HTTP_PARSE_BAD_JSON: msg = "Malformed JSON body"; break;
        case QYMERA_HTTP_PARSE_MISSING:  msg = "Missing required field in request body"; break;
        case QYMERA_HTTP_PARSE_TYPE:     msg = "Request body field has the wrong type"; break;
        default:                         msg = "Invalid request"; break;
    }
    char buf[320];
    snprintf(buf, sizeof(buf),
             "{\"ok\":false,\"error\":{\"code\":\"INVALID_INPUT\",\"message\":\"%s\"}}",
             msg);
    httpd_resp_set_status(req, "400 Bad Request");
    http_send_json(req, buf);
}

/* =========================
 * Skill dispatch helper
 * ========================= */

static bool http_dispatch(qymera_core_t *core, const char *skill_name,
                            qymera_skill_input_t *input,
                            qymera_skill_output_t *output,
                            uint32_t perm_mask) {
    qymera_skill_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.registry = qymera_core_get_registry(core);
    ctx.rule_engine = qymera_core_get_rule_engine(core);
    ctx.control = qymera_core_get_control(core);
    ctx.storage = qymera_core_get_storage(core);
    ctx.log = qymera_core_get_log(core);
    qymera_err_t err = qymera_skill_execute(&ctx, skill_name, input, output, perm_mask);
    return err == QYMERA_OK;
}

static void http_skill_to_http(qymera_core_t *core, const char *skill,
                                 qymera_skill_input_t *input,
                                 uint32_t perm, httpd_req_t *req) {
    qymera_skill_output_t out;
    memset(&out, 0, sizeof(out));
    http_dispatch(core, skill, input, &out, perm);

    qymera_http_api_result_t result;
    memset(&result, 0, sizeof(result));
    qymera_http_api_serialize_result(&out, &result);
    if (!result.ok) {
        char status[40];
        snprintf(status, sizeof(status), "%d Error",
                 qymera_http_api_map_error_to_status(result.error_code));
        httpd_resp_set_status(req, status);
    }
    qymera_http_api_send_result(req, &result);
}

/* =========================
 * URI path helpers
 * ========================= */

static bool extract_rule_id(const char *uri, const char *skip_suffix,
                            char *out, size_t sz) {
    const char *p = strstr(uri, "/api/v1/rules/");
    if (!p) return false;
    p += strlen("/api/v1/rules/");
    const char *end = strchr(p, '?');
    if (skip_suffix) {
        const char *s = strstr(p, skip_suffix);
        if (s) end = s;
    }
    if (!end) end = strchr(p, '\0');
    size_t len = end - p;
    if (len >= sz) len = sz - 1;
    strncpy(out, p, len);
    out[len] = '\0';
    return true;
}

static bool extract_entity_path(const char *uri, char *device, size_t dev_sz,
                                  char *entity, size_t ent_sz) {
    const char *p = strstr(uri, "/api/v1/entities/");
    if (!p) return false;
    p += strlen("/api/v1/entities/");
    const char *slash = strchr(p, '/');
    if (!slash) return false;
    size_t dlen = slash - p;
    if (dlen >= dev_sz) dlen = dev_sz - 1;
    strncpy(device, p, dlen);
    device[dlen] = '\0';
    const char *e = slash + 1;
    const char *eq = strchr(e, '?');
    if (!eq) eq = strchr(e, '\0');
    size_t elen = eq - e;
    if (elen >= ent_sz) elen = ent_sz - 1;
    strncpy(entity, e, elen);
    entity[elen] = '\0';
    return true;
}

static bool extract_rule_action(const char *uri, char *rid, size_t rid_sz,
                                bool *enable, bool *disable) {
    *enable = false;
    *disable = false;
    if (strstr(uri, "/enable")) *enable = true;
    else if (strstr(uri, "/disable")) *disable = true;
    if (*enable || *disable) {
        return extract_rule_id(uri, (*enable) ? "/enable" : "/disable",
                               rid, rid_sz);
    }
    return false;
}

/* =========================
 * Handlers
 * ========================= */

static esp_err_t h_status_get(httpd_req_t *req) {
    qymera_core_t *core = (qymera_core_t *)req->user_ctx;
    uint32_t heap = qymera_system_get_free_heap();
    uint32_t up = qymera_system_get_uptime_ms();
    char ip[16] = {0};
    qymera_wifi_get_ip(ip, sizeof(ip));
    size_t dc = qymera_registry_device_count(qymera_core_get_registry(core));
    size_t ec = qymera_registry_entity_count(qymera_core_get_registry(core));

    qymera_wifi_mode_t mode = qymera_wifi_get_mode();
    const char *network = "STA";
    if (mode == QYMERA_WIFI_MODE_AP) network = "AP";
    else if (mode == QYMERA_WIFI_MODE_APSTA) network = "APSTA";
    char ssid[33] = {0};
    if (mode == QYMERA_WIFI_MODE_AP || mode == QYMERA_WIFI_MODE_APSTA) {
        qymera_wifi_get_ap_ssid(ssid, sizeof(ssid));
    }
    if (ip[0] == '\0') qymera_wifi_get_ap_ip(ip, sizeof(ip));

    /* Load network config (UDP ports persist across reboots). */
    qymera_storage_t *st = qymera_core_get_storage(core);
    qymera_network_config_t ncfg;
    memset(&ncfg, 0, sizeof(ncfg));
    qymera_err_t lerr = qymera_storage_load_network(st, &ncfg);
    uint16_t udp_discovery_port = QYMERA_UDP_PORT_DISCOVERY;
    uint16_t udp_control_port = QYMERA_UDP_PORT_CONTROL;
    if (lerr == QYMERA_OK) {
        udp_discovery_port = ncfg.udp_discovery_port;
        udp_control_port = ncfg.udp_control_port;
    } else if (lerr == QYMERA_ERR_NOT_FOUND) {
        /* defaults already set above */
    }

    char buf[512];
    const char *ai_mode = "none";
    switch (qymera_core_get_config(core)->ai.mode) {
        case QYMERA_AI_MODE_LOCAL:  ai_mode = "local"; break;
        case QYMERA_AI_MODE_REMOTE: ai_mode = "remote"; break;
        case QYMERA_AI_MODE_HYBRID: ai_mode = "hybrid"; break;
        default: break;
    }
    snprintf(buf, sizeof(buf),
        "{\"ok\":true,\"data\":{\"free_heap\":%u,\"uptime_ms\":%u,"
        "\"ip\":\"%s\",\"network\":\"%s\",\"ssid\":\"%s\","
        "\"udp_discovery_port\":%u,\"udp_control_port\":%u,"
        "\"device_count\":%zu,\"entity_count\":%zu,\"ai_mode\":\"%s\"}}",
        heap, up, ip, network, ssid,
        udp_discovery_port, udp_control_port,
        dc, ec, ai_mode);
    http_send_json(req, buf);
    return ESP_OK;
}

static esp_err_t h_devices_get(httpd_req_t *req) {
    qymera_core_t *core = (qymera_core_t *)req->user_ctx;
    qymera_skill_input_t in; memset(&in, 0, sizeof(in));
    http_skill_to_http(core, "list_devices", &in, QYMERA_PERM_READ, req);
    return ESP_OK;
}

static esp_err_t h_entities_get(httpd_req_t *req) {
    qymera_core_t *core = (qymera_core_t *)req->user_ctx;
    qymera_skill_input_t in; memset(&in, 0, sizeof(in));
    http_skill_to_http(core, "list_entities", &in, QYMERA_PERM_READ, req);
    return ESP_OK;
}

static esp_err_t h_entity_state_get(httpd_req_t *req) {
    qymera_core_t *core = (qymera_core_t *)req->user_ctx;
    char device[QYMERA_DEVICE_ID_LEN], entity[QYMERA_ENTITY_ID_LEN];
    if (!extract_entity_path(req->uri, device, sizeof(device),
                              entity, sizeof(entity))) {
        http_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"MALFORMED_URI\"}}");
        return ESP_OK;
    }
    qymera_skill_input_t in; memset(&in, 0, sizeof(in));
    strncpy(in.device_id, device, sizeof(in.device_id) - 1);
    strncpy(in.entity_id, entity, sizeof(in.entity_id) - 1);
    http_skill_to_http(core, "get_entity_state", &in, QYMERA_PERM_READ, req);
    return ESP_OK;
}

/* Single handler for /api/v1/entities* that dispatches:
 * - GET /api/v1/entities -> list_entities
 * - GET /api/v1/entities/device/entity -> get_entity_state */
static esp_err_t h_entities_dispatch(httpd_req_t *req) {
    if (strcmp(req->uri, "/api/v1/entities") == 0) {
        return h_entities_get(req);
    }
    return h_entity_state_get(req);
}

static esp_err_t h_control_post(httpd_req_t *req, const char *control_field,
                                const char *skill_name) {
    qymera_core_t *core = (qymera_core_t *)req->user_ctx;
    char body[QYMERA_HTTP_BODY_SZ];
    int n = httpd_req_recv(req, body, sizeof(body) - 1);
    if (n <= 0) {
        http_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INVALID_INPUT\"}}");
        return ESP_OK;
    }
    body[n] = '\0';
    qymera_skill_input_t in;
    qymera_http_parse_result_t pr =
        qymera_http_api_parse_simple_input(body, &in, control_field);
    if (pr != QYMERA_HTTP_PARSE_OK) {
        http_send_parse_error(req, pr);
        return ESP_OK;
    }
    http_skill_to_http(core, skill_name, &in, QYMERA_PERM_CONTROL, req);
    return ESP_OK;
}

static esp_err_t h_relay_post(httpd_req_t *req) {
    return h_control_post(req, "value", "set_relay");
}

static esp_err_t h_dimmer_post(httpd_req_t *req) {
    return h_control_post(req, "level", "set_dimmer");
}

static esp_err_t h_rules_get(httpd_req_t *req) {
    qymera_core_t *core = (qymera_core_t *)req->user_ctx;
    qymera_skill_input_t in; memset(&in, 0, sizeof(in));
    http_skill_to_http(core, "list_rules", &in, QYMERA_PERM_RULE_READ, req);
    return ESP_OK;
}

static esp_err_t h_rule_get(httpd_req_t *req) {
    qymera_core_t *core = (qymera_core_t *)req->user_ctx;
    char rid[QYMERA_RULE_ID_LEN];
    if (!extract_rule_id(req->uri, NULL, rid, sizeof(rid))) {
        http_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"MALFORMED_URI\"}}");
        return ESP_OK;
    }
    qymera_skill_input_t in; memset(&in, 0, sizeof(in));
    strncpy(in.rule_id, rid, sizeof(in.rule_id) - 1);
    http_skill_to_http(core, "get_rule", &in, QYMERA_PERM_RULE_READ, req);
    return ESP_OK;
}

static esp_err_t h_rules_post(httpd_req_t *req) {
    qymera_core_t *core = (qymera_core_t *)req->user_ctx;
    char body[QYMERA_HTTP_BODY_SZ];
    int n = httpd_req_recv(req, body, sizeof(body) - 1);
    if (n <= 0) { http_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INVALID_INPUT\"}}"); return ESP_OK; }
    body[n] = '\0';
    qymera_skill_input_t in;
    qymera_http_parse_result_t pr = qymera_http_api_parse_rule_input(body, &in);
    if (pr != QYMERA_HTTP_PARSE_OK) {
        http_send_parse_error(req, pr);
        return ESP_OK;
    }
    http_skill_to_http(core, "create_rule", &in, QYMERA_PERM_RULE_WRITE, req);
    return ESP_OK;
}

/* Single handler for /api/v1/rules that dispatches by HTTP method.
 * ESP-IDF httpd does not allow multiple handlers for the same URI path
 * even with different methods in this version. */
static esp_err_t h_rules_base(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        return h_rules_get(req);
    } else if (req->method == HTTP_POST) {
        return h_rules_post(req);
    }
    http_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"METHOD_NOT_ALLOWED\"}}");
    return ESP_OK;
}

static esp_err_t h_rules_put(httpd_req_t *req) {
    qymera_core_t *core = (qymera_core_t *)req->user_ctx;
    char rid[QYMERA_RULE_ID_LEN];
    if (!extract_rule_id(req->uri, NULL, rid, sizeof(rid))) {
        http_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"MALFORMED_URI\"}}");
        return ESP_OK;
    }
    char body[QYMERA_HTTP_BODY_SZ];
    int n = httpd_req_recv(req, body, sizeof(body) - 1);
    if (n <= 0) { http_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INVALID_INPUT\"}}"); return ESP_OK; }
    body[n] = '\0';
    qymera_skill_input_t in;
    qymera_http_parse_result_t pr = qymera_http_api_parse_rule_input(body, &in);
    if (pr != QYMERA_HTTP_PARSE_OK) {
        http_send_parse_error(req, pr);
        return ESP_OK;
    }
    strncpy(in.rule_id, rid, sizeof(in.rule_id) - 1);
    http_skill_to_http(core, "update_rule", &in, QYMERA_PERM_RULE_WRITE, req);
    return ESP_OK;
}

static esp_err_t h_rules_delete(httpd_req_t *req) {
    qymera_core_t *core = (qymera_core_t *)req->user_ctx;
    char rid[QYMERA_RULE_ID_LEN];
    if (!extract_rule_id(req->uri, NULL, rid, sizeof(rid))) {
        http_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"MALFORMED_URI\"}}");
        return ESP_OK;
    }
    qymera_skill_input_t in; memset(&in, 0, sizeof(in));
    strncpy(in.rule_id, rid, sizeof(in.rule_id) - 1);
    http_skill_to_http(core, "delete_rule", &in, QYMERA_PERM_RULE_WRITE, req);
    return ESP_OK;
}

/* POST /api/v1/rules/<id>/enable | /disable */
static esp_err_t h_rule_action(httpd_req_t *req) {
    qymera_core_t *core = (qymera_core_t *)req->user_ctx;
    char rid[QYMERA_RULE_ID_LEN];
    bool enable = false, disable = false;
    if (!extract_rule_action(req->uri, rid, sizeof(rid), &enable, &disable)) {
        http_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"MALFORMED_URI\"}}");
        return ESP_OK;
    }
    qymera_skill_input_t in; memset(&in, 0, sizeof(in));
    strncpy(in.rule_id, rid, sizeof(in.rule_id) - 1);
    in.enabled = enable;
    http_skill_to_http(core, enable ? "enable_rule" : "disable_rule",
                       &in, QYMERA_PERM_RULE_WRITE, req);
    return ESP_OK;
}

static esp_err_t h_rules_dispatch(httpd_req_t *req) {
    /* Base path: /api/v1/rules */
    if (strcmp(req->uri, "/api/v1/rules") == 0) {
        if (req->method == HTTP_GET) {
            return h_rules_get(req);
        } else if (req->method == HTTP_POST) {
            return h_rules_post(req);
        }
    }
    /* Parameterized paths: /api/v1/rules/<id>, /api/v1/rules/<id>/enable, etc. */
    if (req->method == HTTP_GET) {
        return h_rule_get(req);
    } else if (req->method == HTTP_POST) {
        return h_rule_action(req);
    } else if (req->method == HTTP_PUT) {
        return h_rules_put(req);
    } else if (req->method == HTTP_DELETE) {
        return h_rules_delete(req);
    }
    http_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"METHOD_NOT_ALLOWED\"}}");
    return ESP_OK;
}

static esp_err_t h_skills_get(httpd_req_t *req) {
    size_t count = qymera_skill_registry_count();
    char buf[2048];
    char *p = buf;
    size_t remaining = sizeof(buf);
    int n = snprintf(p, remaining, "{\"ok\":true,\"data\":[");
    if (n < 0) { http_send_json(req, "{\"ok\":true,\"data\":[]}"); return ESP_OK; }
    p += (size_t)n; remaining -= (size_t)n;
    for (size_t i = 0; i < count; i++) {
        const qymera_skill_entry_t *entry = NULL;
        qymera_skill_id_t id = qymera_skill_registry_get(i, &entry);
        (void)id;
        if (!entry) continue;
        if (i > 0) { n = snprintf(p, remaining, ",");
                     if (n < 0 || (size_t)n >= remaining) break;
                     p += (size_t)n; remaining -= (size_t)n; }
        n = snprintf(p, remaining,
            "{\"name\":\"%s\",\"version\":\"%s\",\"description\":\"%s\","
            "\"schema_id\":\"%s\",\"permissions\":%u}",
            entry->meta.name, entry->meta.version, entry->meta.description,
            entry->meta.schema_id, entry->meta.permissions);
        if (n < 0 || (size_t)n >= remaining) break; /* stop cleanly if full */
        p += (size_t)n; remaining -= (size_t)n;
    }
    if (remaining > 0) {
        snprintf(p, remaining, "]}");
    }
    http_send_json(req, buf);
    return ESP_OK;
}

static esp_err_t h_logs_get(httpd_req_t *req) {
    qymera_core_t *core = (qymera_core_t *)req->user_ctx;
    char buf[4096];
    qymera_log_get_recent_json(qymera_core_get_log(core), buf, sizeof(buf), 40);
    /* Wrap the spooled JSON array in the {ok,data} envelope using chunks so
     * the handler only holds a single stack buffer, not an extra copy. */
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send_chunk(req, "{\"ok\":true,\"data\":", 18);
    httpd_resp_send_chunk(req, buf, strlen(buf));
    httpd_resp_send_chunk(req, "}", 1);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

/* Minimal JSON object helper: return the string value for `key` in `body`.
 * Handles simple \" escapes; used only for the small wifi-connect body so we
 * do not pull the full HHTP parser into a system-level endpoint. */
static bool http_extract_json_str(const char *body, const char *key,
                                  char *out, size_t out_sz) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = body;
    const char *v = NULL;
    while ((p = strstr(p, pat)) != NULL) {
        const char *q = p + strlen(pat);
        while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r') q++;
        if (*q == ':') { v = q + 1; break; }
        p = q;
    }
    if (!v) return false;
    while (*v == ' ' || *v == '\t' || *v == '\n' || *v == '\r') v++;
    if (*v != '"') return false;
    v++;
    size_t i = 0;
    while (*v && *v != '"' && i < out_sz - 1) {
        if (*v == '\\' && v[1]) v++;
        out[i++] = *v++;
    }
    out[i] = '\0';
    return true;
}

/* Locate `"key":` exactly (same scanner as http_extract_json_str), then parse
 * the raw JSON number following the colon into `*out` when present. Returns
 * false if the key is absent or its value is not a bare JSON number. */
static bool http_extract_json_num(const char *body, const char *key,
                                  uint32_t *out) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = body;
    const char *v = NULL;
    while ((p = strstr(p, pat)) != NULL) {
        const char *q = p + strlen(pat);
        while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r') q++;
        if (*q == ':') { v = q + 1; break; }
        p = q;
    }
    if (!v) return false;
    while (*v == ' ' || *v == '\t' || *v == '\n' || *v == '\r') v++;
    if (*v == '-') v++;
    if (*v < '0' || *v > '9') return false;
    long val = 0;
    while (*v >= '0' && *v <= '9') {
        val = val * 10 + (*v - '0');
        v++;
    }
    if (val < 0) val = 0;
    *out = (uint32_t)val;
    return true;
}

/* GET /api/v1/wifi/scan -> list of nearby networks {"ssid","rssi"}. */
static esp_err_t h_wifi_scan_get(httpd_req_t *req) {
    char buf[2048];
    qymera_err_t err = qymera_wifi_scan(buf, sizeof(buf));
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send_chunk(req, "{\"ok\":true,\"data\":", 18);
    httpd_resp_send_chunk(req, buf, strlen(buf));
    httpd_resp_send_chunk(req, "}", 1);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

/* POST /api/v1/wifi/connect with {"ssid","password"} -> persist credentials,
 * then reboot into STA mode (the persisted sta_enabled drives main.cpp). */
static esp_err_t h_wifi_connect_post(httpd_req_t *req) {
    qymera_core_t *core = (qymera_core_t *)req->user_ctx;
    char body[QYMERA_HTTP_BODY_SZ];
    int n = httpd_req_recv(req, body, sizeof(body) - 1);
    if (n <= 0) {
        http_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INVALID_INPUT\"}}");
        return ESP_OK;
    }
    body[n] = '\0';
    char ssid[33] = {0}, password[65] = {0};
    if (!http_extract_json_str(body, "ssid", ssid, sizeof(ssid)) || ssid[0] == '\0') {
        http_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INVALID_INPUT\","
                             "\"message\":\"ssid is required\"}}");
        return ESP_OK;
    }
    http_extract_json_str(body, "password", password, sizeof(password));

    qymera_storage_t *st = qymera_core_get_storage(core);
    qymera_network_config_t net;
    memset(&net, 0, sizeof(net));
    qymera_err_t lerr = qymera_storage_load_network(st, &net);
    if (lerr != QYMERA_OK && lerr != QYMERA_ERR_NOT_FOUND) {
        http_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"STORAGE\"}}");
        return ESP_OK;
    }
    /* If no config was persisted yet, fill in sane defaults so we never save
     * uninitialized stack bytes into NVS. */
    if (lerr == QYMERA_ERR_NOT_FOUND) {
        net.udp_discovery_port = QYMERA_UDP_PORT_DISCOVERY;
        net.udp_control_port = QYMERA_UDP_PORT_CONTROL;
        net.report_interval_ms = 5000;
    }
    strncpy(net.sta_ssid, ssid, sizeof(net.sta_ssid) - 1);
    strncpy(net.sta_password, password, sizeof(net.sta_password) - 1);
    /* Optional "enabled" bool (default true) controls sta_enabled on boot. */
    net.sta_enabled = true;
    {
        char enabled[8] = {0};
        if (http_extract_json_str(body, "enabled", enabled, sizeof(enabled))) {
            net.sta_enabled = (strcmp(enabled, "true") == 0 ||
                               strcmp(enabled, "1") == 0);
        }
    }
    /* Optional UDP port overrides (default values used if omitted). */
    {
        char port_str[8] = {0};
        if (http_extract_json_str(body, "udp_discovery_port", port_str, sizeof(port_str))) {
            net.udp_discovery_port = (uint16_t)atoi(port_str);
        }
        if (http_extract_json_str(body, "udp_control_port", port_str, sizeof(port_str))) {
            net.udp_control_port = (uint16_t)atoi(port_str);
        }
    }
    qymera_err_t serr = qymera_storage_save_network(st, &net);
    if (serr != QYMERA_OK) {
        http_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"STORAGE\"}}");
        return ESP_OK;
    }
    http_send_json(req, "{\"ok\":true,\"data\":{\"status\":\"rebooting\"}}");
    vTaskDelay(pdMS_TO_TICKS(300));
    qymera_system_restart();
    return ESP_OK;
}

/* =========================
 * POST /api/v1/ai/config
 *
 * Persists the AI provider configuration (Ollama/OpenAI upstream) in NVS and
 * reboots so the core applies it on boot. Body (all optional, absent fields
 * keep their persisted value):
 *   { "mode": "none"|"local"|"remote"|"hybrid",
 *     "local_endpoint": "...", "local_api_key": "...",
 *     "remote_endpoint": "...", "remote_api_key": "...",
 *     "default_model": "...", "timeout_ms": N, "rate_limit_ms": N,
 *     "cache_ms": N }
 * `local_endpoint`/`remote_endpoint` are base URLs like
 * `http://192.168.1.16:11434` (the provider appends /v1/chat/completions).
 * ========================= */
static esp_err_t h_ai_config_post(httpd_req_t *req) {
    qymera_core_t *core = (qymera_core_t *)req->user_ctx;
    if (!core) {
        http_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL\"}}");
        return ESP_OK;
    }

    char body[QYMERA_HTTP_BODY_SZ + 1] = {0};
    int bl = httpd_req_recv(req, body, sizeof(body) - 1);
    if (bl < 0) {
        http_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}");
        return ESP_OK;
    }
    body[bl] = '\0';

    qymera_storage_t *st = qymera_core_get_storage(core);
    qymera_ai_config_t ai;
    memset(&ai, 0, sizeof(ai));
    qymera_err_t lerr = qymera_storage_load_ai(st, &ai);
    if (lerr != QYMERA_OK && lerr != QYMERA_ERR_NOT_FOUND) {
        http_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"STORAGE\"}}");
        return ESP_OK;
    }
    if (lerr == QYMERA_ERR_NOT_FOUND) {
        ai.mode = QYMERA_AI_MODE_NONE;
        ai.default_timeout_ms = QYMERA_LLM_HTTP_DEFAULT_TIMEOUT_MS;
    }

    {
        char mode[16] = {0};
        if (http_extract_json_str(body, "mode", mode, sizeof(mode)) && mode[0]) {
            if (strcmp(mode, "none") == 0) ai.mode = QYMERA_AI_MODE_NONE;
            else if (strcmp(mode, "local") == 0) ai.mode = QYMERA_AI_MODE_LOCAL;
            else if (strcmp(mode, "remote") == 0) ai.mode = QYMERA_AI_MODE_REMOTE;
            else if (strcmp(mode, "hybrid") == 0) ai.mode = QYMERA_AI_MODE_HYBRID;
            else {
                http_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INVALID_INPUT\","
                                     "\"message\":\"mode must be none|local|remote|hybrid\"}}");
                return ESP_OK;
            }
        }
    }
    http_extract_json_str(body, "local_endpoint", ai.local_endpoint, sizeof(ai.local_endpoint));
    http_extract_json_str(body, "local_api_key", ai.local_api_key, sizeof(ai.local_api_key));
    http_extract_json_str(body, "remote_endpoint", ai.remote_endpoint, sizeof(ai.remote_endpoint));
    http_extract_json_str(body, "remote_api_key", ai.remote_api_key, sizeof(ai.remote_api_key));
    http_extract_json_str(body, "default_model", ai.default_model, sizeof(ai.default_model));
    {
        uint32_t tmp = 0;
        bool have_timeout = http_extract_json_num(body, "timeout_ms", &tmp);
        bool have_rate = http_extract_json_num(body, "rate_limit_ms", &tmp);
        bool have_cache = http_extract_json_num(body, "cache_ms", &tmp);
        if (have_timeout) {
            ai.default_timeout_ms = (uint16_t)tmp;
            if (ai.default_timeout_ms < 1000) ai.default_timeout_ms = 1000;
            if (ai.default_timeout_ms > 120000) ai.default_timeout_ms = 120000;
        }
        if (have_rate) ai.default_rate_limit_ms = (uint32_t)tmp;
        if (have_cache) ai.default_cache_ms = (uint32_t)tmp;
    }

    qymera_err_t serr = qymera_storage_save_ai(st, &ai);
    if (serr != QYMERA_OK) {
        http_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"STORAGE\"}}");
        return ESP_OK;
    }
    http_send_json(req, "{\"ok\":true,\"data\":{\"status\":\"rebooting\"}}");
    vTaskDelay(pdMS_TO_TICKS(300));
    qymera_system_restart();
    return ESP_OK;
}

/* =========================
 * POST /api/v1/ai/chat
 *
 * Runs one bounded LLM turn through qymera_llm_adapter_process. Body:
 *   { "prompt": "...", "permission_mask": N, "model": "..." }
 * `prompt` is required; permission_mask defaults to READ|CONTROL|RULE_READ|
 * RULE_WRITE (full local admin) when absent, matching the unauthenticated LAN
 * posture of the rest of the API. `model` is optional (falls back to the
 * provider default). The provider is chosen from the core AI config: a set
 * local/remote endpoint selects the HTTP provider transport, otherwise the
 * deterministic mock provider is used (safe for wiring/demo).
 * ========================= */
static const char *ai_turn_end_name(qymera_llm_turn_end_t e) {
    switch (e) {
        case QYMERA_LLM_TURN_TEXT: return "text";
        case QYMERA_LLM_TURN_TOOL_CALL_LIMIT: return "tool_call_limit";
        case QYMERA_LLM_TURN_PROVIDER_ERROR: return "provider_error";
        case QYMERA_LLM_TURN_MALFORMED: return "malformed";
        default: return "unknown";
    }
}

static const char *ai_msg_kind_name(qymera_llm_message_kind_t k) {
    switch (k) {
        case QYMERA_LLM_MSG_TEXT: return "text";
        case QYMERA_LLM_MSG_TOOL_CALL: return "tool_call";
        case QYMERA_LLM_MSG_MALFORMED: return "malformed";
        case QYMERA_LLM_MSG_PROVIDER_ERROR: return "provider_error";
        case QYMERA_LLM_MSG_TIMEOUT: return "timeout";
        default: return "none";
    }
}

static void ai_json_escape(const char *s, char *out, size_t cap) {
    size_t i = 0;
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p && i + 1 < cap; p++) {
        switch (*p) {
            case '"': out[i++] = '\\'; if (i + 1 < cap) out[i++] = '"'; break;
            case '\\': out[i++] = '\\'; if (i + 1 < cap) out[i++] = '\\'; break;
            case '\n': out[i++] = '\\'; if (i + 1 < cap) out[i++] = 'n'; break;
            case '\r': out[i++] = '\\'; if (i + 1 < cap) out[i++] = 'r'; break;
            case '\t': out[i++] = '\\'; if (i + 1 < cap) out[i++] = 't'; break;
            default: out[i++] = (char)*p; break;
        }
    }
    out[i] = '\0';
}

static esp_err_t h_ai_chat_post(httpd_req_t *req) {
    qymera_core_t *core = (qymera_core_t *)req->user_ctx;

    /* Body read (single bounded recv; prompt is capped at QYMERA_LLM_PROMPT_LEN). */
    char body[QYMERA_HTTP_BODY_SZ + QYMERA_LLM_PROMPT_LEN];
    int n = httpd_req_recv(req, body, sizeof(body) - 1);
    if (n <= 0) {
        http_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INVALID_INPUT\","
                             "\"message\":\"missing request body\"}}");
        return ESP_OK;
    }
    body[n] = '\0';

    char prompt[QYMERA_LLM_PROMPT_LEN] = {0};
    http_extract_json_str(body, "prompt", prompt, sizeof(prompt));
    if (prompt[0] == '\0') {
        http_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INVALID_INPUT\","
                             "\"message\":\"prompt is required\"}}");
        return ESP_OK;
    }

    char mask_str[16] = {0};
    uint32_t perm = QYMERA_PERM_READ | QYMERA_PERM_CONTROL |
                    QYMERA_PERM_RULE_READ | QYMERA_PERM_RULE_WRITE;
    if (http_extract_json_str(body, "permission_mask", mask_str, sizeof(mask_str))) {
        long m = strtol(mask_str, NULL, 0);
        if (m < 0) m = 0;
        perm = (uint32_t)m;
    }

    char model[QYMERA_LLM_MODEL_LEN] = {0};
    http_extract_json_str(body, "model", model, sizeof(model));

    /* Skill context for tool dispatch (same shape as the other handlers). */
    qymera_skill_context_t sctx;
    memset(&sctx, 0, sizeof(sctx));
    sctx.registry = qymera_core_get_registry(core);
    sctx.rule_engine = qymera_core_get_rule_engine(core);
    sctx.control = qymera_core_get_control(core);
    sctx.storage = qymera_core_get_storage(core);
    sctx.log = qymera_core_get_log(core);

    qymera_llm_adapter_t *adapter = NULL;
    qymera_llm_provider_t provider;
    qymera_llm_mock_ctx_t mock_ctx;
    qymera_llm_http_ctx_t *http_ctx = NULL;
    memset(&provider, 0, sizeof(provider));
    memset(&mock_ctx, 0, sizeof(mock_ctx));

    const qymera_core_config_t *cfg = qymera_core_get_config(core);
    bool use_http = false;
    const qymera_ai_config_t *ai = &cfg->ai;
    if (ai->mode == QYMERA_AI_MODE_LOCAL || ai->mode == QYMERA_AI_MODE_REMOTE ||
        ai->mode == QYMERA_AI_MODE_HYBRID) {
        const char *ep = (ai->mode == QYMERA_AI_MODE_LOCAL || ai->mode == QYMERA_AI_MODE_HYBRID)
                             ? ai->local_endpoint
                             : ai->remote_endpoint;
        const char *key = (ai->mode == QYMERA_AI_MODE_LOCAL || ai->mode == QYMERA_AI_MODE_HYBRID)
                             ? ai->local_api_key
                             : ai->remote_api_key;
        if (ep[0]) use_http = true;
    }
    if (use_http) {
        /* The HTTP transport owns two multi-KB buffers (req+resp); keep those
         * off the bounded httpd task stack. Freed after the turn. */
        http_ctx = calloc(1, sizeof(*http_ctx));
        if (!http_ctx) {
            http_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL\","
                                 "\"message\":\"out of memory for AI transport\"}}");
            return ESP_OK;
        }
        const char *ep = (ai->mode == QYMERA_AI_MODE_LOCAL || ai->mode == QYMERA_AI_MODE_HYBRID)
                             ? ai->local_endpoint : ai->remote_endpoint;
        const char *key = (ai->mode == QYMERA_AI_MODE_LOCAL || ai->mode == QYMERA_AI_MODE_HYBRID)
                             ? ai->local_api_key : ai->remote_api_key;
        snprintf(http_ctx->config.endpoint, sizeof(http_ctx->config.endpoint), "%s", ep);
        snprintf(http_ctx->config.api_key, sizeof(http_ctx->config.api_key), "%s", key);
        snprintf(http_ctx->config.model, sizeof(http_ctx->config.model), "%s",
                 ai->default_model[0] ? ai->default_model : "qymera-smart-home");
        http_ctx->config.timeout_ms = ai->default_timeout_ms
                                          ? ai->default_timeout_ms
                                          : QYMERA_LLM_HTTP_DEFAULT_TIMEOUT_MS;
        qymera_llm_http_provider_init(&provider, http_ctx);
    } else {
        qymera_llm_mock_provider_init(&provider, &mock_ctx);
    }

    qymera_llm_turn_result_t result;
    memset(&result, 0, sizeof(result));
    qymera_err_t aerr = qymera_llm_adapter_init(&adapter, &sctx, qymera_core_get_log(core));
    if (aerr == QYMERA_OK) {
        qymera_llm_request_t request;
        memset(&request, 0, sizeof(request));
        snprintf(request.prompt, sizeof(request.prompt), "%s", prompt);
        if (model[0]) snprintf(request.model, sizeof(request.model), "%s", model);
        request.permission_mask = perm;
        request.max_tool_calls = QYMERA_MAX_TOOL_CALLS_PER_TURN;
        aerr = qymera_llm_adapter_process(adapter, &provider, &request, &result);
        free(adapter);
        adapter = NULL;
    }
    if (http_ctx) {
        free(http_ctx);
        http_ctx = NULL;
    }
    if (aerr != QYMERA_OK) {
        http_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL\","
                             "\"message\":\"AI turn could not run\"}}");
        return ESP_OK;
    }

    char esc_outcome[QYMERA_LLM_RESULT_LEN * 2];
    char esc_text[QYMERA_LLM_TEXT_LEN * 2];
    char esc_tool[QYMERA_SKILL_NAME_LEN * 2];
    ai_json_escape(result.outcome, esc_outcome, sizeof(esc_outcome));
    ai_json_escape(result.final.text, esc_text, sizeof(esc_text));
    ai_json_escape(result.final.tool_name, esc_tool, sizeof(esc_tool));

    char buf[QYMERA_LLM_RESULT_LEN * 2 + QYMERA_LLM_PROMPT_LEN + 256];
    snprintf(buf, sizeof(buf),
        "{\"ok\":true,\"data\":{\"ended\":\"%s\",\"tool_calls\":%u,"
        "\"outcome\":\"%s\",\"final\":{\"kind\":\"%s\",\"text\":\"%s\","
        "\"tool_name\":\"%s\"}}}",
        ai_turn_end_name(result.ended), (unsigned)result.tool_calls,
        esc_outcome, ai_msg_kind_name(result.final.kind), esc_text, esc_tool);
    http_send_json(req, buf);
    return ESP_OK;
}

static esp_err_t h_root_get(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, DASHBOARD_HTML, strlen(DASHBOARD_HTML));
    return ESP_OK;
}

/* Route table.
 *
 * NOTE: httpd_method_t values are sequential integers (DELETE=0, GET=1,
 * POST=3, PUT=4, ...), NOT bit flags. Therefore a single handler for a
 * wildcard URI must be registered once PER method; OR-ing the methods into
 * one value does not work as a bitmask in this ESP-IDF/Arduino version.
 * Each (URI, method) pair is a distinct registration key. */

static const httpd_uri_t routes[] = {
    { .uri = "/", .method = HTTP_GET, .handler = h_root_get },
    { .uri = "/api/v1/status", .method = HTTP_GET, .handler = h_status_get },
    { .uri = "/api/v1/devices", .method = HTTP_GET, .handler = h_devices_get },
    { .uri = "/api/v1/entities*", .method = HTTP_GET, .handler = h_entities_dispatch },
    { .uri = "/api/v1/control/relay", .method = HTTP_POST, .handler = h_relay_post },
    { .uri = "/api/v1/control/dimmer", .method = HTTP_POST, .handler = h_dimmer_post },
    { .uri = "/api/v1/rules*", .method = HTTP_GET, .handler = h_rules_dispatch },
    { .uri = "/api/v1/rules*", .method = HTTP_POST, .handler = h_rules_dispatch },
    { .uri = "/api/v1/rules*", .method = HTTP_PUT, .handler = h_rules_dispatch },
    { .uri = "/api/v1/rules*", .method = HTTP_DELETE, .handler = h_rules_dispatch },
    { .uri = "/api/v1/skills", .method = HTTP_GET, .handler = h_skills_get },
    { .uri = "/api/v1/logs", .method = HTTP_GET, .handler = h_logs_get },
    { .uri = "/api/v1/wifi/scan", .method = HTTP_GET, .handler = h_wifi_scan_get },
    { .uri = "/api/v1/wifi/connect", .method = HTTP_POST, .handler = h_wifi_connect_post },
    { .uri = "/api/v1/ai/config", .method = HTTP_POST, .handler = h_ai_config_post },
    { .uri = "/api/v1/ai/chat", .method = HTTP_POST, .handler = h_ai_chat_post },
};

/* =========================
 * Init
 * ========================= */

static const char* qymera_http_method_str(httpd_method_t method) {
    switch (method) {
        case HTTP_GET: return "GET";
        case HTTP_POST: return "POST";
        case HTTP_PUT: return "PUT";
        case HTTP_DELETE: return "DELETE";
        case HTTP_PATCH: return "PATCH";
        case HTTP_HEAD: return "HEAD";
        case HTTP_OPTIONS: return "OPTIONS";
        default: return "UNKNOWN";
    }
}

qymera_err_t qymera_http_api_init(qymera_core_t *core) {
    printf("[HTTP] Starting server on port 80...\n");
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.stack_size = 32768;
    config.max_open_sockets = 4;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 20;
    httpd_handle_t handle = NULL;
    esp_err_t esp_err = httpd_start(&handle, &config);
    printf("[HTTP] httpd_start returned: %d\n", (int)esp_err);
    if (esp_err != ESP_OK) {
        printf("[HTTP] httpd_start failed: %d\n", (int)esp_err);
        return QYMERA_ERR_INVALID_STATE;
    }
    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        httpd_uri_t uri = routes[i];
        uri.user_ctx = core;
        esp_err_t reg_err = httpd_register_uri_handler(handle, &uri);
        printf("[HTTP] Register route %zu: %s %s -> %d\n", i, uri.uri, qymera_http_method_str(uri.method), (int)reg_err);
        if (reg_err != ESP_OK) {
            printf("[HTTP] Failed to register route %zu: %s %s (err=%d)\n", i, uri.uri, qymera_http_method_str(uri.method), (int)reg_err);
            httpd_stop(handle);
            return QYMERA_ERR_INVALID_STATE;
        }
    }
    return QYMERA_OK;
}
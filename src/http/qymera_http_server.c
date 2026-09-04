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

    char buf[512];
    snprintf(buf, sizeof(buf),
        "{\"ok\":true,\"data\":{\"free_heap\":%u,\"uptime_ms\":%u,"
        "\"ip\":\"%s\",\"network\":\"%s\",\"ssid\":\"%s\","
        "\"device_count\":%zu,\"entity_count\":%zu}}",
        heap, up, ip, network, ssid, dc, ec);
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
    config.stack_size = 16384;
    config.max_open_sockets = 4;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 16;
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
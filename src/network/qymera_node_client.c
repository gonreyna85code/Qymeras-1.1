/**
 * Qymera Dashboard - Node v1 HTTP Client (implementation)
 *
 * Plain-socket HTTP/1.1 client for the v1 Node application API. Uses an
 * internal bounded JSON scanner (no external parser dependency) so the module
 * runs on the ESP32 and can be mirrored by host tests and the mock Node in
 * tests/integration.
 */
#include "qymera_node_client.h"
#include "qymera_hal.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include "lwip/netdb.h"
#include "esp_timer.h"

#define NODE_NS     "qymera_cfg"
#define NODE_TARGETS_KEY "node_targets_v1"

/* =========================
 * Per-target runtime state
 * ========================= */

typedef struct {
    bool active;                 /* i < targets.count */
    uint32_t next_poll_ms;
    uint32_t consecutive_failures;
    char node_device_id[QYMERA_DEVICE_ID_LEN]; /* device synced for this target */
} qymera_node_target_state_t;

struct qymera_node_client_s {
    qymera_node_client_config_t cfg;
    qymera_node_target_set_t targets;
    qymera_node_target_state_t state[QYMERA_MAX_NODE_TARGETS];
    uint32_t polls_ok;
    uint32_t polls_fail;
    uint32_t commands_sent;
    uint32_t commands_err;
};

/* =========================
 * Minimal JSON helpers (bounded, defensive)
 * ========================= */

static const char *json_skip_ws(const char *p) {
    while (p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    return p;
}

/* Locate `"key":` exactly; returns pointer just after the ':'. */
static const char *json_find_key(const char *s, const char *key) {
    char pat[48];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = s;
    while (p && (p = strstr(p, pat)) != NULL) {
        const char *q = p + strlen(pat);
        q = json_skip_ws(q);
        if (*q == ':') return q + 1;
        p = q;
    }
    return NULL;
}

static bool json_parse_str(const char *p, char *out, size_t cap) {
    if (!p) return false;
    p = json_skip_ws(p);
    if (*p != '"') return false;
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i + 1 < cap) {
        if (*p == '\\' && p[1]) {
            switch (p[1]) {
                case 'n': out[i] = '\n'; break;
                case 'r': out[i] = '\r'; break;
                case 't': out[i] = '\t'; break;
                default:  out[i] = p[1]; break;
            }
            p += 2;
            i++;
            continue;
        }
        out[i++] = *p++;
    }
    if (*p != '"') return false;
    out[i] = '\0';
    return true;
}

static bool json_parse_num(const char *p, float *out) {
    if (!p) return false;
    p = json_skip_ws(p);
    if (*p != '-' && *p != '+' && !(*p >= '0' && *p <= '9')) return false;
    char buf[40];
    size_t i = 0;
    while (*p && i + 1 < sizeof(buf)) {
        char c = *p;
        if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E') {
            buf[i++] = c;
            p++;
        } else break;
    }
    buf[i] = '\0';
    if (i == 0) return false;
    char *end = NULL;
    float v = strtof(buf, &end);
    if (end == buf) return false;
    *out = v;
    return true;
}

static bool json_parse_bool(const char *p, bool *out) {
    if (!p) return false;
    p = json_skip_ws(p);
    if (strncmp(p, "true", 4) == 0) { *out = true; return true; }
    if (strncmp(p, "false", 5) == 0) { *out = false; return true; }
    return false;
}

/* Scan from *idx for the next top-level balanced '{' ... '}'; advances *idx. */
static bool json_next_object(const char *text, size_t *idx, const char **start, const char **end) {
    const char *p = text + *idx;
    while (*p && *p != '{') p++;
    if (!*p) return false;
    const char *st = p;
    int depth = 0;
    bool in_str = false;
    while (*p) {
        char c = *p;
        if (in_str) {
            if (c == '\\') { p++; }
            else if (c == '"') in_str = false;
        } else if (c == '"') {
            in_str = true;
        } else if (c == '{') {
            depth++;
        } else if (c == '}') {
            depth--;
            if (depth == 0) {
                *start = st;
                *end = p;
                *idx = (size_t)(p - text) + 1;
                return true;
            }
        }
        p++;
    }
    return false;
}

static qymera_entity_type_t type_from_str(const char *s) {
    static const char *names[] = {
        "none", "sensor.temperature", "sensor.humidity", "sensor.luminosity",
        "sensor.pressure", "sensor.level", "sensor.airq", "sensor.rain",
        "sensor.contact", "sensor.generic", "actuator.relay", "actuator.dimmer",
        "virtual.digital", "virtual.analog", "inference.result", "time"
    };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        if (strcmp(s, names[i]) == 0) return (qymera_entity_type_t)i;
    }
    return QYMERA_ENTITY_NONE;
}

static qymera_capability_t cap_from_str(const char *s) {
    if (strcmp(s, "sensor.numeric") == 0) return QYMERA_CAP_SENSOR_NUMERIC;
    if (strcmp(s, "sensor.digital") == 0) return QYMERA_CAP_SENSOR_DIGITAL;
    if (strcmp(s, "actuator.relay") == 0) return QYMERA_CAP_ACTUATOR_RELAY;
    if (strcmp(s, "actuator.dimmer") == 0) return QYMERA_CAP_ACTUATOR_DIMMER;
    if (strcmp(s, "actuator.generic") == 0) return QYMERA_CAP_ACTUATOR_GENERIC;
    if (strcmp(s, "inference.result") == 0) return QYMERA_CAP_INFERENCE_RESULT;
    if (strcmp(s, "time") == 0) return QYMERA_CAP_TIME_SOURCE;
    return QYMERA_CAP_NONE;
}

static uint8_t json_parse_capabilities(const char *chunk, qymera_capability_t *caps, uint8_t max) {
    const char *k = json_find_key(chunk, "capabilities");
    if (!k) return 0;
    k = json_skip_ws(k);
    if (*k != '[') return 0;
    const char *p = k + 1;
    uint8_t n = 0;
    while (*p) {
        p = json_skip_ws(p);
        if (*p == ']' || *p == '\0') break;
        if (*p == '"') {
            char buf[24];
            if (json_parse_str(p, buf, sizeof(buf))) {
                qymera_capability_t c = cap_from_str(buf);
                if (c != QYMERA_CAP_NONE && n < max) caps[n++] = c;
            }
            while (*p && *p != '"') { if (*p == '\\') p++; p++; }
            if (*p == '"') p++;
        } else {
            p++;
        }
        p = json_skip_ws(p);
        if (*p == ',') p++;
    }
    return n;
}

/* native min/max ship inside the entity's "config":{...}; the scanner reads
 * them from the entity chunk directly (keys are unique in the object). */
static void parse_config_minmax(const char *chunk, float *min, float *max) {
    const char *k = json_find_key(chunk, "native_min");
    if (!json_parse_num(k, min)) *min = 0.0f;
    k = json_find_key(chunk, "native_max");
    if (!json_parse_num(k, max)) *max = 0.0f;
}

/* =========================
 * HTTP transport
 * ========================= */

static bool http_open(const char *host, uint16_t port, int *fd_out) {
    if (port == 0) port = 80;
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);

    if (inet_pton(AF_INET, host, &sa.sin_addr) != 1) {
        struct hostent *he = gethostbyname(host);
        if (!he || !he->h_addr_list[0]) return false;
        memcpy(&sa.sin_addr, he->h_addr_list[0], sizeof(sa.sin_addr));
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return false;

    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 500000;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) != 0) {
        close(fd);
        return false;
    }
    *fd_out = fd;
    return true;
}

static int http_read_chunk(int fd, char *buf, size_t cap) {
    ssize_t n = recv(fd, buf, cap, 0);
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return 0;
    return (int)n;
}

/* Perform one request. Returns QYMERA_OK if the round-trip completed (any
 * HTTP status); the response body is returned in resp (headers stripped). */
static qymera_err_t http_request(qymera_node_client_t *client,
                                 const char *host, uint16_t port,
                                 const char *method, const char *path,
                                 const char *body,
                                 char *resp, size_t resp_cap) {
    (void)client;
    int fd;
    if (!http_open(host, port, &fd)) return QYMERA_ERR_NETWORK;

    uint32_t deadline_ms = (uint32_t)(esp_timer_get_time() / 1000) + QYMERA_NODE_TIMEOUT_MS;

    char req[1536];
    int n = snprintf(req, sizeof(req),
                     "%s %s HTTP/1.1\r\nHost: %s:%u\r\nConnection: close\r\n",
                     method, path, host, (unsigned)port);
    if (body) {
        n += snprintf(req + n, sizeof(req) - (size_t)n,
                      "Content-Type: application/json\r\nContent-Length: %d\r\n",
                      (int)strlen(body));
    }
    n += snprintf(req + n, sizeof(req) - (size_t)n, "\r\n%s", body ? body : "");
    if (n < 0 || (size_t)n >= sizeof(req)) {
        close(fd);
        return QYMERA_ERR_INVALID_ARG;
    }

    size_t sent = 0;
    size_t total = (size_t)n;
    while (sent < total) {
        ssize_t w = send(fd, req + sent, total - sent, 0);
        if (w <= 0) { close(fd); return QYMERA_ERR_NETWORK; }
        sent += (size_t)w;
    }

    resp[0] = '\0';
    size_t filled = 0;
    while (filled + 1 < resp_cap) {
        if ((uint32_t)(esp_timer_get_time() / 1000) >= deadline_ms) break;
        int r = http_read_chunk(fd, resp + filled, resp_cap - filled - 1);
        if (r > 0) {
            filled += (size_t)r;
        } else if (r == 0) {
            continue;
        } else {
            break;
        }
    }
    resp[filled] = '\0';
    close(fd);

    if (filled == 0) return QYMERA_ERR_TIMEOUT;

    char *hdr_end = strstr(resp, "\r\n\r\n");
    if (hdr_end) {
        size_t body_len = filled - (size_t)(hdr_end + 4 - resp);
        memmove(resp, hdr_end + 4, body_len);
        resp[body_len] = '\0';
    }
    return (resp[0] == '{' || resp[0] == '[') ? QYMERA_OK : QYMERA_ERR_PROTOCOL;
}

/* =========================
 * Device/entity ingestion
 * ========================= */

/* Mark entities of `device_id` not present in `seen` with `reliability`. */
static void node_mark_entities(qymera_node_client_t *client, const char *device_id,
                               const uint16_t *seen, size_t seen_count, uint8_t reliability) {
    if (!device_id || !device_id[0]) return;
    qymera_registry_t *reg = client->cfg.registry;
    uint16_t dev_idx;
    if (qymera_registry_find_device(reg, device_id, &dev_idx) != QYMERA_OK) return;

    for (uint16_t i = 0; i < QYMERA_MAX_ENTITIES; i++) {
        qymera_entity_t e;
        if (qymera_registry_get_entity(reg, i, &e) != QYMERA_OK) continue;
        if (strcmp(e.device_id, device_id) != 0) continue;
        bool found = false;
        for (size_t s = 0; s < seen_count; s++) {
            if (seen[s] == i) { found = true; break; }
        }
        if (found) continue;
        if (e.value.reliability != reliability) {
            qymera_entity_value_t v = e.value;
            v.reliability = reliability;
            if (reliability == QYMERA_RELIABILITY_STALE) v.valid = true;
            qymera_registry_update_entity_value(reg, i, &v);
        }
    }
}

static qymera_err_t node_fetch_status(qymera_node_client_t *client, uint16_t target_idx) {
    const qymera_node_target_t *t = &client->targets.targets[target_idx];
    char resp[QYMERA_NODE_HTTP_BUF];
    qymera_err_t err = http_request(client, t->host, t->port, "GET", "/api/v1/status",
                                    NULL, resp, sizeof(resp));
    if (err != QYMERA_OK) return err;

    char device_id[QYMERA_DEVICE_ID_LEN] = {0};
    if (!json_parse_str(json_find_key(resp, "device_id"), device_id, sizeof(device_id))) {
        return QYMERA_ERR_PROTOCOL;
    }

    char name[QYMERA_DEVICE_ID_LEN] = {0};
    char model[32] = {0};
    char fw[32] = {0};
    char api[16] = {0};
    char proto[16] = {0};
    char ip[16] = {0};
    json_parse_str(json_find_key(resp, "name"), name, sizeof(name));
    json_parse_str(json_find_key(resp, "model"), model, sizeof(model));
    json_parse_str(json_find_key(resp, "firmware_version"), fw, sizeof(fw));
    json_parse_str(json_find_key(resp, "api_version"), api, sizeof(api));
    json_parse_str(json_find_key(resp, "protocol_version"), proto, sizeof(proto));
    json_parse_str(json_find_key(resp, "ip"), ip, sizeof(ip));

    qymera_registry_t *reg = client->cfg.registry;
    uint16_t dev_idx;
    if (qymera_registry_find_device(reg, device_id, &dev_idx) == QYMERA_OK) {
        qymera_device_t dev;
        if (qymera_registry_get_device(reg, dev_idx, &dev) == QYMERA_OK) {
            if (model[0]) snprintf(dev.model, sizeof(dev.model), "%s", model);
            if (fw[0]) snprintf(dev.fw_version, sizeof(dev.fw_version), "%s", fw);
            if (api[0]) snprintf(dev.api_version, sizeof(dev.api_version), "%s", api);
            if (proto[0]) snprintf(dev.protocol_version, sizeof(dev.protocol_version), "%s", proto);
            if (ip[0]) snprintf(dev.ip_addr, sizeof(dev.ip_addr), "%s", ip);
            dev.port = t->port;
            dev.online = true;
            dev.state = 0;
            qymera_registry_update_seen(reg, dev_idx);
            qymera_registry_set_online(reg, dev_idx, true);
        }
    } else {
        qymera_device_t dev;
        memset(&dev, 0, sizeof(dev));
        snprintf(dev.device_id, sizeof(dev.device_id), "%s", device_id);
        snprintf(dev.name, sizeof(dev.name), "%s", name[0] ? name : device_id);
        snprintf(dev.model, sizeof(dev.model), "%s", model);
        snprintf(dev.fw_version, sizeof(dev.fw_version), "%s", fw);
        snprintf(dev.api_version, sizeof(dev.api_version), "%s", api);
        snprintf(dev.protocol_version, sizeof(dev.protocol_version), "%s", proto);
        snprintf(dev.ip_addr, sizeof(dev.ip_addr), "%s", ip[0] ? ip : t->host);
        dev.port = t->port;
        dev.role = 1;
        dev.state = 0;
        dev.online = true;
        dev.registered_at = qymera_timestamp_now();
        dev.last_seen = dev.registered_at;
        if (qymera_registry_register_device(reg, &dev, &dev_idx) != QYMERA_OK) {
            return QYMERA_ERR_NO_SPACE;
        }
    }

    strncpy(client->state[target_idx].node_device_id, device_id, QYMERA_DEVICE_ID_LEN - 1);
    client->state[target_idx].node_device_id[QYMERA_DEVICE_ID_LEN - 1] = '\0';
    return QYMERA_OK;
}

static qymera_err_t node_fetch_entities(qymera_node_client_t *client, uint16_t target_idx) {
    const qymera_node_target_t *t = &client->targets.targets[target_idx];
    const char *node_device_id = client->state[target_idx].node_device_id;
    char resp[QYMERA_NODE_HTTP_BUF];
    qymera_err_t err = http_request(client, t->host, t->port, "GET", "/api/v1/entities",
                                    NULL, resp, sizeof(resp));
    if (err != QYMERA_OK) return err;

    const char *data = json_find_key(resp, "data");
    if (!data) return QYMERA_ERR_PROTOCOL;
    data = json_skip_ws(data);
    if (*data != '[') return QYMERA_ERR_PROTOCOL;

    qymera_registry_t *reg = client->cfg.registry;
    uint16_t seen[QYMERA_MAX_ENTITIES];
    size_t seen_count = 0;

    size_t idx = (size_t)(data - resp + 1);
    const char *start = NULL, *end = NULL;
    while (json_next_object(resp, &idx, &start, &end)) {
        char entity_id[QYMERA_ENTITY_ID_LEN] = {0};
        char device_id[QYMERA_DEVICE_ID_LEN] = {0};
        char name[QYMERA_ENTITY_ID_LEN] = {0};
        char type_str[24] = {0};
        char unit[16] = {0};
        float native_min = 0.0f, native_max = 0.0f;

        if (!json_parse_str(json_find_key(start, "entity_id"), entity_id, sizeof(entity_id))) continue;
        json_parse_str(json_find_key(start, "device_id"), device_id, sizeof(device_id));
        json_parse_str(json_find_key(start, "name"), name, sizeof(name));
        json_parse_str(json_find_key(start, "type"), type_str, sizeof(type_str));
        json_parse_str(json_find_key(start, "unit"), unit, sizeof(unit));
        parse_config_minmax(start, &native_min, &native_max);

        if (!device_id[0]) snprintf(device_id, sizeof(device_id), "%s", node_device_id);

        qymera_entity_type_t etype = type_from_str(type_str);
        qymera_capability_t caps[4] = {0};
        uint8_t ncap = json_parse_capabilities(start, caps, 4);
        if (ncap == 0) {
            if (etype == QYMERA_ENTITY_ACTUATOR_RELAY) { caps[0] = QYMERA_CAP_ACTUATOR_RELAY; ncap = 1; }
            else if (etype == QYMERA_ENTITY_ACTUATOR_DIMMER) { caps[0] = QYMERA_CAP_ACTUATOR_DIMMER; ncap = 1; }
            else if (etype >= QYMERA_ENTITY_SENSOR_TEMPERATURE && etype <= QYMERA_ENTITY_SENSOR_GENERIC) {
                caps[0] = QYMERA_CAP_SENSOR_NUMERIC; ncap = 1;
            }
        }

        const char *val_key = json_find_key(start, "value");
        float numval = 0.0f;
        bool has_num = json_parse_num(val_key, &numval);
        bool raw_bool = false;
        bool has_bool = has_num ? false : json_parse_bool(val_key, &raw_bool);

        bool available = true;
        const char *avail_key = json_find_key(start, "available");
        json_parse_bool(avail_key, &available);

        uint16_t dev_idx;
        if (qymera_registry_find_device(reg, device_id, &dev_idx) != QYMERA_OK) {
            qymera_device_t dev;
            memset(&dev, 0, sizeof(dev));
            snprintf(dev.device_id, sizeof(dev.device_id), "%s", device_id);
            snprintf(dev.name, sizeof(dev.name), "%s", device_id);
            snprintf(dev.ip_addr, sizeof(dev.ip_addr), "%s", t->host);
            dev.port = t->port;
            dev.role = 1;
            dev.online = true;
            dev.registered_at = qymera_timestamp_now();
            dev.last_seen = dev.registered_at;
            if (qymera_registry_register_device(reg, &dev, &dev_idx) != QYMERA_OK) continue;
        }

        qymera_entity_t ent;
        uint16_t ent_idx;
        if (qymera_registry_find_entity(reg, device_id, entity_id, &ent_idx) == QYMERA_OK) {
            qymera_registry_get_entity(reg, ent_idx, &ent);
        } else {
            memset(&ent, 0, sizeof(ent));
            snprintf(ent.device_id, sizeof(ent.device_id), "%s", device_id);
            snprintf(ent.entity_id, sizeof(ent.entity_id), "%s", entity_id);
            snprintf(ent.name, sizeof(ent.name), "%s", name[0] ? name : entity_id);
            ent.type = etype;
            ent.native_min = native_min;
            ent.native_max = native_max;
            for (uint8_t c = 0; c < ncap && c < QYMERA_ARRAY_SIZE(ent.capabilities); c++) {
                ent.capabilities[c] = caps[c];
            }
            ent.capability_count = ncap;
            if (unit[0]) snprintf(ent.unit, sizeof(ent.unit), "%s", unit);
            if (qymera_registry_register_entity(reg, dev_idx, &ent, &ent_idx) != QYMERA_OK) continue;
        }

        qymera_entity_value_t value = {0};
        value.valid = available;
        value.timestamp = qymera_timestamp_now();
        if (has_num) {
            value.numeric_value = numval;
            value.bool_value = (numval != 0.0f);
        } else if (has_bool) {
            value.bool_value = raw_bool;
            value.numeric_value = raw_bool ? 1.0f : 0.0f;
        } else {
            value.valid = false;
        }
        value.reliability = available ? QYMERA_RELIABILITY_CONFIRMED : QYMERA_RELIABILITY_STALE;
        qymera_registry_update_entity_value(reg, ent_idx, &value);

        if (seen_count < QYMERA_MAX_ENTITIES) seen[seen_count++] = ent_idx;

        if (client->cfg.on_entity_state) {
            client->cfg.on_entity_state(client->cfg.callback_ctx, device_id, entity_id,
                                        available, (uint8_t)etype,
                                        value.numeric_value, value.bool_value);
        }
    }

    node_mark_entities(client, node_device_id, seen, seen_count, QYMERA_RELIABILITY_STALE);
    return QYMERA_OK;
}

static void node_mark_offline(qymera_node_client_t *client, uint16_t target_idx) {
    const char *device_id = client->state[target_idx].node_device_id;
    if (!device_id || !device_id[0]) return;
    qymera_registry_t *reg = client->cfg.registry;
    uint16_t dev_idx;
    if (qymera_registry_find_device(reg, device_id, &dev_idx) == QYMERA_OK) {
        qymera_registry_set_online(reg, dev_idx, false);
        node_mark_entities(client, device_id, NULL, 0, QYMERA_RELIABILITY_OFFLINE);
        if (client->cfg.on_device_online) {
            client->cfg.on_device_online(client->cfg.callback_ctx, device_id, false);
        }
    }
}

/* =========================
 * Public API
 * ========================= */

qymera_err_t qymera_node_client_init(qymera_node_client_t **client,
                                     const qymera_node_client_config_t *config) {
    if (!client || !config || !config->registry) return QYMERA_ERR_INVALID_ARG;
    qymera_node_client_t *c = calloc(1, sizeof(*c));
    if (!c) return QYMERA_ERR_NO_SPACE;
    c->cfg = *config;
    qymera_err_t err = qymera_node_client_load_targets(c);
    if (err != QYMERA_OK && err != QYMERA_ERR_NOT_FOUND) {
        free(c);
        return err;
    }
    *client = c;
    return QYMERA_OK;
}

void qymera_node_client_cleanup(qymera_node_client_t *client) {
    if (!client) return;
    free(client);
}

qymera_err_t qymera_node_client_set_targets(qymera_node_client_t *client,
                                            const qymera_node_target_set_t *set) {
    if (!client || !set || set->count > QYMERA_MAX_NODE_TARGETS) return QYMERA_ERR_INVALID_ARG;
    client->targets = *set;
    for (uint8_t i = 0; i < QYMERA_MAX_NODE_TARGETS; i++) {
        client->state[i].active = (i < set->count);
        if (i < set->count && client->targets.targets[i].poll_interval_ms == 0) {
            client->targets.targets[i].poll_interval_ms = QYMERA_NODE_POLL_MS;
        }
    }
    if (client->cfg.log) {
        qymera_log_info(client->cfg.log, "node", "node targets set (%u)", set->count);
    }
    return qymera_storage_put_blob(client->cfg.storage, NODE_NS, NODE_TARGETS_KEY,
                                   &client->targets, sizeof(client->targets));
}

qymera_err_t qymera_node_client_load_targets(qymera_node_client_t *client) {
    if (!client || !client->cfg.storage) return QYMERA_ERR_INVALID_ARG;
    memset(&client->targets, 0, sizeof(client->targets));
    size_t len = sizeof(client->targets);
    qymera_err_t err = qymera_storage_get_blob(client->cfg.storage, NODE_NS, NODE_TARGETS_KEY,
                                               &client->targets, &len);
    if (err == QYMERA_OK) {
        if (len != sizeof(client->targets) || client->targets.count > QYMERA_MAX_NODE_TARGETS) {
            memset(&client->targets, 0, sizeof(client->targets));
            return QYMERA_ERR_STORAGE;
        }
        for (uint8_t i = 0; i < QYMERA_MAX_NODE_TARGETS; i++) {
            client->state[i].active = (i < client->targets.count);
        }
        for (uint8_t i = 0; i < client->targets.count; i++) {
            if (client->targets.targets[i].poll_interval_ms == 0) {
                client->targets.targets[i].poll_interval_ms = QYMERA_NODE_POLL_MS;
            }
        }
        return QYMERA_OK;
    }
    if (err == QYMERA_ERR_NOT_FOUND) {
        qymera_node_target_set_t empty;
        memset(&empty, 0, sizeof(empty));
        qymera_storage_put_blob(client->cfg.storage, NODE_NS, NODE_TARGETS_KEY, &empty, sizeof(empty));
        return QYMERA_ERR_NOT_FOUND;
    }
    return err;
}

qymera_err_t qymera_node_client_get_targets(const qymera_node_client_t *client,
                                            qymera_node_target_set_t *out) {
    if (!client || !out) return QYMERA_ERR_INVALID_ARG;
    *out = client->targets;
    return QYMERA_OK;
}

void qymera_node_client_tick(qymera_node_client_t *client) {
    if (!client) return;
    uint32_t now_ms = qymera_system_get_uptime_ms();
    for (uint8_t i = 0; i < client->targets.count && i < QYMERA_MAX_NODE_TARGETS; i++) {
        if (!client->state[i].active) continue;
        uint32_t interval = client->targets.targets[i].poll_interval_ms
                                ? client->targets.targets[i].poll_interval_ms
                                : QYMERA_NODE_POLL_MS;
        if (now_ms < client->state[i].next_poll_ms) continue;
        client->state[i].next_poll_ms = now_ms + interval;

        qymera_err_t err = node_fetch_status(client, i);
        if (err == QYMERA_OK) {
            err = node_fetch_entities(client, i);
        }
        if (err == QYMERA_OK) {
            client->polls_ok++;
            client->state[i].consecutive_failures = 0;
            if (client->cfg.log) {
                qymera_log_debug(client->cfg.log, "node", "node %s poll OK",
                                 client->targets.targets[i].host);
            }
        } else {
            client->polls_fail++;
            client->state[i].consecutive_failures++;
            if (client->cfg.log) {
                qymera_log_warn(client->cfg.log, "node", "node %s poll failed (%d)",
                                client->targets.targets[i].host, (int)err);
            }
            node_mark_offline(client, i);
        }
    }
}

qymera_err_t qymera_node_client_send_command(qymera_node_client_t *client,
                                             const char *host, uint16_t port,
                                             const char *device_id,
                                             const char *entity_id,
                                             uint8_t opcode, float value_f,
                                             bool *accepted,
                                             char *err_code, size_t err_sz) {
    if (accepted) *accepted = false;
    if (!client || !host || !entity_id) return QYMERA_ERR_INVALID_ARG;

    for (const char *p = entity_id; *p; p++) {
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
              (*p >= '0' && *p <= '9') || *p == '-' || *p == '_' || *p == '.')) {
            return QYMERA_ERR_INVALID_ARG;
        }
    }

    char path[96];
    snprintf(path, sizeof(path), "/api/v1/entities/%s/command", entity_id);

    char body[256];
    if (opcode == 1) {
        snprintf(body, sizeof(body),
                 "{\"action\":\"set_relay\",\"value\":%s,\"device_id\":\"%s\"}",
                 value_f != 0.0f ? "true" : "false", device_id ? device_id : "");
    } else {
        snprintf(body, sizeof(body),
                 "{\"action\":\"set_dimmer\",\"value\":%.0f,\"device_id\":\"%s\"}",
                 (double)value_f, device_id ? device_id : "");
    }

    char resp[QYMERA_NODE_HTTP_BUF];
    qymera_err_t err = http_request(client, host, port, "POST", path, body, resp, sizeof(resp));
    if (err != QYMERA_OK) {
        client->commands_err++;
        return err;
    }

    client->commands_sent++;

    bool ok = false;
    if (json_parse_bool(json_find_key(resp, "ok"), &ok) && !ok) {
        if (err_code && err_sz) {
            char code[24] = {0};
            json_parse_str(json_find_key(resp, "code"), code, sizeof(code));
            snprintf(err_code, err_sz, "%s", code[0] ? code : "INTERNAL_ERROR");
        }
        client->commands_err++;
        return QYMERA_OK;
    }

    *accepted = false;
    json_parse_bool(json_find_key(resp, "accepted"), accepted);
    if (!*accepted) {
        if (err_code && err_sz) {
            char code[24] = {0};
            json_parse_str(json_find_key(resp, "code"), code, sizeof(code));
            snprintf(err_code, err_sz, "%s", code[0] ? code : "COMMAND_REJECTED");
        }
        client->commands_err++;
    }
    return QYMERA_OK;
}

void qymera_node_client_stats(const qymera_node_client_t *client,
                              uint32_t *polls_ok, uint32_t *polls_fail,
                              uint32_t *commands_sent, uint32_t *commands_err) {
    if (polls_ok) *polls_ok = client ? client->polls_ok : 0;
    if (polls_fail) *polls_fail = client ? client->polls_fail : 0;
    if (commands_sent) *commands_sent = client ? client->commands_sent : 0;
    if (commands_err) *commands_err = client ? client->commands_err : 0;
}
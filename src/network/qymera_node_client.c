/**
 * Qymera Dashboard - Node HTTP Client (implementation)
 *
 * Plain-socket HTTP/1.1 client for the authoritative Qymera 1.0.0 Node API
 * (github.com/gonreyna85code/Qymera). Uses an internal bounded JSON scanner
 * (no external parser dependency) so the module runs on the ESP32 and can be
 * mirrored by host tests and the mock Node in tests/integration.
 *
 * Wire contract:
 *   GET  /calib      -> BARE JSON array of entities (no envelope):
 *                      id, index, device_uid, name, value, correction, avail,
 *                      pulse, state, pulse_ms, persist, fade, type(1..12),
 *                      local, age_ms, ip.
 *   GET  /firmware   -> {product, version, platform, state, latest, channel,
 *                      available, progress, error}.
 *   POST /toggle     -> form id=<uid>; HTTP 200 text/plain "OK" flips state.
 *   POST /dimmer     -> form id=<uid>&value=0..100; HTTP 200 text/plain "OK".
 *   Errors are HTTP status only (400/401/404/405/429); no JSON error codes.
 */
#include "qymera_node_client.h"
#include "qymera_hal.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include "lwip/netdb.h"
#include "esp_timer.h"

#define NODE_NS     "qymera_cfg"
#define NODE_TARGETS_KEY "node_targets_v1"

#define QYMERA_MAX_NODE_DEVICES 4 /* distinct device_uids tracked per target */

/* =========================
 * Per-target runtime state
 * ========================= */

typedef struct {
    bool active;                 /* i < targets.count */
    uint32_t next_poll_ms;
    uint32_t consecutive_failures;
    /* device_uids (as decimal strings) whose entities appeared in the last
     * successful /calib snapshot; used to offline devices on poll failure. */
    char node_device_ids[QYMERA_MAX_NODE_DEVICES][QYMERA_DEVICE_ID_LEN];
    uint8_t node_device_count;
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

static bool json_parse_uint(const char *p, uint32_t *out) {
    if (!p) return false;
    p = json_skip_ws(p);
    if (*p < '0' || *p > '9') return false;
    uint32_t v = 0;
    while (*p >= '0' && *p <= '9') {
        if (v > (UINT32_MAX - (uint32_t)(*p - '0')) / 10) return false;
        v = v * 10 + (uint32_t)(*p - '0');
        p++;
    }
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

/* =========================
 * Calib type mapping (firmware `type` int 1..12)
 * ========================= */

static qymera_entity_type_t calib_type_to_entity(uint32_t t) {
    switch (t) {
        case 1:  return QYMERA_ENTITY_SENSOR_LUMINOSITY;   /* LUMI   */
        case 2:  return QYMERA_ENTITY_SENSOR_HUMIDITY;     /* HUMI   */
        case 3:  return QYMERA_ENTITY_SENSOR_TEMPERATURE;  /* TEMP   */
        case 4:  return QYMERA_ENTITY_SENSOR_PRESSURE;     /* PRESS  */
        case 5:  return QYMERA_ENTITY_SENSOR_LEVEL;        /* LEVEL  */
        case 6:  return QYMERA_ENTITY_SENSOR_AIRQ;         /* AIRQ   */
        case 7:  return QYMERA_ENTITY_SENSOR_RAIN;         /* RAIN   */
        case 8:  return QYMERA_ENTITY_ACTUATOR_DIMMER;     /* DIMMER */
        case 9:  return QYMERA_ENTITY_ACTUATOR_RELAY;      /* RELAY  */
        case 10: return QYMERA_ENTITY_TIME;                /* TIME   */
        case 11: return QYMERA_ENTITY_SENSOR_GENERIC;      /* GENERIC*/
        case 12: return QYMERA_ENTITY_SENSOR_CONTACT;      /* CONTACT*/
        default: return QYMERA_ENTITY_NONE;
    }
}

static void calib_type_caps(uint32_t t, qymera_capability_t *caps, uint8_t *ncap) {
    caps[0] = QYMERA_CAP_SENSOR_NUMERIC;
    switch (t) {
        case 8:  caps[0] = QYMERA_CAP_ACTUATOR_DIMMER; break;
        case 9:  caps[0] = QYMERA_CAP_ACTUATOR_RELAY;  break;
        case 10: caps[0] = QYMERA_CAP_TIME_SOURCE;     break;
        case 12: caps[0] = QYMERA_CAP_SENSOR_DIGITAL;  break;
        default: break;
    }
    *ncap = 1;
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
 * HTTP status); the HTTP status code is returned in *status_out (0 if it
 * could not be parsed) and the response body (headers stripped) in resp.
 * `content_type` is used for the POST body (default form-urlencoded). */
static qymera_err_t http_request(qymera_node_client_t *client,
                                 const char *host, uint16_t port,
                                 const char *method, const char *path,
                                 const char *body, const char *content_type,
                                 char *resp, size_t resp_cap, int *status_out) {
    (void)client;
    if (status_out) *status_out = 0;
    int fd;
    if (!http_open(host, port, &fd)) return QYMERA_ERR_NETWORK;

    uint32_t deadline_ms = (uint32_t)(esp_timer_get_time() / 1000) + QYMERA_NODE_TIMEOUT_MS;

    char req[1536];
    int n = snprintf(req, sizeof(req),
                     "%s %s HTTP/1.1\r\nHost: %s:%u\r\nConnection: close\r\n",
                     method, path, host, (unsigned)port);
    if (body) {
        n += snprintf(req + n, sizeof(req) - (size_t)n,
                      "Content-Type: %s\r\nContent-Length: %d\r\n",
                      content_type ? content_type : "application/x-www-form-urlencoded",
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

    if (status_out) {
        *status_out = 0;
        if (strncmp(resp, "HTTP/", 5) == 0) {
            const char *sp = strchr(resp, ' ');
            if (sp) *status_out = atoi(sp + 1);
        }
    }

    char *hdr_end = strstr(resp, "\r\n\r\n");
    if (hdr_end) {
        size_t body_len = filled - (size_t)(hdr_end + 4 - resp);
        memmove(resp, hdr_end + 4, body_len);
        resp[body_len] = '\0';
    }
    return QYMERA_OK;
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

static qymera_err_t node_fetch_firmware(qymera_node_client_t *client, uint16_t target_idx) {
    const qymera_node_target_t *t = &client->targets.targets[target_idx];
    char resp[QYMERA_NODE_HTTP_BUF];
    int status = 0;
    qymera_err_t err = http_request(client, t->host, t->port, "GET", "/firmware",
                                    NULL, NULL, resp, sizeof(resp), &status);
    if (err != QYMERA_OK || status != 200 || resp[0] != '{') return QYMERA_ERR_PROTOCOL;

    char product[QYMERA_DEVICE_ID_LEN] = {0};
    char version[32] = {0};
    char platform[32] = {0};
    json_parse_str(json_find_key(resp, "product"), product, sizeof(product));
    json_parse_str(json_find_key(resp, "version"), version, sizeof(version));
    json_parse_str(json_find_key(resp, "platform"), platform, sizeof(platform));

    qymera_registry_t *reg = client->cfg.registry;
    for (uint8_t i = 0; i < client->state[target_idx].node_device_count; i++) {
        const char *device_id = client->state[target_idx].node_device_ids[i];
        uint16_t dev_idx;
        if (qymera_registry_find_device(reg, device_id, &dev_idx) != QYMERA_OK) continue;
        qymera_device_t dev;
        if (qymera_registry_get_device(reg, dev_idx, &dev) != QYMERA_OK) continue;
        if (product[0]) snprintf(dev.name, sizeof(dev.name), "%s", product);
        if (version[0]) snprintf(dev.fw_version, sizeof(dev.fw_version), "%s", version);
        if (platform[0]) snprintf(dev.model, sizeof(dev.model), "%s", platform);
        qymera_registry_update_device(reg, dev_idx, &dev);
    }
    return QYMERA_OK;
}

static qymera_err_t node_fetch_calib(qymera_node_client_t *client, uint16_t target_idx) {
    const qymera_node_target_t *t = &client->targets.targets[target_idx];
    qymera_node_target_state_t *st = &client->state[target_idx];
    char resp[QYMERA_NODE_HTTP_BUF];
    int status = 0;
    qymera_err_t err = http_request(client, t->host, t->port, "GET", "/calib",
                                    NULL, NULL, resp, sizeof(resp), &status);
    if (err != QYMERA_OK) return err;
    if (status != 200) return QYMERA_ERR_PROTOCOL;
    if (resp[0] != '[') return QYMERA_ERR_PROTOCOL;

    qymera_registry_t *reg = client->cfg.registry;
    uint16_t seen[QYMERA_MAX_ENTITIES];
    size_t seen_count = 0;

    char new_dev_ids[QYMERA_MAX_NODE_DEVICES][QYMERA_DEVICE_ID_LEN];
    uint8_t new_dev_count = 0;

    size_t idx = 0;
    const char *start = NULL, *end = NULL;
    while (json_next_object(resp, &idx, &start, &end)) {
        char entity_id[QYMERA_ENTITY_ID_LEN] = {0};
        char device_id[QYMERA_DEVICE_ID_LEN] = {0};
        char name[QYMERA_ENTITY_ID_LEN] = {0};
        char ip[16] = {0};
        uint32_t entity_uid = 0, device_uid = 0, calib_type = 0, pulse_ms = 0, age_ms = 0;
        float value = 0.0f, correction = 0.0f;
        bool avail = true, state = false;

        if (!json_parse_uint(json_find_key(start, "id"), &entity_uid)) continue;
        json_parse_uint(json_find_key(start, "device_uid"), &device_uid);
        json_parse_uint(json_find_key(start, "type"), &calib_type);
        json_parse_uint(json_find_key(start, "pulse_ms"), &pulse_ms);
        json_parse_uint(json_find_key(start, "age_ms"), &age_ms);
        json_parse_str(json_find_key(start, "name"), name, sizeof(name));
        json_parse_str(json_find_key(start, "ip"), ip, sizeof(ip));
        json_parse_num(json_find_key(start, "value"), &value);
        json_parse_num(json_find_key(start, "correction"), &correction);
        json_parse_bool(json_find_key(start, "avail"), &avail);
        json_parse_bool(json_find_key(start, "state"), &state);

        qymera_entity_type_t etype = calib_type_to_entity(calib_type);
        if (etype == QYMERA_ENTITY_NONE) continue;

        snprintf(entity_id, sizeof(entity_id), "%u", (unsigned)entity_uid);
        if (device_uid == 0) device_uid = entity_uid; /* defensive fallback */
        snprintf(device_id, sizeof(device_id), "%u", (unsigned)device_uid);
        if (!name[0]) snprintf(name, sizeof(name), "entity-%u", (unsigned)entity_uid);

        if (new_dev_count < QYMERA_MAX_NODE_DEVICES) {
            bool seen_dev = false;
            for (uint8_t i = 0; i < new_dev_count; i++) {
                if (strcmp(new_dev_ids[i], device_id) == 0) { seen_dev = true; break; }
            }
            if (!seen_dev) snprintf(new_dev_ids[new_dev_count++], QYMERA_DEVICE_ID_LEN, "%s", device_id);
        }

        qymera_capability_t caps[4] = { QYMERA_CAP_NONE };
        uint8_t ncap = 0;
        calib_type_caps(calib_type, caps, &ncap);

        qymera_device_t dev;
        memset(&dev, 0, sizeof(dev));
        snprintf(dev.device_id, sizeof(dev.device_id), "%s", device_id);
        snprintf(dev.name, sizeof(dev.name), "%s", device_id);
        snprintf(dev.model, sizeof(dev.model), "%s", "qymera-node");
        snprintf(dev.api_version, sizeof(dev.api_version), "%s", "1.0");
        snprintf(dev.protocol_version, sizeof(dev.protocol_version), "%s", "1.0");
        snprintf(dev.ip_addr, sizeof(dev.ip_addr), "%s", ip[0] ? ip : t->host);
        dev.port = t->port;
        dev.chip_uid = device_uid;
        dev.role = 1;
        dev.online = true;
        dev.state = 0;
        uint16_t dev_idx;
        if (qymera_registry_find_device(reg, device_id, &dev_idx) == QYMERA_OK) {
            qymera_registry_update_device(reg, dev_idx, &dev);
            qymera_registry_set_online(reg, dev_idx, true);
        } else {
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
            snprintf(ent.name, sizeof(ent.name), "%s", name);
            ent.type = etype;
            for (uint8_t c = 0; c < ncap && c < QYMERA_ARRAY_SIZE(ent.capabilities); c++) {
                ent.capabilities[c] = caps[c];
            }
            ent.capability_count = ncap;
            if (etype == QYMERA_ENTITY_ACTUATOR_DIMMER) {
                ent.native_min = 0.0f;
                ent.native_max = 100.0f;
                ent.unit[0] = '%';
                ent.unit[1] = '\0';
            } else if (etype == QYMERA_ENTITY_ACTUATOR_RELAY) {
                ent.native_min = 0.0f;
                ent.native_max = 1.0f;
            }
            ent.correction = correction;
            ent.pulse_ms = pulse_ms;
            if (qymera_registry_register_entity(reg, dev_idx, &ent, &ent_idx) != QYMERA_OK) continue;
        }

        qymera_entity_value_t v = {0};
        v.valid = avail;
        v.timestamp = qymera_timestamp_now();
        if (calib_type == 9) { /* relay: authoritative ON/OFF is `state` */
            v.bool_value = state;
            v.numeric_value = state ? 1.0f : 0.0f;
        } else if (calib_type == 8) { /* dimmer: level is `value`, ON = >0 */
            v.numeric_value = value;
            v.bool_value = value > 0.0f;
        } else { /* sensors: raw reading + state bit when meaningful */
            v.numeric_value = value;
            v.bool_value = state;
        }
        v.reliability = avail ? QYMERA_RELIABILITY_CONFIRMED : QYMERA_RELIABILITY_STALE;
        qymera_registry_update_entity_value(reg, ent_idx, &v);

        if (seen_count < QYMERA_MAX_ENTITIES) seen[seen_count++] = ent_idx;

        if (client->cfg.on_entity_state) {
            client->cfg.on_entity_state(client->cfg.callback_ctx, device_id, entity_id,
                                        avail, (uint8_t)etype,
                                        v.numeric_value, v.bool_value);
        }
    }

    for (uint8_t i = 0; i < new_dev_count; i++) {
        node_mark_entities(client, new_dev_ids[i], seen, seen_count, QYMERA_RELIABILITY_STALE);
    }

    /* Refresh tracked devices (keep the previous set on an empty snapshot so
     * a later poll failure can still offline the last-known devices). */
    if (new_dev_count > 0) {
        st->node_device_count = 0;
        for (uint8_t i = 0; i < new_dev_count; i++) {
            snprintf(st->node_device_ids[st->node_device_count], QYMERA_DEVICE_ID_LEN, "%s", new_dev_ids[i]);
            st->node_device_count++;
        }
    }
    return QYMERA_OK;
}

static void node_mark_offline(qymera_node_client_t *client, uint16_t target_idx) {
    qymera_registry_t *reg = client->cfg.registry;
    for (uint8_t i = 0; i < client->state[target_idx].node_device_count; i++) {
        const char *device_id = client->state[target_idx].node_device_ids[i];
        if (!device_id || !device_id[0]) continue;
        uint16_t dev_idx;
        if (qymera_registry_find_device(reg, device_id, &dev_idx) != QYMERA_OK) continue;
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

        qymera_err_t err = node_fetch_calib(client, i);
        if (err == QYMERA_OK) {
            err = node_fetch_firmware(client, i);
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

/* Map a non-200 command status to a qymera_err_t + stable error code string. */
static qymera_err_t command_status_error(qymera_node_client_t *client, int status,
                                         char *err_code, size_t err_sz) {
    const char *code;
    qymera_err_t err;
    switch (status) {
        case 400:  code = "INVALID_INPUT";    err = QYMERA_ERR_INVALID_ARG;     break;
        case 401:  code = "NOT_AUTHORIZED";   err = QYMERA_ERR_INVALID_STATE;   break;
        case 404:  code = "ENTITY_NOT_FOUND"; err = QYMERA_ERR_NOT_FOUND;       break;
        case 405:  code = "METHOD_NOT_ALLOWED"; err = QYMERA_ERR_PROTOCOL;      break;
        case 408:  code = "TIMEOUT";          err = QYMERA_ERR_TIMEOUT;         break;
        case 429:  code = "RATE_LIMITED";     err = QYMERA_ERR_BUSY;            break;
        default:
            if (status >= 500) { code = "DEVICE_OFFLINE"; err = QYMERA_ERR_NETWORK; }
            else               { code = "PROTOCOL";       err = QYMERA_ERR_PROTOCOL; }
            break;
    }
    if (err_code && err_sz) snprintf(err_code, err_sz, "%s", code);
    client->commands_err++;
    return err;
}

qymera_err_t qymera_node_client_send_command(qymera_node_client_t *client,
                                             const char *host, uint16_t port,
                                             const char *device_id,
                                             const char *entity_id,
                                             uint8_t opcode, float value_f,
                                             bool *accepted,
                                             char *err_code, size_t err_sz) {
    (void)device_id;
    if (accepted) *accepted = false;
    if (!client || !host || !entity_id) return QYMERA_ERR_INVALID_ARG;

    /* entity_id is the numeric uid reported by /calib (`id`), so it must be a
     * sequence of decimal digits. */
    for (const char *p = entity_id; *p; p++) {
        if (*p < '0' || *p > '9') return QYMERA_ERR_INVALID_ARG;
    }

    char path[32];
    char body[48];
    if (opcode == 1) {
        snprintf(path, sizeof(path), "/toggle");
        snprintf(body, sizeof(body), "id=%s", entity_id);
    } else if (opcode == 2) {
        int level = (int)value_f;
        if (level < 0) level = 0;
        if (level > 100) level = 100;
        snprintf(path, sizeof(path), "/dimmer");
        snprintf(body, sizeof(body), "id=%s&value=%d", entity_id, (int)level);
    } else {
        return QYMERA_ERR_INVALID_CAPABILITY;
    }

    char resp[QYMERA_NODE_HTTP_BUF];
    int status = 0;
    qymera_err_t err = http_request(client, host, port, "POST", path, body,
                                    "application/x-www-form-urlencoded",
                                    resp, sizeof(resp), &status);
    if (err != QYMERA_OK) {
        client->commands_err++;
        if (err_code && err_sz) {
            snprintf(err_code, err_sz, "%s", (err == QYMERA_ERR_TIMEOUT) ? "TIMEOUT" : "DEVICE_OFFLINE");
        }
        return err;
    }

    client->commands_sent++;
    if (status == 200) {
        if (accepted) *accepted = true;
        return QYMERA_OK;
    }
    return command_status_error(client, status, err_code, err_sz);
}

void qymera_node_client_stats(const qymera_node_client_t *client,
                              uint32_t *polls_ok, uint32_t *polls_fail,
                              uint32_t *commands_sent, uint32_t *commands_err) {
    if (polls_ok) *polls_ok = client ? client->polls_ok : 0;
    if (polls_fail) *polls_fail = client ? client->polls_fail : 0;
    if (commands_sent) *commands_sent = client ? client->commands_sent : 0;
    if (commands_err) *commands_err = client ? client->commands_err : 0;
}
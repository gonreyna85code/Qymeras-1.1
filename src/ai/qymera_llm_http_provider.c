/**
 * Qymera Dashboard - LLM HTTP Provider transport implementation.
 *
 * Concrete, bounded OpenAI-compatible chat provider (also speaks Ollama's
 * /api/chat tool format). Builds a compact JSON request body carrying the
 * registry-derived tool catalog, performs a single HTTP POST, and classifies
 * the bounded response into exactly one qymera_llm_message_t. Everything here
 * is deterministic and bounded; no JSON library, no TLS, no streaming.
 *
 * The pure request-builder / response-parser logic is mirrored by host tests in
 * tests/host_sanity.py (Phase 3F) so the classification contract is pinned
 * without needing a live model or a network on the host.
 */
#include "qymera_llm_http_provider.h"
#include "qymera_log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include "lwip/netdb.h"

#define LOG_AI(l, ...)   qymera_log_ai((l), "llhttp", __VA_ARGS__)
#define LOG_ERR(l, ...)  qymera_log_error((l), "llhttp", __VA_ARGS__)

/* =========================
 * Tolerances
 * ========================= */
#define QYMERA_LLM_HTTP_CONNECT_TRIES   3
#define QYMERA_LLM_HTTP_PATH_DEFAULT    "/v1/chat/completions"

static size_t hen_len(struct hostent *he) {
    return he->h_length ? (size_t)he->h_length : 4;
}

/* =========================
 * Miniature JSON writer (bounded, no allocation)
 * ========================= */
typedef struct {
    char *buf;
    size_t cap;
    size_t len;
    bool truncated;
} jw_t;

static void jw_add_raw(jw_t *w, const char *s, size_t n) {
    if (!w || w->truncated) return;
    if (w->len + n > w->cap) { w->truncated = true; return; }
    if (n) memcpy(w->buf + w->len, s, n);
    w->len += n;
    w->buf[ w->len < w->cap ? w->len : w->cap - 1 ] = '\0';
}

static void jw_puts(jw_t *w, const char *s) {
    jw_add_raw(w, s, s ? strlen(s) : 0);
}

/* JSON-escaped string literal (with quotes). */
static void jw_string(jw_t *w, const char *s) {
    if (!s) s = "";
    jw_add_raw(w, "\"", 1);
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        unsigned char c = *p++;
        switch (c) {
            case '"':  jw_puts(w, "\\\""); break;
            case '\\': jw_puts(w, "\\\\"); break;
            case '\b': jw_puts(w, "\\b");  break;
            case '\f': jw_puts(w, "\\f");  break;
            case '\n': jw_puts(w, "\\n");  break;
            case '\r': jw_puts(w, "\\r");  break;
            case '\t': jw_puts(w, "\\t");  break;
            default:
                if (c < 0x20) {
                    char esc[7];
                    snprintf(esc, sizeof(esc), "\\u%04x", c);
                    jw_puts(w, esc);
                } else {
                    jw_add_raw(w, (const char *)&c, 1);
                }
                break;
        }
    }
    jw_add_raw(w, "\"", 1);
}

/* =========================
 * Tool catalog schema (DERIVED from the Skill registry).
 *
 * Each skill's function schema exposes exactly the flat fields the adapter's
 * structured carrier understands (qymera_llm_tool_arguments_t). This is not a
 * second tool list: names come from the registry; only the JSON-schema field
 * hints are per-skill here.
 * ========================= */
typedef enum {
    F_DEVICE = 1 << 0,
    F_ENTITY = 1 << 1,
    F_NAME   = 1 << 2,
    F_RULEID = 1 << 3,
    F_VALUE  = 1 << 4,
    F_LEVEL  = 1 << 5,
    F_ENABLED= 1 << 6,
    F_RULE   = 1 << 7,
} tool_field_t;

static unsigned tool_fields(const char *name) {
    if (strcmp(name, "get_entity_state") == 0 || strcmp(name, "get_entity_info") == 0)
        return F_DEVICE | F_ENTITY;
    if (strcmp(name, "set_relay") == 0)  return F_DEVICE | F_ENTITY | F_VALUE;
    if (strcmp(name, "set_dimmer") == 0) return F_DEVICE | F_ENTITY | F_LEVEL;
    if (strcmp(name, "get_rule") == 0 || strcmp(name, "delete_rule") == 0 ||
        strcmp(name, "enable_rule") == 0 || strcmp(name, "disable_rule") == 0)
        return F_RULEID;
    if (strcmp(name, "update_rule") == 0) return F_RULEID | F_NAME | F_RULE;
    if (strcmp(name, "create_rule") == 0) return F_NAME | F_RULE;
    return 0; /* list_* skills take no arguments */
}

static void jw_field_schema(jw_t *w, unsigned f) {
    const struct { unsigned bit; const char *key; const char *schema; } fields[] = {
        { F_DEVICE,  "device_id", "{\"type\":\"string\"}" },
        { F_ENTITY,  "entity_id", "{\"type\":\"string\"}" },
        { F_NAME,    "name",      "{\"type\":\"string\"}" },
        { F_RULEID,  "rule_id",   "{\"type\":\"string\"}" },
        { F_VALUE,   "value",     "{\"type\":\"boolean\"}" },
        { F_LEVEL,   "level",     "{\"type\":\"integer\",\"minimum\":0,\"maximum\":100}" },
        { F_ENABLED, "enabled",   "{\"type\":\"boolean\"}" },
        { F_RULE,    "rule",      "{\"type\":\"object\"}" },
    };
    bool first = true;
    jw_puts(w, "\"properties\":{");
    for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
        if (!(f & fields[i].bit)) continue;
        if (!first) jw_add_raw(w, ",", 1);
        first = false;
        jw_string(w, fields[i].key);
        jw_add_raw(w, ":", 1);
        jw_puts(w, fields[i].schema);
    }
    jw_puts(w, "}");
}

/* =========================
 * Request body builder
 * ========================= */
static qymera_err_t build_request_body(qymera_llm_http_ctx_t *ctx,
                                       const qymera_llm_request_t *request) {
    jw_t w;
    w.buf = ctx->req_buf;
    w.cap = sizeof(ctx->req_buf);
    w.len = 0;
    w.truncated = false;

    const char *model = request && request->model[0] ? request->model
                      : ctx->config.model[0]        ? ctx->config.model
                      : "qymera-smart-home";

    jw_puts(&w, "{\"model\":");
    jw_string(&w, model);
    jw_puts(&w, ",\"temperature\":0,\"stream\":false,\"messages\":[");
    jw_puts(&w, "{\"role\":\"system\",\"content\":");
    jw_string(&w, QYMERA_LLM_HTTP_SYSTEM_PROMPT);
    jw_puts(&w, "},{\"role\":\"user\",\"content\":");
    jw_string(&w, request ? request->prompt : "");
    jw_puts(&w, "}],\"tools\":[");

    size_t count = qymera_llm_adapter_tool_count();
    bool first = true;
    for (size_t i = 0; i < count && !w.truncated; i++) {
        const qymera_skill_meta_t *meta = NULL;
        qymera_llm_adapter_tool_at(i, &meta);
        if (!meta || !meta->name[0]) continue;
        if (!first) jw_add_raw(&w, ",", 1);
        first = false;
        jw_puts(&w, "{\"type\":\"function\",\"function\":{\"name\":");
        jw_string(&w, meta->name);
        jw_puts(&w, ",\"description\":");
        jw_string(&w, meta->description);
        jw_puts(&w, ",\"parameters\":{\"type\":\"object\",");
        jw_field_schema(&w, tool_fields(meta->name));
        jw_puts(&w, "}}}");
    }
    jw_puts(&w, "]}");

    return w.truncated ? QYMERA_ERR_NO_SPACE : QYMERA_OK;
}

/* =========================
 * Endpoint URL parsing (bounded). Format:
 *   http://host[:port][/path]
 * Accepts IP literals and hostnames. Defaults: port 80, path
 * /v1/chat/completions.
 * ========================= */
typedef struct {
    char host[96];
    uint16_t port;
    char path[96];
} llhttp_target_t;

static bool parse_endpoint(const char *endpoint, llhttp_target_t *t) {
    memset(t, 0, sizeof(*t));
    if (!endpoint || strncmp(endpoint, "http://", 7) != 0) return false;
    const char *p = endpoint + 7;

    /* host: up to ':' (with port) or '/' (path) or end. */
    const char *host_start = p;
    const char *colon = strchr(p, ':');
    const char *slash = strchr(p, '/');
    const char *host_end = NULL;
    if (colon && (!slash || colon < slash)) host_end = colon;
    else if (slash) host_end = slash;
    else host_end = host_start + strlen(host_start);
    size_t hlen = (size_t)(host_end - host_start);
    if (hlen == 0 || hlen >= sizeof(t->host)) return false;
    memcpy(t->host, host_start, hlen);
    t->host[hlen] = '\0';
    t->port = 80;

    if (colon && (!slash || colon < slash)) {
        p = colon + 1;
        char port_str[8];
        size_t i = 0;
        while (*p >= '0' && *p <= '9' && i < sizeof(port_str) - 1) port_str[i++] = *p++;
        port_str[i] = '\0';
        if (i == 0) return false;
        long port = strtol(port_str, NULL, 10);
        if (port <= 0 || port > 65535) return false;
        t->port = (uint16_t)port;
    }
    if (slash) {
        const char *pe = strchr(slash, '\0');
        size_t plen = (size_t)(pe - slash);
        if (plen >= sizeof(t->path)) return false;
        memcpy(t->path, slash, plen);
        t->path[plen] = '\0';
    } else {
        strncpy(t->path, QYMERA_LLM_HTTP_PATH_DEFAULT, sizeof(t->path) - 1);
    }
    return true;
}

/* =========================
 * Outbound HTTP POST (plain TCP, bounded, with timeout).
 * Returns 0 on success (HTTP 2xx captured in *status), or a negative
 * QYMERA_ERR_* code. The full response (headers + body) is left in
 * ctx->resp_buf (NUL-terminated, bounded).
 * ========================= */
static qymera_err_t http_post(qymera_llm_http_ctx_t *ctx,
                              const llhttp_target_t *target,
                              uint32_t timeout_ms,
                              long *status_out) {
    *status_out = 0;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(target->port);

    /* Accept IP literals directly (lwIP gethostbyname does not resolve them). */
    if (inet_aton(target->host, &addr.sin_addr) == 0) {
        struct hostent *he = gethostbyname(target->host);
        if (!he || !he->h_addr_list[0]) return QYMERA_ERR_NETWORK;
        memcpy(&addr.sin_addr.s_addr, he->h_addr_list[0], hen_len(he));
    }

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int fd = -1;
    bool connected = false;
    for (int attempt = 0; attempt < QYMERA_LLM_HTTP_CONNECT_TRIES && !connected; attempt++) {
        fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) return QYMERA_ERR_NETWORK;
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0) connected = true;
        else { close(fd); fd = -1; }
    }
    if (!connected) return QYMERA_ERR_TIMEOUT;

    size_t body_len = strlen(ctx->req_buf);

    /* Request line + headers (bounded). */
    char header[256];
    int hlen = snprintf(header, sizeof(header),
        "POST %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %u\r\n"
        "Connection: close\r\n",
        target->path, target->host, (unsigned)body_len);
    if (hlen < 0 || (size_t)hlen >= sizeof(header)) { close(fd); return QYMERA_ERR_PROTOCOL; }
    header[hlen] = '\0';

    if ((size_t)hlen + body_len + 2 > sizeof(ctx->resp_buf)) { close(fd); return QYMERA_ERR_NO_SPACE; }

    /* Spoon the request in bounded chunks. */
    char hdr_full[256 + 2];
    memcpy(hdr_full, header, (size_t)hlen);
    hdr_full[hlen] = '\r';
    hdr_full[hlen + 1] = '\n';
    hdr_full[hlen + 2] = '\0';

    ssize_t sent = send(fd, hdr_full, (size_t)hlen + 2, 0);
    if (sent < 0) { close(fd); return QYMERA_ERR_NETWORK; }
    size_t pos = 0;
    while (pos < body_len) {
        ssize_t n = send(fd, ctx->req_buf + pos, body_len - pos, 0);
        if (n <= 0) { close(fd); return QYMERA_ERR_NETWORK; }
        pos += (size_t)n;
    }

    /* Read the whole bounded response. */
    size_t got = 0;
    for (;;) {
        ssize_t n = recv(fd, ctx->resp_buf + got, sizeof(ctx->resp_buf) - 1 - got, 0);
        if (n > 0) {
            got += (size_t)n;
            if (got >= sizeof(ctx->resp_buf) - 1) break; /* bounded */
            continue;
        }
        if (n == 0) break; /* clean close */
        if (errno == EWOULDBLOCK || errno == EAGAIN) { close(fd); return QYMERA_ERR_TIMEOUT; }
        close(fd); return QYMERA_ERR_NETWORK;
    }
    close(fd);
    ctx->resp_buf[got] = '\0';

    /* Parse status line: "HTTP/1.1 200 OK". */
    char *sp = strchr(ctx->resp_buf, ' ');
    if (sp) {
        long code = strtol(sp + 1, NULL, 10);
        if (code >= 200 && code < 300) { *status_out = code; return QYMERA_OK; }
        *status_out = code;
        return QYMERA_ERR_PROTOCOL; /* non-2xx: error details in body */
    }
    return QYMERA_ERR_PROTOCOL;
}

/* =========================
 * Minimal bounded JSON scanner (response parsing only).
 * ========================= */
static const char *j_skip_ws(const char *p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}

/* Parse a JSON string at *p into out (bounded, with escapes decoded).
 * Long strings are truncated to cap-1, never treated as malformed.
 * Returns pointer past the closing quote, or NULL on malformed input. */
static const char *j_string(const char *p, char *out, size_t cap) {
    if (!p || *p != '"') return NULL;
    p++;
    size_t i = 0;
    while (*p && *p != '"') {
        if (*p == '\\') {
            p++;
            if (!*p) return NULL;
            if (i < cap - 1) {
                switch (*p) {
                    case '"':  out[i++] = '"'; break;
                    case '\\': out[i++] = '\\'; break;
                    case '/':  out[i++] = '/'; break;
                    case 'b':  out[i++] = '\b'; break;
                    case 'f':  out[i++] = '\f'; break;
                    case 'n':  out[i++] = '\n'; break;
                    case 'r':  out[i++] = '\r'; break;
                    case 't':  out[i++] = '\t'; break;
                    case 'u': {
                        if (!p[1] || !p[2] || !p[3] || !p[4]) return NULL;
                        for (int k = 1; k <= 4; k++) {
                            char c = p[k];
                            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                                  (c >= 'A' && c <= 'F')))
                                return NULL;
                        }
                        out[i++] = '?';
                        p += 4;
                        break;
                    }
                    default: return NULL;
                }
            }
            p++;
        } else {
            if (i < cap - 1) out[i++] = *p;
            p++;
        }
    }
    if (!*p) return NULL;      /* unterminated */
    out[i] = '\0';
    return p + 1;
}

/* Structurally validate-and-skip a JSON string without decoding it. */
static const char *j_skip_string(const char *p) {
    if (!p || *p != '"') return NULL;
    p++;
    while (*p && *p != '"') {
        if (*p == '\\') {
            p++;
            if (!*p) return NULL;
            p += (*p == 'u') ? 5 : 1;  /* \u plus 4 hex digits; else 1 char */
        } else {
            p++;
        }
    }
    return *p == '"' ? p + 1 : NULL;
}

/* Skip one JSON value (validating structure). Returns past-value pointer. */
static const char *j_skip(const char *p);
static const char *j_skip_obj(const char *p) {
    p = j_skip_ws(p + 1);
    if (*p == '}') return p + 1;
    for (;;) {
        p = j_skip_ws(p);
        if (*p != '"') return NULL;
        char k[64];
        p = j_string(p, k, sizeof(k));
        if (!p) return NULL;
        p = j_skip_ws(p);
        if (*p != ':') return NULL;
        p = j_skip(j_skip_ws(p + 1));
        if (!p) return NULL;
        p = j_skip_ws(p);
        if (*p == '}') return p + 1;
        if (*p != ',') return NULL;
        p++;
    }
}
static const char *j_skip_arr(const char *p) {
    p = j_skip_ws(p + 1);
    if (*p == ']') return p + 1;
    for (;;) {
        p = j_skip(j_skip_ws(p));
        if (!p) return NULL;
        p = j_skip_ws(p);
        if (*p == ']') return p + 1;
        if (*p != ',') return NULL;
        p++;
    }
}
static const char *j_skip(const char *p) {
    p = j_skip_ws(p);
    switch (*p) {
        case '{': return j_skip_obj(p);
        case '[': return j_skip_arr(p);
        case '"': return j_skip_string(p);
        case 't': return strncmp(p, "true", 4) == 0 ? p + 4 : NULL;
        case 'f': return strncmp(p, "false", 5) == 0 ? p + 5 : NULL;
        case 'n': return strncmp(p, "null", 4) == 0 ? p + 4 : NULL;
        default: {
            /* number */
            const char *q = p;
            if (*q == '-') q++;
            if (*q < '0' || *q > '9') return NULL;
            while (*q >= '0' && *q <= '9') q++;
            if (*q == '.') { q++; while (*q >= '0' && *q <= '9') q++; }
            if (*q == 'e' || *q == 'E') {
                q++;
                if (*q == '+' || *q == '-') q++;
                if (*q < '0' || *q > '9') return NULL;
                while (*q >= '0' && *q <= '9') q++;
            }
            return q;
        }
    }
}

/* Locate the value of key inside an object starting at `obj` ('{'). Returns a
 * pointer to the value start, or NULL. Deep objects are skipped, never parsed. */
static const char *j_obj_find(const char *obj, const char *key) {
    obj = j_skip_ws(obj);
    if (*obj != '{') return NULL;
    const char *p = j_skip_ws(obj + 1);
    if (*p == '}') return NULL;
    size_t klen = strlen(key);
    for (;;) {
        p = j_skip_ws(p);
        if (*p != '"') return NULL;
        if (strncmp(p + 1, key, klen) == 0 && p[1 + klen] == '"') {
            char k[64];
            const char *after = j_string(p, k, sizeof(k));
            if (!after) return NULL;
            after = j_skip_ws(after);
            if (*after != ':') return NULL;
            return j_skip_ws(after + 1);
        }
        char k[64];
        p = j_string(p, k, sizeof(k));
        if (!p) return NULL;
        p = j_skip_ws(p);
        if (*p != ':') return NULL;
        p = j_skip(j_skip_ws(p + 1));
        if (!p) return NULL;
        p = j_skip_ws(p);
        if (*p == '}') return NULL;
        if (*p != ',') return NULL;
        p++;
    }
}

/* Locate the idx-th array element (value must be an array). */
static const char *j_arr_nth(const char *arr, size_t idx) {
    arr = j_skip_ws(arr);
    if (*arr != '[') return NULL;
    const char *p = j_skip_ws(arr + 1);
    if (*p == ']') return NULL;
    size_t i = 0;
    for (;;) {
        const char *v = j_skip_ws(p);
        const char *e = j_skip(v);
        if (!e) return NULL;
        if (i == idx) return v;
        p = j_skip_ws(e);
        if (*p == ']') return NULL;
        if (*p != ',') return NULL;
        p++;
        i++;
    }
}

/* =========================
 * Response classification (OpenAI chat-completions AND Ollama /api/chat).
 *   - error object           -> PROVIDER_ERROR
 *   - message.tool_calls     -> TOOL_CALL (name + arguments)
 *   - message.content        -> TEXT
 *   - otherwise              -> MALFORMED
 * ========================= */
static void set_msg_kind(qymera_llm_message_t *m, qymera_llm_message_kind_t kind,
                         const char *text, const char *tool_name) {
    memset(m, 0, sizeof(*m));
    m->kind = kind;
    if (text) snprintf(m->text, sizeof(m->text), "%s", text);
    if (tool_name) snprintf(m->tool_name, sizeof(m->tool_name), "%s", tool_name);
}

static const char *r_find_message(const char *body) {
    /* OpenAI: choices[0].message ; Ollama: message at top level. */
    const char *c = j_obj_find(body, "choices");
    if (c) {
        const char *first = j_arr_nth(c, 0);
        if (first) {
            const char *msg = j_obj_find(first, "message");
            if (msg) return msg;
        }
    }
    const char *msg = j_obj_find(body, "message");
    if (msg) return msg;
    return NULL;
}

static qymera_err_t parse_response(const char *body, size_t len,
                                   qymera_llm_message_t *message) {
    if (!body || !message || len == 0) return QYMERA_ERR_INVALID_ARG;
    if (j_skip(body) != body + len) {
        set_msg_kind(message, QYMERA_LLM_MSG_MALFORMED, "malformed provider JSON", NULL);
        return QYMERA_OK;
    }

    /* Provider error object (OpenAI style: {"error":{...}}). */
    const char *errv = j_obj_find(body, "error");
    if (errv && *errv != 'n') { /* "error" present and not null */
        const char *em = j_obj_find(errv, "message");
        char err_text[QYMERA_LLM_TEXT_LEN];
        if (em && *em == '"') {
            if (j_string(em, err_text, sizeof(err_text)))
                set_msg_kind(message, QYMERA_LLM_MSG_PROVIDER_ERROR, err_text, NULL);
            else set_msg_kind(message, QYMERA_LLM_MSG_PROVIDER_ERROR, "provider returned an error", NULL);
        } else {
            set_msg_kind(message, QYMERA_LLM_MSG_PROVIDER_ERROR, "provider returned an error", NULL);
        }
        return QYMERA_OK;
    }

    const char *msgv = r_find_message(body);
    if (!msgv) {
        set_msg_kind(message, QYMERA_LLM_MSG_MALFORMED, "response has no message object", NULL);
        return QYMERA_OK;
    }

    /* Tool calls first (OpenAI + Ollama). */
    const char *tcv = j_obj_find(msgv, "tool_calls");
    if (tcv) {
        const char *tc = j_arr_nth(tcv, 0);
        if (tc) {
            const char *fn = j_obj_find(tc, "function");
            if (fn) {
                char tname[QYMERA_SKILL_NAME_LEN];
                const char *nv = j_obj_find(fn, "name");
                if (nv && *nv == '"' && j_string(nv, tname, sizeof(tname)) && tname[0]) {
                    const char *av = j_obj_find(fn, "arguments");
                    char arg_span[160];
                    if (av && *av == '"') {
                        if (!j_string(av, arg_span, sizeof(arg_span))) {
                            set_msg_kind(message, QYMERA_LLM_MSG_MALFORMED, "tool arguments malformed", NULL);
                            return QYMERA_OK;
                        }
                        set_msg_kind(message, QYMERA_LLM_MSG_TOOL_CALL, NULL, tname);
                        qymera_err_t ar = qymera_llm_args_from_json(arg_span, strlen(arg_span), &message->args);
                        if (ar != QYMERA_OK) {
                            set_msg_kind(message, QYMERA_LLM_MSG_MALFORMED, "tool arguments invalid", NULL);
                            return QYMERA_OK;
                        }
                    } else if (av && *av == '{') {
                        const char *end = j_skip(av);
                        if (!end || (size_t)(end - av) >= sizeof(arg_span)) {
                            set_msg_kind(message, QYMERA_LLM_MSG_MALFORMED, "tool arguments too large", NULL);
                            return QYMERA_OK;
                        }
                        memcpy(arg_span, av, (size_t)(end - av));
                        arg_span[(size_t)(end - av)] = '\0';
                        set_msg_kind(message, QYMERA_LLM_MSG_TOOL_CALL, NULL, tname);
                        qymera_err_t ar = qymera_llm_args_from_json(arg_span, (size_t)(end - av), &message->args);
                        if (ar != QYMERA_OK) {
                            set_msg_kind(message, QYMERA_LLM_MSG_MALFORMED, "tool arguments invalid", NULL);
                            return QYMERA_OK;
                        }
                    } else {
                        set_msg_kind(message, QYMERA_LLM_MSG_MALFORMED, "tool arguments missing", NULL);
                        return QYMERA_OK;
                    }
                    return QYMERA_OK;
                }
            }
        }
        set_msg_kind(message, QYMERA_LLM_MSG_MALFORMED, "tool call is not structurally valid", NULL);
        return QYMERA_OK;
    }

    /* Assistant text. */
    const char *cv = j_obj_find(msgv, "content");
    if (cv && *cv == '"') {
        char text[QYMERA_LLM_TEXT_LEN];
        if (j_string(cv, text, sizeof(text))) {
            set_msg_kind(message, QYMERA_LLM_MSG_TEXT, text, NULL);
            return QYMERA_OK;
        }
    }

    set_msg_kind(message, QYMERA_LLM_MSG_MALFORMED, "response has no usable content", NULL);
    return QYMERA_OK;
}

/* =========================
 * Public entry points
 * ========================= */
static qymera_err_t provider_complete(void *provider_ctx,
                                      const qymera_llm_request_t *request,
                                      qymera_llm_message_t *message);

void qymera_llm_http_provider_init(qymera_llm_provider_t *provider,
                                   qymera_llm_http_ctx_t *ctx) {
    if (!provider) return;
    provider->provider_ctx = ctx;
    provider->complete = provider_complete;
    if (ctx) {
        memset(ctx->req_buf, 0, sizeof(ctx->req_buf));
        memset(ctx->resp_buf, 0, sizeof(ctx->resp_buf));
    }
}

/* Internal adapter-facing callback. */
static qymera_err_t provider_complete(void *provider_ctx,
                                      const qymera_llm_request_t *request,
                                      qymera_llm_message_t *message) {
    return qymera_llm_http_complete((qymera_llm_http_ctx_t *)provider_ctx,
                                    request, message);
}

qymera_err_t qymera_llm_http_complete(qymera_llm_http_ctx_t *ctx,
                                      const qymera_llm_request_t *request,
                                      qymera_llm_message_t *message) {
    if (!ctx || !request || !message) return QYMERA_ERR_INVALID_ARG;
    if (!ctx->config.endpoint[0]) {
        set_msg_kind(message, QYMERA_LLM_MSG_PROVIDER_ERROR,
                     "provider endpoint not configured", NULL);
        return QYMERA_OK;
    }

    qymera_err_t err = build_request_body(ctx, request);
    if (err != QYMERA_OK) {
        set_msg_kind(message, QYMERA_LLM_MSG_MALFORMED, "request body build failed", NULL);
        return QYMERA_OK;
    }

    llhttp_target_t target;
    if (!parse_endpoint(ctx->config.endpoint, &target)) {
        set_msg_kind(message, QYMERA_LLM_MSG_PROVIDER_ERROR, "provider endpoint malformed", NULL);
        return QYMERA_OK;
    }

    uint32_t timeout = ctx->config.timeout_ms ? ctx->config.timeout_ms
                                              : QYMERA_LLM_HTTP_DEFAULT_TIMEOUT_MS;
    long status = 0;
    err = http_post(ctx, &target, timeout, &status);
    if (err == QYMERA_ERR_TIMEOUT) {
        set_msg_kind(message, QYMERA_LLM_MSG_TIMEOUT, "provider request timed out", NULL);
        return QYMERA_OK;
    }
    if (err == QYMERA_ERR_NETWORK) {
        set_msg_kind(message, QYMERA_LLM_MSG_PROVIDER_ERROR, "provider unreachable", NULL);
        return QYMERA_OK;
    }
    if (err == QYMERA_ERR_PROTOCOL && status != 0) {
        char txt[40];
        snprintf(txt, sizeof(txt), "provider HTTP status %ld", status);
        set_msg_kind(message, QYMERA_LLM_MSG_PROVIDER_ERROR, txt, NULL);
        return QYMERA_OK;
    }
    if (err != QYMERA_OK) {
        set_msg_kind(message, QYMERA_LLM_MSG_PROVIDER_ERROR, "provider transport failure", NULL);
        return QYMERA_OK;
    }

    /* Locate the response body after the "\r\n\r\n" separator. */
    const char *sep = strstr(ctx->resp_buf, "\r\n\r\n");
    const char *body = sep ? sep + 4 : ctx->resp_buf;
    size_t blen = strlen(body);
    return parse_response(body, blen, message);
}
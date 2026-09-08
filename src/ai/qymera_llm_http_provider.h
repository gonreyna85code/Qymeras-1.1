/**
 * Qymera Dashboard - LLM HTTP Provider transport
 *
 * A concrete, bounded provider transport implementing qymera_llm_provider_t
 * against an OpenAI-compatible chat-completions endpoint (also served by
 * Ollama's /api/chat with tools). It builds a compact JSON request body
 * (model + system + user prompt + a tool catalog DERIVED from the Skill
 * registry) and parses the single bounded response into one
 * qymera_llm_message_t (TEXT / TOOL_CALL / MALFORMED / PROVIDER_ERROR /
 * TIMEOUT).
 *
 * Constraints (same philosophy as the adapter core):
 *  - No JSON library dependency: a minimal, bounded, deterministic JSON
 *    scanner lives in this module and is mirrored by host tests.
 *  - Bounded buffers: one request buffer, one response buffer, fixed at
 *    compile time. No malloc per call (a short-lived TLS session block is the
 *    only heap use on https:// endpoints), no unbounded reads, bounded timeout.
 *  - Only explicit tool calls from the model are surfaced; nothing is ever
 *    executed outside qymera_llm_adapter_process (permission/budget guards).
 *  - The tool catalog given to the model is DERIVED from the Skill registry
 *    (qymera_llm_adapter_tool_*), never a second hard-coded list.
 */
#pragma once

#include "qymera_types.h"
#include "qymera_llm_adapter.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

struct qymera_log_s;
typedef struct qymera_log_s qymera_log_t;   /* fwd decl, matches qymera_log.h */

#ifdef __cplusplus
extern "C" {
#endif

/* =========================
 * Bounded transport capacities
 * ========================= */
#define QYMERA_LLM_HTTP_ENDPOINT_LEN  128
#define QYMERA_LLM_HTTP_APIKEY_LEN    64
#define QYMERA_LLM_HTTP_REQ_BUF       4096     /* bounded outbound JSON body  */
#define QYMERA_LLM_HTTP_RESP_BUF      8192     /* bounded inbound HTTP response */
#define QYMERA_LLM_HTTP_DEFAULT_TIMEOUT_MS 8000

/* Fixed, deterministic system prompt (kept tiny; instructs the model to stay
 * inside the provided tool catalog). */
#define QYMERA_LLM_HTTP_SYSTEM_PROMPT \
    "You are Qymera's home-automation assistant. Only call the provided tools. " \
    "Never invent tool names or arguments. If an action is needed, emit exactly " \
    "one tool call. Otherwise reply concisely in plain text."

/* =========================
 * Configuration + transport context
 * ========================= */
typedef struct {
    char endpoint[QYMERA_LLM_HTTP_ENDPOINT_LEN];  /* e.g. http://host:11434/v1/chat/completions */
    char api_key[QYMERA_LLM_HTTP_APIKEY_LEN];     /* optional Bearer key       */
    char model[QYMERA_LLM_MODEL_LEN];             /* fallback if request lacks model */
    uint32_t timeout_ms;                          /* connect + recv timeout    */
} qymera_llm_http_config_t;

struct qymera_llm_http_ctx_s {
    qymera_llm_http_config_t config;
    /* A single bounded I/O buffer, allocated on the heap only after the
     * connect + TLS handshake complete: the mbedtls session alone needs two
     * ~16 KB content buffers (plus context), and holding our request/response
     * buffers at the same time starves the RSA modpow scratch during the
     * ServerKeyExchange verify (mbedtls -0x4290 = RSA_PUBLIC_FAILED |
     * MPI_ALLOC_FAILED on this board). The one buffer is reused for the
     * outbound JSON body and then for the inbound HTTP response. Freed when
     * the turn ends. */
    char *io_buf;    /* heap, capacity QYMERA_LLM_HTTP_REQ_BUF (also RESP) */
    qymera_log_t *log;   /* optional logging handle (set by the owner) */
};
typedef struct qymera_llm_http_ctx_s qymera_llm_http_ctx_t;

/* =========================
 * Provider binding
 * ========================= */
void qymera_llm_http_provider_init(qymera_llm_provider_t *provider,
                                   qymera_llm_http_ctx_t *ctx);

/* Single outbound chat round-trip (host/testable core).
 *
 * Builds the bounded JSON request body into `req_buf` (NUL-terminated) for a
 * chat-completions request carrying the registry-derived tool catalog, then
 * performs the HTTP POST and parses the response into `message`. Returns
 * QYMERA_OK and sets message->kind (TOOL_CALL / TEXT / MALFORMED /
 * PROVIDER_ERROR / TIMEOUT). Returns QYMERA_ERR_INVALID_ARG on bad inputs.
 */
qymera_err_t qymera_llm_http_complete(qymera_llm_http_ctx_t *ctx,
                                      const qymera_llm_request_t *request,
                                      qymera_llm_message_t *message);

#ifdef __cplusplus
}
#endif
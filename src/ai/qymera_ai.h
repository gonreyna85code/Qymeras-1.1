/**
 * Qymera Dashboard - AI Boundary
 * Abstraction for external AI inference (not the agent itself)
 */
#pragma once

#include "qymera_types.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================
 * AI Provider Modes
 * ========================= */

typedef enum {
    QYMERA_AI_MODE_NONE = 0,      // No AI (factory default)
    QYMERA_AI_MODE_LOCAL,         // Local/on-LAN OpenAI-compatible
    QYMERA_AI_MODE_REMOTE,        // Cloud provider
    QYMERA_AI_MODE_HYBRID,        // Local preferred, remote fallback
} qymera_ai_mode_t;

/* =========================
 * Inference Request
 * ========================= */

typedef struct {
    char request_id[32];
    char model[64];
    char prompt[512];
    char system_prompt[512];
    float temperature;
    uint16_t max_tokens;
    uint32_t timeout_ms;
    bool stream;
} qymera_inference_request_t;

/* =========================
 * Inference Response
 * ========================= */

typedef struct {
    char request_id[32];
    bool valid;
    char text[1024];
    float numeric_value;
    bool bool_value;
    uint32_t latency_ms;
    uint16_t tokens_used;
    char error[128];
} qymera_inference_response_t;

/* =========================
 * Inference Condition (for rules)
 * ========================= */

typedef struct {
    char provider[32];
    char model[64];
    char prompt_template[512];  // Template with {entity} placeholders
    qymera_entity_ref_t result_entity;  // Where to store result
    uint8_t result_type;        // 0=bool, 1=float, 2=category
    uint32_t timeout_ms;
    uint32_t cache_ms;
    char fallback_policy[16];   // "fail_open", "fail_closed", "last_known"
} qymera_inference_condition_t;

/* =========================
 * AI Provider Configuration
 * ========================= */

typedef struct {
    qymera_ai_mode_t mode;
    char local_endpoint[128];
    char local_api_key[64];
    char remote_endpoint[128];
    char remote_api_key[64];
    char default_model[64];
    uint16_t default_timeout_ms;
    uint32_t default_rate_limit_ms;
    uint32_t default_cache_ms;
} qymera_ai_config_t;

/* =========================
 * AI Context (for agent)
 * ========================= */

typedef struct {
    char device_summary[2048];
    char entity_summary[2048];
    char rule_summary[2048];
    char recent_events[2048];
    char analytics_summary[1024];
    uint32_t generated_at;
} qymera_ai_context_t;

/* =========================
 * AI Handle
 * ========================= */

typedef struct qymera_ai_s qymera_ai_t;

/* =========================
 * AI API
 * ========================= */

/**
 * Initialize AI subsystem
 * @param ai      Output AI handle
 * @param config  AI configuration
 * @return QYMERA_OK on success
 */
qymera_err_t qymera_ai_init(qymera_ai_t **ai, const qymera_ai_config_t *config);

/**
 * Execute inference request
 * @param ai       AI handle
 * @param request  Inference request
 * @param response Output response
 * @return QYMERA_OK on success
 */
qymera_err_t qymera_ai_infer(qymera_ai_t *ai, const qymera_inference_request_t *request, qymera_inference_response_t *response);

/**
 * Evaluate inference condition (for rule engine)
 * @param ai          AI handle
 * @param condition   Inference condition
 * @param context     Current entity values for template substitution
 * @param result      Output result entity value
 * @return QYMERA_OK on success
 */
qymera_err_t qymera_ai_eval_condition(qymera_ai_t *ai, const qymera_inference_condition_t *condition,
                                       const qymera_entity_value_t *context, qymera_entity_value_t *result);

/**
 * Generate AI context bundle (for agent)
 * @param ai      AI handle
 * @param hints   Optional hints for what to include
 * @param context Output context bundle
 * @return QYMERA_OK on success
 */
qymera_err_t qymera_ai_get_context(qymera_ai_t *ai, const char *hints, qymera_ai_context_t *context);

/**
 * Set AI mode
 * @param ai   AI handle
 * @param mode New mode
 * @return QYMERA_OK on success
 */
qymera_err_t qymera_ai_set_mode(qymera_ai_t *ai, qymera_ai_mode_t mode);

/**
 * Get current AI mode
 * @param ai   AI handle
 * @return Current mode
 */
qymera_ai_mode_t qymera_ai_get_mode(qymera_ai_t *ai);

/**
 * Check if AI is available
 * @param ai AI handle
 * @return true if AI provider reachable
 */
bool qymera_ai_is_available(qymera_ai_t *ai);

/**
 * Get AI statistics
 * @param ai    AI handle
 * @param requests_total Total requests
 * @param failures Total failures
 * @param cache_hits Cache hits
 * @param avg_latency_ms Average latency
 */
void qymera_ai_get_stats(qymera_ai_t *ai, uint32_t *requests_total, uint32_t *failures,
                          uint32_t *cache_hits, uint32_t *avg_latency_ms);

#ifdef __cplusplus
}
#endif
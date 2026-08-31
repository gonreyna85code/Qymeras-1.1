/**
 * Qymera Dashboard - AI Boundary Implementation
 * Stub implementation for AI inference abstraction
 */
#include "qymera_ai.h"
#include "qymera_log.h"
#include "qymera_hal.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

struct qymera_ai_s {
    qymera_ai_config_t config;
    qymera_log_t *log;
    
    uint32_t requests_total;
    uint32_t failures;
    uint32_t cache_hits;
    uint32_t total_latency_ms;
    
    bool available;
};

qymera_err_t qymera_ai_init(qymera_ai_t **ai, const qymera_ai_config_t *config) {
    if (!ai) return QYMERA_ERR_INVALID_ARG;
    
    qymera_ai_t *a = calloc(1, sizeof(qymera_ai_t));
    if (!a) return QYMERA_ERR_NO_SPACE;
    
    if (config) {
        a->config = *config;
    } else {
        // Defaults
        a->config.mode = QYMERA_AI_MODE_NONE;
        a->config.default_timeout_ms = 5000;
        a->config.default_cache_ms = 30000;
        strncpy(a->config.default_model, "gpt-4o-mini", sizeof(a->config.default_model) - 1);
    }
    
    a->available = false;
    *ai = a;
    return QYMERA_OK;
}

qymera_err_t qymera_ai_infer(qymera_ai_t *ai, const qymera_inference_request_t *request, qymera_inference_response_t *response) {
    if (!ai || !request || !response) return QYMERA_ERR_INVALID_ARG;
    
    memset(response, 0, sizeof(qymera_inference_response_t));
    strncpy(response->request_id, request->request_id, sizeof(response->request_id) - 1);
    
    if (ai->config.mode == QYMERA_AI_MODE_NONE) {
        snprintf(response->error, sizeof(response->error), "AI disabled (mode=NONE)");
        ai->failures++;
        return QYMERA_ERR_INVALID_STATE;
    }
    
    // Stub: In a real implementation, this would make HTTP request to provider
    // For now, return a deterministic mock response
    response->valid = true;
    snprintf(response->text, sizeof(response->text), "[MOCK] Response to: %.50s", request->prompt);
    response->latency_ms = 10;
    response->tokens_used = 50;
    
    ai->requests_total++;
    ai->total_latency_ms += response->latency_ms;
    
    return QYMERA_OK;
}

qymera_err_t qymera_ai_eval_condition(qymera_ai_t *ai, const qymera_inference_condition_t *condition,
                                       const qymera_entity_value_t *context, qymera_entity_value_t *result) {
    if (!ai || !condition || !result) return QYMERA_ERR_INVALID_ARG;
    
    memset(result, 0, sizeof(qymera_entity_value_t));
    
    if (ai->config.mode == QYMERA_AI_MODE_NONE) {
        // Apply fallback policy
        if (strcmp(condition->fallback_policy, "fail_open") == 0) {
            result->valid = true;
            result->bool_value = true;
            result->numeric_value = 1.0f;
        } else if (strcmp(condition->fallback_policy, "fail_closed") == 0) {
            result->valid = true;
            result->bool_value = false;
            result->numeric_value = 0.0f;
        } else {
            result->valid = false;
        }
        return QYMERA_OK;
    }
    
    // Build request from template
    qymera_inference_request_t request = {0};
    snprintf(request.request_id, sizeof(request.request_id), "cond_%lu", qymera_system_get_uptime_ms());
    strncpy(request.model, condition->model[0] ? condition->model : ai->config.default_model, sizeof(request.model) - 1);
    strncpy(request.prompt, condition->prompt_template, sizeof(request.prompt) - 1);
    request.timeout_ms = condition->timeout_ms ? condition->timeout_ms : ai->config.default_timeout_ms;
    request.temperature = 0.1f;
    request.max_tokens = 10;
    
    qymera_inference_response_t response;
    qymera_err_t err = qymera_ai_infer(ai, &request, &response);
    
    if (err != QYMERA_OK || !response.valid) {
        // Apply fallback
        if (strcmp(condition->fallback_policy, "fail_open") == 0) {
            result->valid = true;
            result->bool_value = true;
            result->numeric_value = 1.0f;
        } else if (strcmp(condition->fallback_policy, "fail_closed") == 0) {
            result->valid = true;
            result->bool_value = false;
            result->numeric_value = 0.0f;
        } else if (strcmp(condition->fallback_policy, "last_known") == 0) {
            // Would return cached result
            result->valid = false;
        } else {
            result->valid = false;
        }
        return QYMERA_OK;
    }
    
    // Parse response based on result_type
    result->valid = true;
    result->timestamp = qymera_timestamp_now();
    
    if (condition->result_type == 0) {  // boolean
        result->bool_value = (strstr(response.text, "true") != NULL || strstr(response.text, "yes") != NULL);
        result->numeric_value = result->bool_value ? 1.0f : 0.0f;
    } else if (condition->result_type == 1) {  // float
        result->numeric_value = strtof(response.text, NULL);
        result->bool_value = (result->numeric_value != 0.0f);
    }
    
    return QYMERA_OK;
}

qymera_err_t qymera_ai_get_context(qymera_ai_t *ai, const char *hints, qymera_ai_context_t *context) {
    if (!ai || !context) return QYMERA_ERR_INVALID_ARG;
    
    memset(context, 0, sizeof(qymera_ai_context_t));
    context->generated_at = qymera_system_get_uptime_ms();
    
    // Stub: In real implementation, this would query registry, rules, events, analytics
    snprintf(context->device_summary, sizeof(context->device_summary), "Dashboard + %d remote devices", 0);
    snprintf(context->entity_summary, sizeof(context->entity_summary), "%d entities registered", 0);
    snprintf(context->rule_summary, sizeof(context->rule_summary), "%d rules loaded", 0);
    snprintf(context->recent_events, sizeof(context->recent_events), "No recent events");
    snprintf(context->analytics_summary, sizeof(context->analytics_summary), "No analytics data");
    
    return QYMERA_OK;
}

qymera_err_t qymera_ai_set_mode(qymera_ai_t *ai, qymera_ai_mode_t mode) {
    if (!ai) return QYMERA_ERR_INVALID_ARG;
    ai->config.mode = mode;
    return QYMERA_OK;
}

qymera_ai_mode_t qymera_ai_get_mode(qymera_ai_t *ai) {
    return ai ? ai->config.mode : QYMERA_AI_MODE_NONE;
}

bool qymera_ai_is_available(qymera_ai_t *ai) {
    if (!ai) return false;
    if (ai->config.mode == QYMERA_AI_MODE_NONE) return false;
    return ai->available;
}

void qymera_ai_get_stats(qymera_ai_t *ai, uint32_t *requests_total, uint32_t *failures,
                          uint32_t *cache_hits, uint32_t *avg_latency_ms) {
    if (!ai) return;
    if (requests_total) *requests_total = ai->requests_total;
    if (failures) *failures = ai->failures;
    if (cache_hits) *cache_hits = ai->cache_hits;
    if (avg_latency_ms) *avg_latency_ms = ai->requests_total ? (ai->total_latency_ms / ai->requests_total) : 0;
}
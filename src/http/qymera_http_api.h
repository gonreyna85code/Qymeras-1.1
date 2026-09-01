/**
 * Qymera Dashboard - HTTP API boundary over the Skill layer
 *
 * This module provides a REST API that routes ALL operations through
 * qymera_skill_execute(). No direct GPIO, UDP, registry, rule engine,
 * or storage access from the HTTP layer. The Skill API is the single,
 * authoritative path.
 *
 * HTTP → Skill adapter → qymera_skill_execute → deterministic runtime
 *
 * All endpoints are under /api/v1/. The Skill error codes remain the
 * machine-readable application error; they are included in HTTP
 * responses alongside the status code.
 */

#pragma once

#include <esp_http_server.h>
#include "qymera_core.h"
#include "qymera_skill.h"
#include "qymera_types.h"

/* =========================
 * HTTP API configuration
 * ========================= */

#define QYMERA_HTTP_API_BASE_PATH "/api/v1"
#define QYMERA_HTTP_MAX_URI_LEN  64
#define QYMERA_HTTP_BODY_BUF_SZ  512

/* Permission masks taken from qymera_permission_t */
#define QYMERA_PERM_READ       (1 << 0)
#define QYMERA_PERM_CONTROL    (1 << 1)
#define QYMERA_PERM_RULE_READ  (1 << 2)
#define QYMERA_PERM_RULE_WRITE (1 << 3)

/* =========================
 * HTTP API result envelope
 * ========================= */

/* Sent on skill success: the data payload is the skill's JSON output. */
typedef struct {
    bool ok;
    char data[QYMERA_SKILL_OUTPUT_SIZE];  /* exactly one valid JSON value on success */
    size_t data_len;
    /* Error fields (used when ok=false) */
    char error_code[QYMERA_SKILL_ERROR_CODE_LEN];
    char message[QYMERA_SKILL_MESSAGE_LEN];
} qymera_http_api_result_t;

/* Sent on skill failure: error code + message from the skill. */
typedef struct {
    bool ok;
    char error_code[QYMERA_SKILL_ERROR_CODE_LEN];
    char message[QYMERA_SKILL_MESSAGE_LEN];
} qymera_http_api_error_t;

/* =========================
 * Initialization
 * ========================= */

qymera_err_t qymera_http_api_init(qymera_core_t *core);

/* =========================
 * Skill-to-endpoint mapping helpers
 * ========================= */

/* Build a qymera_skill_input_t from a simple JSON body extracted from
 * an httpd_req_t. Returns true on success, false on parse failure. */
bool qymera_http_api_parse_simple_input(const char *json_body,
                                        qymera_skill_input_t *out);

/* Build a qymera_skill_input_t from a rule JSON body (create_rule /
 * update_rule). Returns true on success. */
bool qymera_http_api_parse_rule_input(const char *json_body,
                                      qymera_skill_input_t *out);

/* Map a skill error code to an HTTP status code. The caller should
 * also include the error_code string in the HTTP response body. */
uint16_t qymera_http_api_map_error_to_status(const char *error_code);

/* =========================
 * Result serialization
 * ========================= */

/* Serialize a qymera_skill_output_t into the HTTP result envelope.
 * On success, data contains the skill's JSON output. */
void qymera_http_api_serialize_result(qymera_skill_output_t *output,
                                     qymera_http_api_result_t *result);

/* Serialize a skill error into the HTTP error envelope. */
void qymera_http_api_serialize_error(qymera_skill_output_t *output,
                                    qymera_http_api_error_t *error);

/* Send the HTTP result/envelope over the given request. */
void qymera_http_api_send_result(httpd_req_t *req, qymera_http_api_result_t *result);
void qymera_http_api_send_error(httpd_req_t *req, qymera_http_api_error_t *error);
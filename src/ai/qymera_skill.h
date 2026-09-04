/**
 * Qymera Dashboard - Deterministic Skill API
 *
 * The single, machine-readable surface an external agent (future LLM/adapter,
 * web UI, or automation) uses to observe and control Qymera. This layer is
 * intentionally deterministic: it only validates structured input and calls
 * the existing deterministic APIs (Registry, Rule Engine, Control API). It
 * never touches GPIO, UDP, or internal structures directly, and it never parses
 * natural language nor connects to any inference provider.
 *
 * The Skill layer is caller-agnostic (Ollama / OpenAI / local model / remote
 * model / human UI / automation are all identical callers here).
 */
#pragma once

#include "qymera_types.h"
#include "qymera_registry.h"
#include "qymera_control.h"
#include "qymera_rule.h"
#include "qymera_storage.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Bounded capacity: 13 fixed, build-time skills. No dynamic registry. */
#define QYMERA_MAX_SKILLS            13
#define QYMERA_SKILL_NAME_LEN        24
#define QYMERA_SKILL_VERSION_LEN     8
#define QYMERA_SKILL_DESC_LEN        96
#define QYMERA_SKILL_SCHEMA_LEN      32
#define QYMERA_SKILL_OUTPUT_SIZE     1024
#define QYMERA_SKILL_ERROR_CODE_LEN  32
#define QYMERA_SKILL_MESSAGE_LEN     160

/* Stable machine-readable error codes (never change between releases). */
#define QYMERA_SKILL_ERR_SKILL_NOT_FOUND    "SKILL_NOT_FOUND"
#define QYMERA_SKILL_ERR_PERMISSION_DENIED  "PERMISSION_DENIED"
#define QYMERA_SKILL_ERR_ENTITY_NOT_FOUND   "ENTITY_NOT_FOUND"
#define QYMERA_SKILL_ERR_INVALID_CAPABILITY "INVALID_CAPABILITY"
#define QYMERA_SKILL_ERR_INVALID_VALUE      "INVALID_VALUE"
#define QYMERA_SKILL_ERR_INVALID_INPUT      "INVALID_INPUT"
#define QYMERA_SKILL_ERR_RULE_INVALID       "RULE_INVALID"
#define QYMERA_SKILL_ERR_RULE_CONFLICT      "RULE_CONFLICT"
#define QYMERA_SKILL_ERR_NO_SPACE           "NO_SPACE"
#define QYMERA_SKILL_ERR_STORAGE_ERROR      "STORAGE_ERROR"
#define QYMERA_SKILL_ERR_OUTPUT_TOO_LARGE   "OUTPUT_TOO_LARGE"
#define QYMERA_SKILL_ERR_DEVICE_OFFLINE     "DEVICE_OFFLINE"
#define QYMERA_SKILL_ERR_COMMAND_TIMEOUT    "COMMAND_TIMEOUT"
#define QYMERA_SKILL_ERR_DEPENDENCY_MISSING "DEPENDENCY_MISSING"

/* =========================
 * Permission model (authorization boundary, not a security system)
 * ========================= */
typedef enum {
    QYMERA_PERM_NONE       = 0,
    QYMERA_PERM_READ       = (1 << 0),
    QYMERA_PERM_CONTROL    = (1 << 1),
    QYMERA_PERM_RULE_READ  = (1 << 2),
    QYMERA_PERM_RULE_WRITE = (1 << 3),
} qymera_permission_t;

/* =========================
 * Skill identifiers (fixed table index)
 * ========================= */
typedef enum {
    QYMERA_SKILL_DEVICES_LIST = 0,
    QYMERA_SKILL_ENTITIES_LIST,
    QYMERA_SKILL_ENTITY_STATE_GET,
    QYMERA_SKILL_ENTITY_INFO_GET,
    QYMERA_SKILL_RELAY_SET,
    QYMERA_SKILL_DIMMER_SET,
    QYMERA_SKILL_RULES_LIST,
    QYMERA_SKILL_RULE_GET,
    QYMERA_SKILL_RULE_CREATE,
    QYMERA_SKILL_RULE_UPDATE,
    QYMERA_SKILL_RULE_DELETE,
    QYMERA_SKILL_RULE_ENABLE,
    QYMERA_SKILL_RULE_DISABLE,
} qymera_skill_id_t;

/* =========================
 * Skill metadata (name/version/description/schema/permissions)
 * ========================= */
typedef struct {
    char name[QYMERA_SKILL_NAME_LEN];
    char version[QYMERA_SKILL_VERSION_LEN];
    char description[QYMERA_SKILL_DESC_LEN];
    char schema_id[QYMERA_SKILL_SCHEMA_LEN];
    uint32_t permissions;        /* Required permission mask */
} qymera_skill_meta_t;

/* Single registry entry. The full table is defined at compile time. */
typedef struct {
    qymera_skill_id_t id;
    qymera_skill_meta_t meta;
} qymera_skill_entry_t;

/* =========================
 * Structured Skill input
 *
 * Bounded union of the parameters each skill accepts. A future LLM adapter
 * maps a JSON tool call onto this struct; it does not arrive as natural
 * language and is not parsed heuristically here.
 * ========================= */
typedef struct {
    char device_id[QYMERA_DEVICE_ID_LEN];
    char entity_id[QYMERA_ENTITY_ID_LEN];
    char name[64];
    char rule_id[QYMERA_RULE_ID_LEN];

    bool value;              /* set_relay */
    uint8_t level;           /* set_dimmer (0-100) */

    qymera_rule_t rule;      /* create_rule / update_rule (structured) */
    bool enabled;            /* enable_rule / disable_rule */
} qymera_skill_input_t;

/* =========================
 * Skill output / result model
 *
 * The builder guarantees the serialized form is always one of two stable
 * envelopes (never malformed, never truncated JSON):
 *
 *   { "ok": true,  "data": <bounded valid JSON fragment> }
 *   { "ok": false, "error": { "code": "...", "message": "...", "details": <...> } }
 *
 * On success `data` holds exactly one valid JSON value (object or array). Every
 * string inserted into `data` is JSON-escaped. If the fragment would exceed
 * QYMERA_SKILL_OUTPUT_SIZE the skill returns `OUTPUT_TOO_LARGE` (ok=false) and
 * leaves `data` empty — it never returns truncated/malformed JSON.
 * ========================= */
typedef struct {
    bool ok;
    bool truncated;                        /* internal: set on overflow; resolved to OUTPUT_TOO_LARGE */
    char error_code[QYMERA_SKILL_ERROR_CODE_LEN]; /* machine-stable code */
    char message[QYMERA_SKILL_MESSAGE_LEN];       /* human-readable detail */
    char data[QYMERA_SKILL_OUTPUT_SIZE];          /* exactly one valid JSON value on success, else empty */
    size_t data_len;
} qymera_skill_output_t;

/* =========================
 * Skill context: typed references to the deterministic runtime.
 * The Skill layer only ever talks through these.
 * ========================= */
typedef struct {
    qymera_registry_t *registry;
    qymera_rule_engine_t *rule_engine;
    qymera_control_context_t *control;
    qymera_storage_t *storage;
    qymera_log_t *log;
} qymera_skill_context_t;

/* =========================
 * Skill API
 * ========================= */

qymera_err_t qymera_skill_context_init(qymera_skill_context_t *ctx,
                                       qymera_registry_t *registry,
                                       qymera_rule_engine_t *rule_engine,
                                       qymera_control_context_t *control,
                                       qymera_storage_t *storage,
                                       qymera_log_t *log);

/* Bounded registry discovery. registry_get returns the fixed skill id for a
 * valid index, and (qymera_skill_id_t)-1 for an out-of-range index or NULL
 * entry pointer (it never reports a real skill for an invalid index). lookup
 * returns (qymera_skill_id_t)-1 when the name is unknown or NULL. */
size_t qymera_skill_registry_count(void);
qymera_skill_id_t qymera_skill_registry_get(size_t idx, const qymera_skill_entry_t **entry);
qymera_skill_id_t qymera_skill_lookup(const char *skill_name);

/**
 * Execute a skill by name.
 *
 * @param ctx              Skill context (typed runtime references)
 * @param skill_name       Exact skill name (e.g. "set_relay")
 * @param input            Structured input (may be NULL for no-arg skills)
 * @param output           Output/result model (bounded JSON or error)
 * @param permission_mask  Caller's granted permission mask
 * @return QYMERA_OK if the call itself was valid (domain result in output).
 */
qymera_err_t qymera_skill_execute(qymera_skill_context_t *ctx,
                                  const char *skill_name,
                                  const qymera_skill_input_t *input,
                                  qymera_skill_output_t *output,
                                  uint32_t permission_mask);

/* Stable string helper (exposed for the future adapter / tests). */
const char *qymera_skill_cmd_status_str(qymera_cmd_status_t status);
const char *qymera_skill_reliability_str(uint8_t observed_reliability, qymera_cmd_status_t status);

#ifdef __cplusplus
}
#endif

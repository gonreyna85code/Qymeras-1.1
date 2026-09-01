/**
 * Qymera Dashboard - LLM Adapter
 *
 * The single provider-facing boundary that sits ABOVE the deterministic Skill
 * API. This layer turns a bounded, structured "tool call" produced by an LLM /
 * provider into a call to `qymera_skill_execute()`. It never accesses GPIO, UDP,
 * the Registry, the Rule Engine, Storage, or any raw hardware directly - every
 * action (including rule mutation) flows through the Skill layer, so all
 * validation, permission, JSON-validity, and transactional semantics stay
 * centralized and deterministic.
 *
 * The adapter is deliberately provider-agnostic:
 *
 *   user / external caller
 *        |            (natural language - the model's job, not this layer's)
 *        v
 *      LLM
 *        |            (provider transport - a pluggable qymera_llm_provider_t)
 *        v
 *   LLM Adapter  <-- accepts bounded structured tool calls only
 *        |
 *        v
 *   Skill Executor -> qymera_skill_execute() -> deterministic Qymera runtime
 *
 * Constraint summary:
 *  - No Ollama/OpenAI HTTP, no model download, no JSON library dependency.
 *  - No arbitrary model-provided code execution, no unrestricted scripting.
 *  - No autonomous loops / background planning / agents.
 *  - Bounded memory: no malloc per tool call, no unbounded prompt / history,
 *    no dynamic tool list (the tool catalog is DERIVED from the fixed Skill
 *    registry, not duplicated here).
 *  - The Skill registry (qymera_skill_registry_*) is the single source of truth
 *    for the tool catalog.
 */
#pragma once

#include "qymera_types.h"
#include "qymera_skill.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================
 * Bounded capacities
 * ========================= */
#define QYMERA_MAX_TOOL_CALLS_PER_TURN   8        /* tool->LLM->tool budget (see below) */
#define QYMERA_LLM_PROMPT_LEN            512      /* bounded model prompt / context */
#define QYMERA_LLM_TEXT_LEN              256      /* bounded assistant text / relayed result */
#define QYMERA_LLM_RESULT_LEN            760      /* bounded per-turn outcome transcript */
#define QYMERA_LLM_REQUEST_ID_LEN        24
#define QYMERA_LLM_PROVIDER_ID_LEN       24
#define QYMERA_LLM_MODEL_LEN             64

/*
 * Tool budget justification (QYMERA_MAX_TOOL_CALLS_PER_TURN = 8):
 * The canonical multi-step workflow (list_entities -> get_entity_state ->
 * create_rule -> enable_rule -> set_relay -> observe state) needs at most 6
 * sequential tool calls. 8 leaves headroom for a couple of discovery/retry
 * calls while keeping the per-turn working set (one provider message + one
 * skill output buffer) small enough for ESP32 and preventing pathological
 * tool/agent loops. Exceeding the budget stops the turn with TOOL_CALL_LIMIT.
 */

/* =========================
 * Structured tool arguments (bounded carrier)
 *
 * The adapter only accepts structured arguments. It does NOT parse natural
 * language. For convenience a strict, bounded JSON tool-arguments extractor is
 * provided (qymera_llm_addr_args_from_json) so JSON-form structured arguments
 * from a provider can be mapped onto this carrier.
 * ========================= */
typedef struct {
    char device_id[QYMERA_DEVICE_ID_LEN];
    char entity_id[QYMERA_ENTITY_ID_LEN];
    char name[64];
    char rule_id[QYMERA_RULE_ID_LEN];

    bool value;              /* set_relay */
    uint8_t level;           /* set_dimmer (0-100) */
    bool value_set;

    qymera_rule_t rule;      /* create_rule / update_rule (structured) */
    bool has_rule;           /* true when a rule body was supplied   */

    bool enabled;            /* enable_rule / disable_rule */
    bool enabled_set;
} qymera_llm_tool_arguments_t;

/* =========================
 * Provider message kinds (distinguish model outputs, never assume a tool call)
 * ========================= */
typedef enum {
    QYMERA_LLM_MSG_NONE = 0,
    QYMERA_LLM_MSG_TEXT,        /* assistant text only         */
    QYMERA_LLM_MSG_TOOL_CALL,   /* explicit structured tool call */
    QYMERA_LLM_MSG_MALFORMED,   /* structurally invalid output */
    QYMERA_LLM_MSG_PROVIDER_ERROR,
    QYMERA_LLM_MSG_TIMEOUT,
} qymera_llm_message_kind_t;

/* One bounded message from the provider transport. */
typedef struct {
    qymera_llm_message_kind_t kind;
    char tool_name[QYMERA_SKILL_NAME_LEN];
    qymera_llm_tool_arguments_t args;   /* valid when kind == TOOL_CALL */
    char text[QYMERA_LLM_TEXT_LEN];     /* assistant text or error detail */
} qymera_llm_message_t;

/* Bounded request/session context passed to the provider transport. */
typedef struct {
    char request_id[QYMERA_LLM_REQUEST_ID_LEN];
    char provider_id[QYMERA_LLM_PROVIDER_ID_LEN];
    char model[QYMERA_LLM_MODEL_LEN];
    char prompt[QYMERA_LLM_PROMPT_LEN];   /* bounded prompt/context */
    uint32_t permission_mask;
    uint8_t max_tool_calls;
} qymera_llm_request_t;

/* =========================
 * Provider interface (pluggable transport stub)
 *
 * A concrete provider (Ollama, OpenAI, a local/remote model) is implemented
 * against this interface LATER and lives OUTSIDE the deterministic Skill
 * module. No HTTP / SDK code lives in the Skill layer or the adapter core.
 * The interface is intentionally tiny and bounded: produce one bounded message.
 * ========================= */
typedef qymera_err_t (*qymera_llm_provider_complete_fn)(
    void *provider_ctx,
    const qymera_llm_request_t *request,
    qymera_llm_message_t *message);

typedef struct {
    void *provider_ctx;
    qymera_llm_provider_complete_fn complete;
} qymera_llm_provider_t;

/* =========================
 * Tool execution result classification
 *
 * Envelope-level failures (unknown tool / missing or malformed arguments /
 * permission / budget) are classified here and NEVER reach the runtime. They
 * are still serialized through the stable Skill envelope (qymera_skill_output_t)
 * so the adapter does not introduce a second error taxonomy on the wire.
 * ========================= */
typedef enum {
    QYMERA_LLM_TOOL_OK = 0,       /* dispatched; `out` holds the skill result */
    QYMERA_LLM_TOOL_UNKNOWN,      /* skill/tool name not in the registry      */
    QYMERA_LLM_TOOL_MISSING_ARGS, /* arguments absent where required          */
    QYMERA_LLM_TOOL_BAD_ARGS,     /* arguments structurally invalid           */
    QYMERA_LLM_TOOL_MALFORMED,    /* malformed model output rejected          */
    QYMERA_LLM_TOOL_PERMISSION,   /* caller lacks the required permission bit */
    QYMERA_LLM_TOOL_LIMIT,        /* per-turn tool budget exhausted           */
} qymera_llm_tool_error_t;

/* =========================
 * Turn outcome (bounded)
 * ========================= */
typedef enum {
    QYMERA_LLM_TURN_TEXT = 0,       /* ended on assistant text         */
    QYMERA_LLM_TURN_TOOL_CALL_LIMIT,/* ended because the budget hit    */
    QYMERA_LLM_TURN_PROVIDER_ERROR, /* provider error / timeout        */
    QYMERA_LLM_TURN_MALFORMED,      /* malformed provider output       */
} qymera_llm_turn_end_t;

typedef struct {
    qymera_llm_turn_end_t ended;
    uint8_t tool_calls;                 /* number of tool calls executed in the turn */
    char outcome[QYMERA_LLM_RESULT_LEN]; /* bounded textual transcript of the turn    */
    qymera_llm_message_t final;        /* final raw message (text/error/malformed)    */
} qymera_llm_turn_result_t;

/* =========================
 * Adapter handle (opaque)
 * ========================= */
typedef struct qymera_llm_adapter_s qymera_llm_adapter_t;

/* =========================
 * Adapter API
 * ========================= */

/* Bind the adapter to the Skill context so every tool call is dispatched via
 * qymera_skill_execute(). */
qymera_err_t qymera_llm_adapter_init(qymera_llm_adapter_t **adapter,
                                     qymera_skill_context_t *skills,
                                     qymera_log_t *log);

/* Tool catalog DERIVED from the Skill registry (source of truth). There is no
 * second, manually-maintained tool list. */
size_t qymera_llm_adapter_tool_count(void);
qymera_skill_id_t qymera_llm_adapter_tool_at(size_t idx, const qymera_skill_meta_t **meta);
qymera_skill_id_t qymera_llm_adapter_tool_lookup(const char *name);

/* Execute one validated structured tool call strictly through
 * qymera_skill_execute(). Envelope validation happens here and never reaches
 * the runtime. On QYMERA_LLM_TOOL_OK, `out` holds the unchanged Skill result
 * (standard {ok:true,data} / {ok:false,error} envelope). */
qymera_err_t qymera_llm_adapter_execute_tool(qymera_llm_adapter_t *adapter,
                                             const char *tool_name,
                                             const qymera_llm_tool_arguments_t *args,
                                             uint32_t permission_mask,
                                             qymera_skill_output_t *out,
                                             qymera_llm_tool_error_t *tool_err);

/* Run a bounded turn: loop tool-call -> skill -> result -> model, guarded by
 * QYMERA_MAX_TOOL_CALLS_PER_TURN (recursion protection). Only explicit tool
 * calls from the model are executed - no autonomous behavior. */
qymera_err_t qymera_llm_adapter_process(qymera_llm_adapter_t *adapter,
                                        const qymera_llm_provider_t *provider,
                                        const qymera_llm_request_t *request,
                                        qymera_llm_turn_result_t *result);

/* Strict, bounded JSON tool-arguments extractor. Validates that `json` is a
 * JSON object and maps the flat fields of qymera_llm_tool_arguments_t. Used to
 * accept JSON-form structured arguments without a JSON library dependency.
 * Returns QYMERA_OK and a zeroed `args` on success, or QYMERA_ERR_INVALID_ARG
 * on malformed input. */
qymera_err_t qymera_llm_args_from_json(const char *json, size_t json_len,
                                       qymera_llm_tool_arguments_t *args);

/* Deterministic mock provider (compiled in). Drives the documented structured
 * workflow with fixed tool calls. Useful for wiring and for demonstrating the
 * adapter end-to-end without any real LLM. */
#define QYMERA_LLM_MOCK_MAX_STEPS 8
typedef struct {
    uint8_t step;                 /* next step to emit (0-based)     */
} qymera_llm_mock_ctx_t;

/* Bind `provider` to a caller-allocated qymera_llm_mock_ctx_t. On each
 * complete() the mock emits the next fixed step of the canonical workflow
 * (discover -> inspect -> build rule -> create -> enable -> act -> observe),
 * then a final assistant-text step. */
void qymera_llm_mock_provider_init(qymera_llm_provider_t *provider, void *ctx);

#ifdef __cplusplus
}
#endif

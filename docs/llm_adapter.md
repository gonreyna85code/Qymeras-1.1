# Qymera LLM Adapter (Phase 3C)

The single **provider-facing boundary** that sits strictly **above** the
deterministic Skill API. It turns a bounded, structured **tool call** produced
by an LLM / provider into a call to `qymera_skill_execute()`. Every action —
including rule mutation and relay/dimmer control — flows through the Skill layer,
so all validation, permission, JSON-validity, and transactional semantics stay
centralized in the deterministic runtime.

> **Core principle:** *LLM proposes → Skills validate → deterministic runtime
> executes.*

The adapter **never** accesses GPIO, UDP, the Registry internals, the Rule
Engine internals, Storage, or Matter directly, and it **never parses natural
language** (that stays the model's job). There is no arbitrary model-provided
code execution and no autonomous agent loop.

Implementation: `src/ai/qymera_llm_adapter.h` / `qymera_llm_adapter.c`.

---

## Architecture

```text
User / external caller
        |            (natural language — the model's job)
        v
      LLM
        |            (provider transport — a pluggable qymera_llm_provider_t)
        v
   LLM Adapter  <-- accepts bounded structured tool calls only
        |
        v
   Skill Executor -> qymera_skill_execute()
        |
        v
deterministic Qymera runtime (Registry / Event Bus / Rule Engine / Control /
UDP / ACK-state confirmation / Skill API)
```

The deterministic runtime and the Skill API are the **authoritative execution
layer**. The LLM and the adapter live strictly above it. Skill execution is
**identical** whether the model is local or remote, or whether the caller is an
LLM, a web UI, or an automation — the Skill layer is caller-agnostic.

---

## Provider boundary

The adapter is coupled to a tiny, provider-agnostic transport interface, not to
any specific SDK:

```c
typedef qymera_err_t (*qymera_llm_provider_complete_fn)(
    void *provider_ctx,
    const qymera_llm_request_t *request,
    qymera_llm_message_t *message);

typedef struct {
    void *provider_ctx;
    qymera_llm_provider_complete_fn complete;
} qymera_llm_provider_t;
```

A concrete provider (Ollama, OpenAI, a local/remote model) is implemented
against this interface **later** and lives outside the Skill module. No
HTTP / Ollama / OpenAI / Gemma / Qwen-specific syntax is embedded in
`qymera_skill.c` or in the adapter core.

The provider returns one **bounded message** which the adapter classifies:

| Kind | Meaning |
|------|---------|
| `QYMERA_LLM_MSG_TEXT` | assistant text only (turn ends) |
| `QYMERA_LLM_MSG_TOOL_CALL` | explicit structured tool call |
| `QYMERA_LLM_MSG_MALFORMED` | structurally invalid model output |
| `QYMERA_LLM_MSG_PROVIDER_ERROR` | provider failure |
| `QYMERA_LLM_MSG_TIMEOUT` | provider did not respond in time |

The adapter never assumes every response is a tool call.

### Local vs remote

The adapter supports the concept of local vs remote inference without affecting
skill execution: the provider `complete` callback is the only place that differs.
The Skill layer remains identical in both cases.

---

## Tool catalog (derived, single source of truth)

The adapter does **not** keep a second, manually-duplicated tool list. Every
tool is derived from the existing Skill registry:

```c
size_t qymera_llm_adapter_tool_count(void);                 /* = registry count (13) */
qymera_skill_id_t qymera_llm_adapter_tool_at(size_t idx, const qymera_skill_meta_t **meta);
qymera_skill_id_t qymera_llm_adapter_tool_lookup(const char *name);
```

```text
Skill:
set_relay
  ↓
LLM tool:
set_relay
```

The Skill registry remains the source of truth. Each skill exposes machine-
readable metadata: `name`, `version`, `description`, `schema_id`, and the
required `permissions` bit — that metadata *is* the tool schema.

### Tool schemas and required fields

Each exposed skill maps to a bounded structured input (`qymera_skill_input_t`).
The adapter's structured arguments carrier (`qymera_llm_tool_arguments_t`) holds
the flat fields; the adapter validates presence of required fields per skill:

| Skill(s) | Required arguments |
|----------|--------------------|
| `list_devices`, `list_entities`, `list_rules` | none |
| `get_entity_state`, `get_entity_info`, `set_relay`, `set_dimmer` | `device_id`, `entity_id` |
| `get_rule`, `update_rule`, `delete_rule`, `enable_rule`, `disable_rule` | `rule_id` |
| `create_rule` | `name` (+ rule body) |

Arguments may be supplied as a native typed carrier or as a strict, bounded JSON
object (`qymera_llm_args_from_json`) for the flat fields (`device_id`, `entity_id`,
`name`, `rule_id`, `value`, `level`, `enabled`). No JSON-library dependency and
no natural-language parsing.

---

## Tool call lifecycle

```text
LLM response
 ↓
tool_call(name, structured arguments)
 ↓
adapter validates envelope   (name present, args present, args structurally valid)
 ↓
skill lookup                 (qymera_skill_lookup)
 ↓
permission context           (explicit mask, never grant-all)
 ↓
qymera_skill_execute()       (dispatch through the Skill layer)
 ↓
Skill result                 (unchanged {ok,data}/ {ok,error} envelope)
 ↓
adapter serializes tool result
 ↓
LLM
```

All rule mutations still pass through Skill validation → Rule Engine validation →
compile → persist → activate. The adapter adds **no** bypass and **weakens
nothing**.

---

## Permission propagation

The adapter must receive an explicit permission mask/context per call/turn. It
does **not** silently grant `READ | CONTROL | RULE_READ | RULE_WRITE` by default.
The mask is verified against each skill's required bit before dispatch.

```c
// read-only model
READ | RULE_READ
// automation-authorized agent
READ | CONTROL | RULE_READ | RULE_WRITE
```

Permission policy stays **outside** the individual Skill implementations; the
adapter is a pure propagation point.

---

## Tool budget & recursion protection

The tool→LLM→tool→LLM loop is bounded by `QYMERA_MAX_TOOL_CALLS_PER_TURN` (**8**).

Justification: the canonical multi-step workflow (discover → inspect → build rule
→ create → enable → act → observe) needs at most **6** sequential tool calls.
8 leaves headroom for discovery/retry while keeping the per-turn working set
(one provider message + one skill output buffer) small enough for ESP32 and
preventing pathological tool/agent loops. Exceeding the budget ends the turn
with `TOOL_CALL_LIMIT`.

The adapter has an explicit execution boundary: each `qymera_llm_adapter_process`
turn is capped, so unbounded tool/agent recursion is impossible.

---

## Error handling

- **Envelope failures** (unknown tool, missing/malformed arguments, permission,
  budget) are classified (`qymera_llm_tool_error_t`) and **never reach the
  runtime**. They are still serialized through the **stable Skill envelope**
  (`{ok:false, error:{code,message,details}}`) so the adapter does **not**
  introduce a second on-wire error taxonomy.
- **Skill failures** are returned unchanged (e.g. `ENTITY_NOT_FOUND`,
  `INVALID_CAPABILITY`, `RULE_INVALID`, `STORAGE_ERROR`, `OUTPUT_TOO_LARGE`).
- **Provider failure isolation:** a provider/LLM being unavailable does not
  affect the deterministic runtime. Existing rules, automation, local control,
  and remote control keep running — the adapter is **optional** and passive.

---

## Memory bounds

- No `malloc` per tool call (the adapter handle is allocated once at init).
- Bounded prompt/context buffer; no unbounded conversation history.
- No dynamic tool list (catalog is the fixed compile-time registry).
- No large/unbounded provider responses (single bounded message per step).

Object-measured footprint of `qymera_llm_adapter.c.o`:
- **RAM: 0 bytes static** (no static data; only a small heap handle allocated
  once — 2 pointers).
- **FLASH: ~3.4 KB** (text + rodata).

---

## Example workflow (no LLM required)

"Turn on the garden relay when temperature exceeds 30 °C" is the model's job; the
adapter only executes the model's structured tool calls. The canonical sequence
is runnable end-to-end with the compiled-in deterministic mock provider
(`qymera_llm_mock_provider_init`):

```text
list_entities
 → get_entity_state
 → create_rule
 → enable_rule
 → set_relay
 → observe state
```

Each step executes strictly through `qymera_skill_execute()`, and the resulting
state is returned to the model. No natural-language parsing happens in firmware.

---

## Deterministic system-prompt contract (documented for a future model)

A future model-facing prompt must reflect only the following deterministic facts
(mirrored from `docs/skills.md` and the hardened Skill API). **No** natural-
language "personality" prompt is hardcoded into firmware.

### Available skills
`list_devices`, `list_entities`, `get_entity_state`, `get_entity_info`,
`set_relay`, `set_dimmer`, `list_rules`, `get_rule`, `create_rule`,
`update_rule`, `delete_rule`, `enable_rule`, `disable_rule`.

### Tool schemas
Structured arguments only; required fields per skills as tabulated above;
rule mutation uses the native rule body (structured), never free-form text.

### Permissions
Read skills need `READ`; control need `CONTROL`; rule read `RULE_READ`; rule
write `RULE_WRITE`. Missing bit → `PERMISSION_DENIED`.

### Error model
Stable `{ok,data}` / `{ok:false,error:{code,message,details}}`. Codes never
change between releases: `SKILL_NOT_FOUND`, `PERMISSION_DENIED`,
`ENTITY_NOT_FOUND`, `INVALID_CAPABILITY`, `INVALID_VALUE`, `INVALID_INPUT`,
`RULE_INVALID`, `RULE_CONFLICT`, `STORAGE_ERROR`, `OUTPUT_TOO_LARGE`,
`DEPENDENCY_MISSING`, `NO_SPACE`, `DEVICE_OFFLINE`, `COMMAND_TIMEOUT`.

### State semantics
- `get_entity_state` distinguishes `observed`, `desired`, `status`, `reliability`,
  `timestamp`.
- **`ACKED != CONFIRMED`**: an ACKed command is dispatched/acknowledged but the
  physical device value may not yet reflect it. `CONFIRMED` is authoritative.
- **`desired != observed`**: `desired` is the requested set-point; `observed` is
  the last sampled physical value; they may differ transiently.
- `set_relay`/`set_dimmer` return requested plus status/reliability; repeat
  `get_entity_state` to observe convergence.

### Rule mutation behavior
- Rule creation/update/delete/enable/disable are **transactional/atomic**: on a
  storage failure the previous rule (and its revision) stays intact and
  `STORAGE_ERROR` is returned.
- Revision increments only on a fully successful update.
- Id collision → `RULE_CONFLICT`.

---

## Testing

`tests/host_sanity.py` carries a Python reference model of the adapter plus a
deterministic mock provider (no real LLM). It covers: tool catalog (13, no
duplicates, derived from registry), tool execution (valid / unknown / missing
args / permission denied), budgets (1 / N / N+1 → `TOOL_CALL_LIMIT`), provider
behavior (text / tool call / malformed / provider error / timeout), permission
propagation (READ / CONTROL / RULE_READ / RULE_WRITE), and the full 6-step
workflow.

```
python tests/host_sanity.py     # 226 checks (144 P3A + 53 P3B + 29 P3C)
pio run -e esp32_devkit          # SUCCESS, no new warnings
```

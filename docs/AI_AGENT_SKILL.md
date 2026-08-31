# Qymera Dashboard — AI Agent Skill/API Specification

> Part of the Qymera Dashboard architecture contract. Companion to [QYMERA_DASHBOARD_ARCHITECTURE.md](./QYMERA_DASHBOARD_ARCHITECTURE.md) and [AUTOMATION_ENGINE.md](./AUTOMATION_ENGINE.md).
> The Skill/API is a **first-class subsystem**: it is the single surface an AI agent (LLM tool-calling) — and by extension the web UI — uses to observe, program, and control Qymera Dashboard.

---

## 1. Design Position

In Qymera 1.1 (branch `feature/ai-experiments`) the "agent" surface is:

- four fixed **AI prompt slots** (`ai.cpp`) that ask the LLM for a value (digital/analog/analytic) or a fragile flat JSON "CONTROL" tool call which is string-parsed (`extractJsonString`, brace-count guard) before calling control primitives directly from `ai.cpp`;
- a stateless `/ai/chat` relay that injects a small PROGMEM tool schema (`list_entities`, `device_status`, `list_rules`, `read_logs`, `save_rule`, `delete_rule`, `toggle_actuator`, `set_dimmer`) into a browser-driven, **same-origin tool loop**.

The Dashboard replaces all of this with a **designed, versioned, validated Skill tool surface**. The agent no longer answers "what value", it performs *capability-scoped operations* through typed tools. Nothing executes GPIO; every control tool funnels through the single Control API.

---

## 2. Tool Surface — Complete List

### 2.1 Discovery
```
list_devices()                    → [DeviceSummary]
get_device(device_id)             → DeviceDetail
list_entities(filter)             → [EntitySummary]        # sensors+actuators
get_entity(entity_id)             → EntityDetail
get_capabilities(entity_id)       → [Capability]
list_inference_providers()        → [ProviderSummary]
```

### 2.2 State
```
get_state()                       → global state snapshot (bounded)
get_sensor(entity_id)             → EntityReading
get_multiple_states(entity_ids[]) → { entity_id: EntityReading }
get_online_devices()              → [device_id]
```

### 2.3 Control
```
set_relay(entity_id, on|off)
set_dimmer(entity_id, level 0-100)
set_value(entity_id, value)          # typed value respecting capability
set_entity(entity_id, value, opts?)  # generalized (schema-bounded)
pulse(entity_id, ms)
fade(entity_id, from, to, ms)
```
All control tools require confirmation by default at the *UI* level (see safety model §7). Control is idempotent: repeated identical `set` is a no-op returning current state.

### 2.4 Automations
```
list_rules(filter)      → [RuleSummary]
get_rule(rule_id)       → RuleDetail (+ last executions)
create_rule(rule_json)  → RuleReport   # validation + compile + activate
update_rule(rule_id, rule_json) → RuleReport
delete_rule(rule_id)    → Ack
enable_rule(rule_id) / disable_rule(rule_id) → Ack
test_rule(rule_id)      → DryRunReport  # simulated evaluation (no actuation)
```

### 2.5 Analytics
```
query_history(entity_id, range, resolution) → TimeSeries
aggregate(entity_id, window, ops[])         → { op: value }
average(entity_id, window) / min / max / count / rate / trend
time_in_state(entity_id, state, window)     → seconds
activation_count(actuator_entity, window)   → count
```

### 2.6 Events
```
get_recent_events(limit, kinds) → [Event]
query_events(filter)            → [Event]   # subject to ring retention
```

### 2.7 Diagnostics
```
get_logs(layer?, level?, limit)     → [LogEntry]
get_device_health(device_id?)       → Health
get_system_status()                 → uptime, heap, transport, rx metrics
get_context()                       → targeted AI context bundle (see §8)
get_ai_activity(limit)              → [AI Audit Entry]
```

The final tool *names* may vary, but **all of the above functionality must be covered** by the shipped Skill.

---

## 3. Transport & Framing

- Two binding modes:
  1. **Local HTTP(s) API** (device web server) — JSON over `application/json`, versioned under `/skill/v1/<tool>` mirroring today's REST style but standardized.
  2. **Same-origin tool loop** (browser agent) — the UI calls the same endpoints.
- Tool-call framing: a single request maps to one tool execution; responses are single JSON objects. Supports tool-call batch inside one HTTP request (`tool_choice`, array) bounded to N tools per request and a total payload budget.

---

## 4. Schemas

Every tool is defined by a published JSON Schema (property names, types, bounds, required). Key shared types:

```json
EntityRef  = { "device_id": "greenhouse-01", "entity_id": "temperature" }
Capability = "sensor.numeric" | "sensor.digital" | "actuator.relay"
           | "actuator.dimmer" | "actuator.generic" | "inference.result"
           | "time" | ...
EntityReading = { "entity_ref": EntityRef,
                  "value": number|bool,
                  "state": "on"|"off"|"open"|...,
                  "ts": epoch_sec,
                  "age_ms": int,
                  "reliability": "live"|"stale"|"offline" }
RuleReport  = { "ok": bool, "rule_id": string, "revision": int,
                "errors": [ {code,message,path} ],
                "warnings": [ ... ], "activated": bool }
```

The schema lives in a single PROGMEM/flash constant injected into the tool surface (pattern retained from `AI_CHAT_TOOLS_JSON`) and is also served at `/skill/v1/schema` for external agents.

---

## 5. Validation

- **Input validation** per tool schema (strict: no silent coercion; retained from `parseStrict*` philosophy in `web.cpp`).
- **Rule validation**: full pipeline in [AUTOMATION_ENGINE.md](./AUTOMATION_ENGINE.md) §3 (schema → references → semantics → conflicts → compile).
- Unknown tools / bad payloads → structured error (never a bare 500).
- Validation is executed on-device; the agent cannot bypass it.

---

## 6. Errors

Uniform error envelope:

```json
{ "error": {
    "code": "not_found" | "validation" | "permission" | "rate_limited"
         | "conflict" | "unavailable" | "inference_timeout" | "internal",
    "message": "...",
    "details": { "path": "...", "suggestions": [...] }
} }
```

HTTP mapping (local API): 400 validation, 401 auth, 403 permission, 404 not_found, 409 conflict, 429 rate_limited, 504 inference/provider timeout. The AI-prompt-slot approach of textual `last_error` is replaced by this structured model.

---

## 7. Permissions & Safety Model

| Concern | Policy |
|---|---|
| Authentication | Local API guarded (Basic/ bearer token), rate-limited (retained burst-tolerant limiter pattern). Default factory state: provisioning-open on LAN; dashboard UI warns. |
| Role separation | Read tools = `observer`; control = `operator`; rule create/update/delete = `programmer`; config/security = `admin`. Enforcement point is the Skill layer. |
| Control safety | Every control call: capability check → bounds check → **target-safety check** (entity must be an actuator with declared capability). No GPIO from the skill path directly — only via `Control API`. |
| Confirmation | Control and rule-activation require UI confirmation by default; agent calls may carry `confirm:true` opt-in when the agent was explicitly told the operation is confirmed by the user's message. |
| Idempotency | Control tools idempotent (set same → no-op, returns current). Create is idempotent on `rule_id` (rejects duplicate with conflict unless `overwrite:true`). |
| Rate limits | All mutating tools share the burst-limiter; inference provider has its own budget. |
| Human oversight | `get_ai_activity()` is the audit ledger; `AI`/`AUTOMATION` log layers record every agent action. |
| Safety boundaries | A rule/action may never target an actuator the registry classifies as `protected` without `policy.allow_protected`. |

---

## 8. AI Context Engine

Principle: **targeted retrieval, never a system dump.** The old `buildBody()` built one ad-hoc sensor text macro; the Actor Context Engine replaces it.

- The `get_context()` tool returns a **queryable bundle**: the agent first resolves what it needs via narrow tools (devices → state → history → analytics) and builds its own context, or asks for a prebuilt slice:
  ```
  get_context(slice="sensors.warming"|"devices.all_summary"|"rules.active",
              hints=["temperature","greenhouse-01"], budget_bytes)
  ```
- Every slice has a hard byte budget (e.g. ≤ 2 KB per bundle); content is projected/rounded/downsampled before serialization.
- History slices are downsampled (`resolution`) in the telemetry layer, never raw-full.
- Caching: entity-aware LRU; invalidation on `entity.changed` events so repeated agent turns are cheap.
- The context engine is a *consumer of the event bus*, exactly like the rule engine, analytics, and logger.

---

## 9. Rule Creation Workflow (agent side)

```
1. User: "Turn the extractor on when temp > 30°C for 5 min, not 23:00-07:00."
2. Agent calls  list_devices() → sees greenhouse-01/temperature, /extractor
3. Agent calls  get_entity()/get_capabilities() → numeric sensor, relay actuator
4. Agent calls  create_rule({ ...canonical JSON... })
5. Skill → validation (schema/refs/semantics/conflicts)
   → maybe returns errors → agent repairs and retries (bounded retries)
6. Skill → compile → persist → activate
7. Returns RuleReport { ok:true, rule_id, revision, activated:true }
8. Agent summarizes to user. All steps in AI audit log.
```

The agent must **verify referenced entities via discovery** before authoring; the Skill enforces the same at validation time.

---

## 10. Versioning

- Skill surface versioned: `/skill/v1/*`; additive changes bump minor; breaking changes bump major and the old version is deprecated with a migration map.
- Every response carries `{"api_version":"v1","schema_version":"qymera.rule.v1"}` where relevant.
- Rules version/revision as defined in the Automation Engine document.
- Tool schema object is versioned inside its own flash constant.

---

## 11. Response Format

- Non-streaming tools → single JSON object/array as above.
- `query_history`/`aggregate` → typed series with `resolution`, `start`, `end`, `points[]` (bounded length, dropped/rounding policy documented).
- `get_context` → `{ "slices": [...], "bytes": n, "truncated": bool }`.
- `test_rule` → dry-run execution trace (`steps[]`) with no actuation.
- Big outputs stream (chunked) at the HTTP layer, mirroring today's streamed `/calib`.

---

## 12. External Inference Abstraction (Skill-adjacent)

The Skill exposes inference *settings* and *results*, while the provider abstraction lives in the AI subsystem:

- Provider modes: `NO AI` | `LOCAL AI` (on-LAN OpenAI-compatible) | `REMOTE AI` (cloud) | `HYBRID`.
- Provider contract: request/response JSON over the configured endpoint; typed result schemas; **timeout** (bounded, provider-configurable, UI never frozen — the old "ESP8266 20 s cap / 504" lesson generalizes to a per-request budget and non-blocking dispatch); **failure decomposition** (timeout/unreachable/schema mismatch), **caching** (`cache_ms`), **rate limiting** (per provider budget), **security** (API key stored write-only, never echoed — pattern retained), **deterministic fallback** (per-condition `fallback` policy).
- Inference is optional by construction: `NO AI` is the factory default and the deterministic core is byte-identical with the provider absent.

---

## 13. Relationship to the Existing HTTP API

The current ad-hoc endpoints (`/calib`, `/rules/set`, `/toggle`, `/dimmer`, `/ai/*`) coalesce into the versioned Skill surface. The web Dashboard becomes a thin consumer of the Skill; legacy URLs are removed or mapped during a migration window (see [MIGRATION_PLAN.md](./MIGRATION_PLAN.md)).

---

## 14. Open Items

1. Exact role model granularity for `operator`/`programmer` on a single-user device.
2. Whether `test_rule` dry-run should allow near-miss constraints (e.g. force a state) for better agent feedback.
3. Streaming vs. request/response for `query_history` over large windows (recommend bounded request/response with explicit pagination/range).
# Qymera Deterministic Skill API (Phase 3A)

The single, machine-readable surface an external agent (a future LLM adapter,
web UI, or automation) uses to observe and control Qymera. The Skill layer is
**deterministic**: it only validates structured input and calls the existing
deterministic APIs — the Registry, the Rule Engine, and the Control API. It never
touches GPIO, UDP, or internal structures directly, and it never parses natural
language nor connects to any inference provider.

The layer is **caller-agnostic**: Ollama, OpenAI, a local model, a remote model,
a human UI, and an automation all look identical here.

Implementation: `src/ai/qymera_skill.c` / `src/ai/qymera_skill.h`. Wired into the
runtime in `src/core/qymera_core.c` via `qymera_core_get_skills()`.

---

## Skill model

Every call returns the same bounded result shape:

```json
{ "ok": true,  "data": { ... bounded JSON fragment ... } }
{ "ok": false, "error": { "code": "MACHINE_STABLE_CODE", "message": "human detail", "details": {...} } }
```

- `data` is capped at `QYMERA_SKILL_OUTPUT_SIZE` (1024 bytes); `truncated` is set
  in the output model when the cap is hit.
- Error `code`s are machine-stable and never change between releases.
- No memory is allocated per invocation and no strings are unbounded: the
  registry is a compile-time `const` table of exactly `QYMERA_MAX_SKILLS` (13)
  skills. There is no dynamic tool registry and no scripting engine.

### Permissions (authorization boundary)

Skills are gated by an authorization boundary — **not** a security/authentication
system. A caller is granted a bitmask; each skill requires one bit.

| Bit    | Value | Required for |
|--------|-------|--------------|
| `READ` | 1     | `list_devices`, `list_entities`, `get_entity_state`, `get_entity_info` |
| `CONTROL` | 2 | `set_relay`, `set_dimmer` |
| `RULE_READ` | 4 | `list_rules`, `get_rule` |
| `RULE_WRITE` | 8 | `create_rule`, `update_rule`, `delete_rule`, `enable_rule`, `disable_rule` |

If the required bit is missing: `PERMISSION_DENIED`.

---

## Skills

| # | Name | Perm | Purpose |
|---|------|------|---------|
| 1 | `list_devices` | READ | Enumerate registered devices |
| 2 | `list_entities` | READ | Enumerate all entities across devices |
| 3 | `get_entity_state` | READ | Observed/desired value, cmd status, reliability |
| 4 | `get_entity_info` | READ | Static entity metadata (name, type, capabilities) |
| 5 | `set_relay` | CONTROL | Set a relay actuator on/off |
| 6 | `set_dimmer` | CONTROL | Set a dimmer level 0-100 |
| 7 | `list_rules` | RULE_READ | Enumerate loaded rules |
| 8 | `get_rule` | RULE_READ | Full rule detail incl. trigger/conditions/actions |
| 9 | `create_rule` | RULE_WRITE | Compile, load, persist, and activate a rule |
| 10 | `update_rule` | RULE_WRITE | Replace rule body, bump revision, repersist |
| 11 | `delete_rule` | RULE_WRITE | Unload and delete a rule |
| 12 | `enable_rule` | RULE_WRITE | Re-enable a rule |
| 13 | `disable_rule` | RULE_WRITE | Disable a rule |

### Structured input

```c
typedef struct {
    char device_id[QYMERA_DEVICE_ID_LEN];
    char entity_id[QYMERA_ENTITY_ID_LEN];
    char name[64];
    char rule_id[QYMERA_RULE_ID_LEN];
    bool value;          /* set_relay */
    uint8_t level;       /* set_dimmer (0-100) */
    qymera_rule_t rule;  /* create_rule / update_rule (structured) */
    bool enabled;        /* enable_rule / disable_rule */
} qymera_skill_input_t;
```

---

## Skill behavior & output shapes

### `list_devices`
```json
[ {"device_id":"...","name":"...","model":"...","role":"...",
   "online":true,"state":"...","location":"..."}, ... ]
```

### `list_entities`
```json
[ {"device_id":"...","entity_id":"...","name":"...","type":"...",
   "capabilities":["..."],"unit":"...","current":<v>,"desired":<v>,
   "cmd_status":"..."}, ... ]
```
`current`/`desired` are booleans for relays, numerics otherwise.

### `get_entity_state`
```json
{"device_id":"...","entity_id":"...","observed":<v>,"desired":<v>,
 "status":"...","reliability":"...","timestamp":<secs>}
```
Errors: `ENTITY_NOT_FOUND` (unknown `device_id`/`entity_id`).

### `get_entity_info`
```json
{"device_id":"...","entity_id":"...","name":"...","type":"...",
 "capabilities":["..."],"unit":"..."}
```

### `set_relay`
```json
{"device_id":"...","entity_id":"...","requested":true,"status":"WAITING_ACK","reliability":"PENDING"}
```
Validation: entity must exist (`ENTITY_NOT_FOUND`) and have `actuator.relay`
(`INVALID_CAPABILITY`). Dispatches through the Control API (always with
`local_only=false`). Errors from the transport: `DEVICE_OFFLINE`,
`COMMAND_TIMEOUT`, `NO_SPACE` (pending command table full).

### `set_dimmer`
```json
{"device_id":"...","entity_id":"...","requested":50,"status":"WAITING_ACK","reliability":"PENDING"}
```
Validation: `level` must be 0-100 (`INVALID_VALUE`); entity must exist
(`ENTITY_NOT_FOUND`) and have `actuator.dimmer` (`INVALID_CAPABILITY`).

### `list_rules`
```json
[ {"rule_id":"...","name":"...","enabled":true,"revision":1}, ... ]
```

### `get_rule`
```json
{"rule_id":"...","name":"...","enabled":true,"revision":1,"priority":0,
 "cooldown_ms":0,"max_activations_per_hour":0,"created_ts":0,"updated_ts":0,
 "state":{"activation_count":0,"last_triggered":0},
 "trigger":[{...}],"conditions":[...],"actions":[...]}
```

### `create_rule`
```json
{"rule_id":"rule_1","revision":1,"enabled":true,"activated":true,"slot":0}
```
Flow: validate references → `qymera_rule_engine_validate` → `compile` → `load` →
`persist` (storage). If `rule_id` is not supplied, one is generated (`rule_%u`).
Errors: `INVALID_VALUE` (no name), `ENTITY_NOT_FOUND` (unknown entity ref),
`RULE_INVALID` (bad operator / type / incompatible capability / validation /
compile / persist failure), `NO_SPACE` (rule table full).

### `update_rule`
```json
{"rule_id":"...","revision":2,"enabled":true,"updated":true}
```
Reuses the create pipeline then unloads the old slot and loads the new body.
Errors: `INVALID_VALUE` (no `rule_id` / no name), `RULE_INVALID` (not found /
validation / compile / persist), `ENTITY_NOT_FOUND`, `NO_SPACE`.

### `delete_rule`
```json
{"rule_id":"...","deleted":true}
```

### `enable_rule` / `disable_rule`
```json
{"rule_id":"...","enabled":true}
```

---

## Error code catalog

| Code | Meaning |
|------|---------|
| `SKILL_NOT_FOUND` | Unknown/unregistered skill name |
| `PERMISSION_DENIED` | Caller lacks the required permission bit |
| `ENTITY_NOT_FOUND` | `device_id`/`entity_id` does not resolve |
| `INVALID_CAPABILITY` | Target entity lacks the required capability (relay/dimmer) |
| `INVALID_VALUE` | A field is malformed or out of range (e.g. dimmer level, missing name) |
| `RULE_INVALID` | Rule not found, or failed reference/validation/compile/persist |
| `NO_SPACE` | Pending command table or rule table full |
| `DEVICE_OFFLINE` | Command transport unavailable for a remote dispatch |
| `COMMAND_TIMEOUT` | A dispatched command timed out |

---

## Command status / reliability strings

- `cmd_status` maps to the Control API state machine:
  `STATE_CONFIRMED`, `ACKED`, `WAITING_ACK`, `DISPATCHED`, `REQUESTED`,
  `FAILED`, `TIMEOUT`, and others as defined by `qymera_skill_cmd_status_str`.
- `reliability` is a derived, stable classification:
  `CONFIRMED` / `PENDING` / `FAILED` / `STALE`, from
  `qymera_skill_reliability_str`.

---

## Testing (LLM-independent)

The Skill layer is verified without any model. `tests/host_sanity.py` carries a
deterministic Python reference model (`SkillEnv`) that mirrors `qymera_skill.c`
and covers 16 categories plus a complete future-AI workflow
(`list_entities` → `get_entity_state` → `create_rule` → `enable_rule` →
`set_relay` → observe state), with no LLM involved:

```
python tests/host_sanity.py
```

The ESP32 build is independent of inference infrastructure:

```
pio run -e esp32_devkit
```

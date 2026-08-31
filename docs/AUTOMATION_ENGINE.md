# Qymera Dashboard — Automation Engine Specification

> Part of the Qymera Dashboard architecture contract. Companion to [QYMERA_DASHBOARD_ARCHITECTURE.md](./QYMERA_DASHBOARD_ARCHITECTURE.md).
> The rule engine is best understood as an **Automation Runtime / Automation VM**: AI-generated rules are compiled into an efficient internal representation and executed deterministically, locally, and — in normal operation — entirely without an LLM.

---

## 1. Design Position

The current Qymera 1.1 rule engine (`automations.cpp/h`) is a **fixed-size, index-addressed, poll-everything** engine:

- `Rule rules[MAX_RULES]` with `MAX_RULES = 20`, in RAM, mirrored to a 1600-byte EEPROM block.
- Rules address sensors/actuators by **calibration table index** (`sensor_idxs[5]`), not by stable ID.
- Only 4 rule types: EDGE, THRESHOLD, TIME, INTERVAL. ≤5 inputs, ≤5 outputs.
- Every 50 ms the whole table is re-evaluated (`SAMPLE_MS`), scanning every non-empty rule, computing comparisons inline.
- Logic is flat: one `logical_and` flag over one condition vector; no nesting, no NOT, no hysteresis, no state history, no time-window composition, no inference.
- The AI layer ("AI prompt slots") writes into *virtual sensors* (`aidig`/`aiana`) and the existing engine consumes them — i.e., AI is currently wired in as faked sensor inputs and a fragile JSON "CONTROL" parse that calls control primitives directly from `ai.cpp`.

**Limitations to replace (explicit):**

| # | Current limitation | Consequence |
|---|---|---|
| L1 | 20-rule fixed array | cannot hold hundreds of AI-authored rules |
| L2 | index-addressed entities | rule survival is coupled to slot reuse/lifetime hacks (`isIndexReferenced`, stale reclaim); renaming/reordering breaks rules |
| L3 | poll-every-rule every 50 ms | O(rules) per tick; worse with hundreds of rules |
| L4 | flat AND/OR + 1 condition vector | no NOT, no nested groups, no historical/trend conditions |
| L5 | no sustained-condition window (the "for 5 minutes" case) | duration conditions cannot be expressed |
| L6 | delay only; no per-condition timers, cooldown per rule only | no robust timer semantics |
| L7 | no inference conditions, no fallback policy | AI is input-only, cannot be a condition with defined degradation |
| L8 | rules not versioned | no audit of AI-authored changes |

The Dashboard engine addresses all of the above.

---

## 2. Rule Model

### 2.1 Rule object (authoring schema)

Versioned JSON schema accepted by the Skill/API `create_rule`/`update_rule`. Example:

```json
{
  "schema": "qymera.rule.v1",
  "id": "rule-cooling-001",
  "name": "Cooling",
  "enabled": true,
  "trigger": {
    "type": "threshold",
    "entity": "greenhouse-01/temperature",
    "operator": ">",
    "value": 30,
    "duration": 300
  },
  "conditions": {
    "all": [
      { "type": "time_window", "start": "07:00", "end": "23:00" },
      {
        "not": {
          "type": "device_state",
          "entity": "maintenance/mode",
          "equals": true
        }
      }
    ]
  },
  "actions": [
    { "type": "set", "entity": "greenhouse-01/extractor", "value": true }
  ],
  "policy": {
    "cooldown_ms": 60000,
    "max_activations_per_hour": 10
  }
}
```

The schema is expressive enough to cover all required constructs (see §4 and §5).

### 2.2 Authoring form versus runtime form

| Layer | Form | Producer → Consumer |
|---|---|---|
| Authoring | JSON (`qymera.rule.v1`), human/agent friendly | Agent/UI → Skill/API |
| Normalized | canonical JSON (ordered, defaults materialized, references resolved) | validation/normalizer output |
| Compiled | internal **IR/bytecode** (`RuleImage`) | compiler → VM |
| Persisted | versioned canonical JSON + compiled image checksum | storage |

The **canonical JSON** is the durable, auditable artifact; the **compiled image** is the hot execution form reconstructed at boot or loaded from a flash image.

---

## 3. Validation

Pipeline stages, applied in order. Any failure returns a structured error report to the agent (not a generic 400).

1. **Schema validation** — structural (types, required fields, bounds, enums); done against the pinned JSON schema.
2. **Reference resolution** — every `entity` in trigger/conditions/actions must resolve in the Device Registry to a *known entity* with the *required capability* (e.g. `temperature` is a numeric sensor; `extractor` is a relay actuator). Unknown/mismatched capability → error with candidate suggestions.
3. **Semantic validation**:
   - malformed logic (e.g., empty `all`/`any`, contradictory `not` wrapping, nonsense ranges);
   - type/operator mismatch (comparing a boolean entity with `>`);
   - range sanity (values inside `min/max` where registry declares them);
   - time-window shape (`start < end`, valid clock, wrap allowed with explicit `wrap:true`);
   - schedule parsing (cron-style subset) validity.
4. **Conflict detection** — obvious conflicts against *active* rules: same trigger signature + same actuation target with opposite `set` polarity; same target with mutually exclusive `duration` windows. This is a *warning* (agent informed, request proceeds) unless `strict:true` in the policy.
5. **Compile gate** — only rules that pass 1–4 reach the compiler.

Validation is done by the **Skill layer + Compiler**, not by the agent. The agent never decides validity.

---

## 4. Compiler / Normalizer

Pipeline: canonical JSON → AST → optimized IR (RuleImage).

### 4.1 AST → IR normalization

- References replaced by **EntityRef** (`device_id + entity_id + capability`), stable across boots (IDs, not indices).
- Conditions flattened into a **condition graph**:
  ```
  AND(node[])
  ├─ TriggerNode(threshold, src, op, value, persist_ms)
  ├─ TimeWindowNode(start_tod, end_tod, wrap, days[] optional)
  ├─ DeviceStateNode(src, equals)
  └─ NOT( InRangeNode(humidity, [70,100]) )
  ```
- Constants folded; operator enums encoded; strings moved to entity tables (IR itself carries no longer-lived strings except rule name/notes which live in the canonical JSON, not hot IR).
- `duration`/`persist` conditions lower to **stateful window nodes** (see §5.3) with `emit_on: entry | exit | both`.
- Inference conditions lower to **provider callbacks** with a declared fallback policy.

### 4.2 Compile-time checks

- IR size ≤ per-rule budget (fails rule, returns report).
- Max inputs/outputs enforced (design: up to 8 input refs, up to 8 actions per rule; configurable constant).
- No cycles possible by construction (conditions are acyclic DAG over entity refs).

### 4.3 Output

```
RuleImage {
  header: {id, revision, enabled, created/updated_epoch, policy_flags}
  packed_conditions: bytecode blob
  packed_actions: bytecode blob
  input_entity_refs[]: EntityRef    (index used for subscriber index)
  output_entity_refs[]: EntityRef
  consts: numbers/strings table
  state_header: size of per-instance state block
  checksum
}
```

Per-rule instance state (the `RuleState` evolution of today's `automations.cpp`) is a fixed-size block derived from `state_header`: evaluation cursors, window persistence counters, last-action, cooldown expiry, pending-action slot, activation count, revision marker.

---

## 5. Conditions

### 5.1 Condition taxonomy

| Category | Constructs |
|---|---|
| Numeric | threshold (`>`, `<`, `>=`, `<=`, `==` with epsilon), within-range, outside-range |
| Hysteresis | `threshold` with `hysteresis` band: fire above `hi`, clear below `lo` (stateful) |
| Time | time-window (`HH:MM` – `HH:MM`, optional wrap), date range (replaces `year/month/day` fields), schedule (cron-subset: weekday + slot) |
| Duration | sustained condition "for ≥ N seconds/minutes" → window state machine with `emit_on` |
| Device/sensor state | `device_state`, `entity_online`, `entity_value`, `entity_state` |
| Event trigger | `event` (e.g. `event.type == "sensor.changed"` and entity filter), with optional count/rate |
| Sequencing | sequences: `sequence` of steps with per-step delay, or chained rules via `rule_completed` event |
| Cooldown/rate | per-rule cooldown, `max_activations_per_hour`, global per-target rate limit |
| State transition | `transition` on an entity: rising/falling/changed, with anti-bounce confirmation (retains today's `CONFIRM_READS` concept) |
| Historical | `history`/`trend` (see §8) |
| Aggregate | `aggregate` over telemetry window (avg/min/max/count) vs operator |
| Inference | `inference` — optional (see §9) |

### 5.2 Boolean composition

- `all` = AND, `any` = OR, `not` = NOT.
- Arbitrary nesting depth bounded by IR budget (e.g. ≤ 8 condition nodes + 4 groups per rule).
- Short-circuit evaluation; event-driven wake-up per subscribed node.

### 5.3 Duration / sustained-window state machine

A node like `temperature > 30 for 5 minutes` compiles to a window state machine:

```
IDLE --(cond_true, start persist)--> COUNTING
COUNTING --(persist_ms reached)--> ARMED(entry)      → emits trigger.enter
COUNTING --(cond_false)--> RESET
ARMED --(cond_false)--> COOLDOWN-exit (if emit_on:exit) → emits trigger.exit
```

This is exactly the semantics needed for the product example ("exceeds 30°C for five minutes").

### 5.4 Anti-bounce / debounce

Digital inputs use confirmed-read debounce (retains current `CONFIRM_READS` behavior but as a condition primitive, not inline loop code).

---

## 6. Triggers

- A rule has **one trigger** (the "when/if" head) plus **conditions** (the "and/or/unless" tail). Semantically a trigger is the *entry condition of interest*; the VM subscribes the rule to the trigger's source entity.
- Trigger types map to the taxonomy: threshold(+duration), transition/edge, event, time/schedule, interval, device-state.
- `UNLESS` clauses are expressed as `not{...}` under `conditions`; `WHEN ... AND ...` maps to a group.
- The example in the brief:

```
WHEN temperature > 30 AND humidity > 70
  AND temperature has remained above 30 for 5 minutes
  AND time between 08:00 and 22:00
UNLESS mode == maintenance
THEN extractor = ON
```

compiles as:

```
TRIGGER: within([temp>30 AND humi>70], persist=300s, emit_on=entry)
CONDITIONS: AND[
  time_window(08:00,22:00),
  NOT( device_state(maintenance/mode, ==true) )
]
ACTIONS: set(extractor, true)
```

Once armed, execution needs no LLM.

---

## 7. Runtime (Automation VM)

### 7.1 Architecture

```
Event Bus ──► Rule Scheduler (subscriber index: entity → rules)
                │
                ▼
            Rule Runner (one per triggered rule)
                │  iterates packed_conditions (short-circuit)
                ▼
            Action Executor -> Control API (single path) -> events/logs
```

- **Event-driven**: the VM subscribes to entity change/event topics. A sensor event touches only rules subscribed to that entity. No global rescan.
- **Timer wheel**: cooldowns, delays, sustained windows, schedules, interval triggers, and per-target rate limits are timer-wheel entries with O(1) amortized dispatch. No per-rule polling.
- **Tick budget**: the VM contributes to the core tick budget (bounded number of rule runs per tick; rounds continue next tick), so a rule-heavy load cannot starve networking/UI.
- **State isolation**: per-rule instance state; a rule is a pure function of (compiled image, incoming events, instance state, wall clock). Deterministic given the same inputs.
- **Ordering**: within a tick, rules run in stable priority order (`priority`, default 0; deterministic tiebreak by id). Actions are serialized and idempotent where possible.

### 7.2 Evaluation cycle per rule

1. Wake via subscriber match or timer.
2. Evaluate **conditions** short-circuit; if trigger+gated conditions satisfied → check cooldown/rate limits.
3. If `delay_ms > 0`: schedule pending-action timer; else execute actions now.
4. Execute actions through the Control API; record `rule.triggered`, per-action `actuator.command`, activation counters, timestamps.
5. Update cooldown/rate state; persist only mutations that are durable-state-marked (cooldowns across reboot are optional per rule policy).

### 7.3 State transitions

| State | Meaning |
|---|---|
| `DISABLED` | loaded, not evaluated |
| `WAITING` | armed, waiting for trigger |
| `WINDOW` | sustained-window counting |
| `PENDING_DELAY` | delay timer running before actions |
| `EXECUTING` | action dispatch in progress (atomic, non-blocking) |
| `COOLDOWN` | post-action suppression |
| `ERRORED` | compile/eval exception; retried on next dispatch or disabled by policy |

Reentrancy: a rule never runs twice concurrently; re-trigger while `PENDING_DELAY`/`EXECUTING` is dropped or coalesced per per-rule `rearm` policy (`drop | coalesce | queue1`).

---

## 8. Historical Conditions

Historical conditions read the telemetry ring via the Analytics/telemetry interface:

- `history(entity=temp, range=6h, resolution=5m)` → downsampled series.
- `trend(entity, window)` → slope sign / magnitude over a window.
- `aggregate(entity, window, op)` → avg/min/max/count/rate over window.
- `time_in_state(entity, state, window)` → seconds in state.

Evaluation strategy: these are expensive; they run at a **bounded cadence** (not on every sample). A `min_interval` guards cache + recomputation so trend conditions don't recompute per event. Computed values are pinned timestamps; rules consuming them behave like numeric conditions with a defined staleness (`max_age_ms`), falling back per policy (see §9).

---

## 9. Inference Conditions (optional)

- Declared as `{ "type":"inference", "provider":"default", "request": {...}, "fallback":"fail_closed|fail_open|last_known", "timeout_ms":2000, "cache_ms":30000 }`.
- At runtime the VM emits `inference.request` on the event bus; the inference service responses with `inference.result` (validated against the declared response schema, e.g. boolean).
- **Failure behavior**: timeout, provider error, schema mismatch, or stale cache → condition resolves per `fallback`:
  - `fail_open` → treat as true,
  - `fail_closed` → treat as false,
  - `last_known` → use last accepted result with expiry `max_staleness_ms`; beyond that, fail closed.
- **Cache + rate limit**: provider calls are cached for `cache_ms` per key and rate-limit-budgeted (bounded queue; excess requests are refused with a counter, never queue forever).
- **Deterministic core**: no inference condition → no provider interaction whatsoever. A rule with only deterministic conditions is byte-identical in behavior with every provider offline.

---

## 10. Actions

| Action | Semantics |
|---|---|
| `set(entity, bool|level)` | relay/dimmer/generic set via Control API |
| `set_entity(entity,value)` | generic typed value |
| `toggle(entity)` | state flip via Control API |
| `pulse(entity, ms)` | pulse actuation (retains current `pulse_ms` support) |
| `fade(entity, from,to,ms)` | dimmer fade via control path |
| `send_command(device, entity, value)` | remote device command via UDP control path |
| `record()` | write a structured log / event (audit-only action) |
| `notify()` | emit an event for the UI / agent (no GPIO) |
| `run_sequence(steps)` | chained actions with per-step delays (compiled to timers) |

Every action goes through **Control API validation** (permissions, capability, bounds) exactly as an agent/UI call would. Actions are serialized; an action failure is logged and (optionally) aborts the rest of the rule per policy.

---

## 11. Lifecycle

- `create` → validated → compiled → `ACTIVE`.
- `update` → new revision; old revision kept in history log (audit); atomic swap.
- `disable/enable` → state flip, persisted.
- `delete` → archive record in log, remove image.
- Boot: load canonical rules → validate persisted checksums → compile → hot load → activate. Compile failures autoskip with a structured error entry in the log ring.
- Versioning: `rule.version` (schema), `rule.revision` (content). Execution logs reference `(id, revision)`.

---

## 12. Persistence

- Canonical rule JSON + compiled `RuleImage` stored in the rules store (NVS-backed with flash paging on capacity — see [STORAGE_AND_MEMORY.md](./STORAGE_AND_MEMORY.md) §4).
- Fixed capacity policy: when the store is full, `create` fails with a structured error (admins purge/disable first). No silent eviction of active rules.
- Atomic, checksummed writes; transactionality via NVS commit; a torn write rolls back to last-good revision.
- Runtime instance state is RAM-only unless a rule opts into durable timers (rare), in which case only the bearer fields persist.

---

## 13. Error Handling

| Error class | Handling |
|---|---|
| Schema/semantic/conflict (authoring) | Returned in structured `RuleReport` to the agent (`errors[]`, `warnings[]`, `suggestions[]`) |
| Compile failure | Rule rejected; last-good revision retained |
| Eval exception (never expected — defensive) | rule → `ERRORED`, logged, watchdog counts; auto-retry or disable per policy |
| Action failure | logged `action.failed`; per-policy abort-continue |
| Inference failure | handled by fallback policy (§9) |
| Persistence failure | `create/update` fails atomically with report; runtime keeps RAM image until success |

---

## 14. Capacity & Performance Targets

| Metric | Target |
|---|---|
| Active rules | 500 (configurable constant; admission-controlled) |
| Rule inputs/outputs | ≤ 8 / ≤ 8 |
| Condition nodes per rule | ≤ 8 + 4 groups |
| Per-rule instance state | ≤ 64 B boxed (fixed) |
| Sustained window granularity | 1 s; timer wheel resolution 10–100 ms |
| Event-driven dispatch | O(affected rules) per event |
| Max activations/hour per rule | configurable policy |

Referenced budgets and worst-cases added in [STORAGE_AND_MEMORY.md](./STORAGE_AND_MEMORY.md).

---

## 15. Relationship with the current AI slot engine

The old model ("LLM returns true/false → virtual sensor `AIDIG` → existing threshold rule") is *replaced*: the Dashboard model makes the agent a **rule author**, and inference a **condition source** with defined fallback. The old `CONTROL` JSON parse in `ai.cpp` (string-scanning a tool call) is replaced by the structured Skill/API tool surface. No part of the OLD pipeline (prompt slots, `out_type`, `staged_prompt`, `extractJsonString`) survives in the Dashboard architecture.
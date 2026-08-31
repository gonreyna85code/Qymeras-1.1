# Qymera Dashboard — Data Model

> Part of the Qymera Dashboard architecture contract. Defines the canonical structured types shared by every subsystem (registry, event bus, telemetry, analytics, logs, rules, Skill/API, protocol).
> It deliberately replaces Qymera 1.1's flat, index-addressed `Calibration calibrations[MAX_SENSORS]` model with named, stable, capability-typed entities.

---

## 1. Devices

A **device** is a physical (or virtual) node reachable by Qymera Dashboard:

```json
Device {
  "device_id": "greenhouse-01",          // stable, user/tool-assignable; unique
  "net_id": { "kind": "udp", "addr": "192.168.1.42", "epoch": 1788050000 },
  "identity": { "chip_uid": 1234567890, "model": "ESP32", "fw": "qymera-dash-1.0.0" },
  "role": "dashboard" | "remote" | "provisioning",
  "registered_at": 1788050000,
  "last_seen": 1788050000,
  "online": true,
  "state": "operational" | "offline" | "degraded" | "provisioning",
  "capabilities": [ "sensor.numeric", "actuator.relay" ],
  "entities": ["temperature","humidity","extractor"],
  "metadata": { "location": "greenhouse", "tags": [] }
}
```

Rules:

- `device_id` is the primary key; stable across reboots and slot use. Entity resolution is by `(device_id, entity_id)` — removing today's index-based addressing.
- `net_id` is the UDP addressing record ([NETWORK_PROTOCOL.md](./NETWORK_PROTOCOL.md) §7). Identity (`chip_uid`) is device-provisioned and immutable; `net_id` may change with DHCP.
- A Dashboard is itself a device in the registry (`role:"dashboard"`), so rules and UI treat local and remote uniformly.
- Remote devices come and go; the registry keeps **provisioned identity + last_seen/online** independent of RAM slot pressure (capacity-bound; see Storage doc).

## 2. Entities

An **entity** is one addressable point on a device (sensor, actuator, virtual/AI state, virtual/inference result):

```json
Entity {
  "device_id": "greenhouse-01",
  "entity_id": "temperature",
  "name": "Temperature",
  "kind": "sensor" | "actuator" | "virtual" | "inference" | "time",
  "type": "temperature" | "humidity" | "luminosity" | "pressure" | "level"
        | "airq" | "rain" | "contact" | "relay" | "dimmer" | "generic"
        | "digital_ai" | "analog_ai" | "time" | ...,
  "capabilities": ["sensor.numeric"] ,          // see §3
  "unit": "°C" | "%" | "lx" | "kPa" | "bool" | "0-100" | "s" | ...,
  "native_range": { "min": -50, "max": 150 },   // physical, pre-calibration
  "calibration": { "correction": 0, "min": 0, "max": 100 },  // applied model
  "value": { "numeric": 31.4 } ,                // most recent reading
  "state": "on" | "off" | "open" | "closed" | "active" | "inactive",
  "reliability": "live" | "stale" | "offline" | "unknown",
  "last_update": 1788050000,
  "attrs": { "pulse_ms": 250, "fade_ms": 500, "protected": false }
}
```

- `type` set is an extension of today's 14 `SensorType`s plus `inference`.
- `calibration` replaces the `Calibration` struct semantics (min/max/correction/persist/pulse/fade) — kept, but owned by the registry, not by a flat sensor slot.
- Actuation attrs (`pulse_ms`, `fade_ms`, `protected`, `persist`) move into `attrs`.

## 3. Capabilities

A **capability** is the machine-readable contract used by validation, the Skill/API, Matter bridging, and agent discovery:

```
naming pattern: "<kind>.<aspect>"
sensor.numeric        → numeric readable, unit, range
sensor.digital        → boolean readable
actuator.relay        → set true/false, optional pulse
actuator.dimmer       → set level 0-100, optional fade
actuator.generic      → typed value set
virtual/value         → computed/local value
inference.result      → typed requestable inference (optional provider)
time.source           → wall clock source
```

Rules reference entities by **capability**, so a rule "set_extractor" works on any entity exposing `actuator.relay`. Validation refuses to bind a rule action to an entity lacking the capability.

---

## 4. Events

Canonical event schema (fits one UDP sample or one local sample or one system event):

```json
Event {
  "seq": 10042,                       // global monotonically increasing (post-boot)
  "ts": 1788050000,                   // epoch seconds (wall clock)
  "ts_ms": 1788050000123,             // optional sub-second
  "type": "sample" | "entity.changed" | "entity.command" | "actuator.changed"
        | "rule.triggered" | "rule.action" | "rule.lifecycle"
        | "schedule.alarm" | "device.online" | "device.offline"
        | "device.discovered" | "inference.request" | "inference.result"
        | "inference.failure" | "log.ingest" | "system.health" | "user.action",
  "device_id": "greenhouse-01",
  "entity_id": "temperature",
  "payload": { "value": 31.4, "prev": 30.9, "reliability": "live" },
  "source": "udp" | "mcu" | "rule" | "skill" | "ui" | "system",
  "priority": 0 | 1 | 2              // 0=high(control), 1=normal(samples), 2=low(log/audit)
}
```

### 4.1 Event types (canonical set)

| type | producers | consumers |
|---|---|---|
| `sample` | MCU sensors, UDP telemetry | registry, telemetry ring, analytics, rule VM |
| `entity.changed` | registry (on significant change) | rule VM, analytics, AI context invalidation, Web SSE |
| `entity.command` | Control API | registry, audit log |
| `actuator.changed` | actuation primitives | rule VM, analytics(activation count), UI |
| `rule.triggered`,`rule.action` | rule VM | logs, UI, AI audit, activation counters |
| `rule.lifecycle` | Skill | logs, AI audit |
| `schedule.alarm` | timer wheel / scheduler | rule VM |
| `device.online/offline/discovered` | UDP stack, registry | registry, rules(device-state), UI |
| `inference.request/result/failure` | AI provider | rule VM, logs, AI audit |
| `system.health` | core runtime | logs, UI |
| `user.action` | Skill/UI | AI audit, logs |

### 4.2 Queueing & guarantees

| Property | Policy |
|---|---|
| Buffering | Fixed-depth priority queues per consumer; producers append to the shared ring (event ring, see Storage doc §3.3). |
| Depth | Design 10,000 events ring (bounded), e.g. split 8k normal + 2k high-priority slots (ring is one logical ring, priorities bias drop choice). |
| Overflow | Drop **lowest priority, oldest** first; drop counters; never block producer. Dropped events still yield a `system.health` counter increment. |
| Ordering | Producers publish through a single writer lane; ordering within a producer is FIFO. Cross-producer total order is `ts` plus `seq` tiebreak. |
| Timestamping | Producers stamp `ts` (epoch) when available and `ts_ms` monotonic; consumers do NOT re-stamp. UDP packets carry a source-stamped timestamp the receiver validates for skew (see protocol doc). |
| Priority | `0` control/rule outcomes, `1` samples, `2` audit/log. On congestion, priority-0 events are never dropped while any 1/2 slot exists. |

---

## 5. Rules

Rule store records:

```json
RuleRecord {
  "rule_id": "rule-cooling-001",
  "schema": "qymera.rule.v1",
  "revision": 3,
  "name": "Cooling",
  "enabled": true,
  "canonical": { ...compiled-authored object... },   // §2 of AUTOMATION_ENGINE
  "fingerprint": "sha256-ish checksum",
  "created/updated": 1788050000
}
Executions = { "execution_id", "rule_id", "revision", "ts", "trigger_entity",
               "outcome": "fired" | "suppressed" | "failed" | "cooldown",
               "actions": [ { "entity", "op", "value", "accepted": bool } ] }
```

See [AUTOMATION_ENGINE.md](./AUTOMATION_ENGINE.md) for the full rule model.

---

## 6. Telemetry (sensor history)

Bounded per-entity history, ring-based:

```json
Sample { "seq": 100, "ts": 1788050000, "value_n": 31.4,
         "value_b": null, "reliability": "live" }
SampleAgg { "from": 1788048000, "to": 1788050000, "count": 12,
            "min": 30.1, "max": 32.2, "avg": 31.2, "last": 31.4 }
TimeSeries { "entity": "...temperature", "resolution_s": 300,
             "start": ..., "end": ..., "points": [ ... ], "aggregates": {...} }
```

Structure model:

```
Entity
 ├── recent raw samples (RAM ring, high resolution)
 ├── short-term history (RAM, downsampled 1:60)
 └── aggregated history (flash ring, 1:3600)
```

See [STORAGE_AND_MEMORY.md](./STORAGE_AND_MEMORY.md) §3 for budgets and retention.

---

## 7. Analytics Results

```json
AnalyticResult {
  "kind": "trend" | "average" | "min" | "max" | "count" | "rate"
        | "delta" | "duration" | "time_in_state" | "activation_count"
        | "rolling_avg",
  "entity": "...",
  "window": { "start": ..., "end": ... },
  "value": 0.42,
  "covers": 0.97,          // fraction of window with samples (confidence)
  "recomputed_at": 1788050000
}
```

Analytics is a subscribe-compute-cache consumer: computations are cached per `(kind,entity,window,resolution)` key with `min_interval` to bound cost; rules read cached results (bounded staleness).

---

## 8. Logs (structured)

```json
LogEntry {
  "seq": 2048,
  "ts": 1788050000,
  "layer": "DEBUG" | "INFO" | "WARNING" | "ERROR"
         | "EVENT" | "ACTION" | "AUTOMATION" | "AI" | "SYSTEM",
  "source": "rule" | "ai" | "net" | "core" | "skill" | "hal" | ...,
  "message": "...",
  "refs": { "rule_id":"...", "entity":"...", "repeat": 3 }
}
```

- Replaces the 3-layer/3-level `logger` model (CORE/SENSORS/EVENTS, INFO/WARN/ERROR). The 9 canonical layers above are the contract.
- Audit requirement: `AI` and `AUTOMATION` layers must record the full authored-action trail used by `get_ai_activity`:
  ```
  AI requested rule creation → Rule validated → Rule compiled
  → Rule activated → Rule triggered → Action executed
  ```
- Ring-buffered (RAM recent + flash retention); deduplication of repeats (`repeat` counter).

---

## 9. Inference Results

```json
InferenceResult {
  "request_id": "...", "provider": "remote", "request": {...},
  "response": { "schema": "boolean" | "numeric" | "categorical", "value": true },
  "received_at": 1788050000, "latency_ms": 340,
  "cached": false, "valid": true
}
InferenceStatus {
  "provider": "remote", "mode": "NO AI"|"LOCAL AI"|"REMOTE AI"|"HYBRID",
  "reachable": true, "last_error": null, "requests": 42, "failures": 3,
  "cache_hits": 31
}
```

Inference results are events (`inference.result`) and condition inputs in the rule VM with the fallback policy from [AUTOMATION_ENGINE.md](./AUTOMATION_ENGINE.md) §9.

---

## 10. Naming & IDs Summary

| Object | Key | Notes |
|---|---|---|
| Device | `device_id` (string) | registry PK; independent of `chip_uid` |
| Entity | `device_id/entity_id` | stable; not an array index |
| Rule | `rule_id` | stable, unique |
| Rule revision | `revision` | monotonically increasing per rule |
| Event | `seq` | monotonic post-boot |
| Sample | `(entity, seq)` | ring ordering |
| Execution | `execution_id` | rule run instance |

---

## 11. Validation Rules for Data

1. Values respect `capabilities` + `native_range`; NaN/Inf rejected at ingest.
2. Timestamps: wall clock must be present for calendar/analytics; a sample without a valid clock is flagged `unclocked` and excluded from calendar conditions (retains `timeValid()` semantics).
3. Every write to a bounded structure records a drop/overwrite counter for operability (where the structure is a ring).
4. IDs are case-sensitive strings, `[a-z0-9_-]`, length-bounded.
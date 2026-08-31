# Qymera Dashboard — Storage & Memory Specification

> Part of the Qymera Dashboard architecture contract. Defines how memory is deliberately invested into rules, state, history, events, analytics, logs, AI context — and how unbounded growth is made impossible by design.
> All budgets are **design targets** on an ESP32-class basis (~320 KB usable SRAM typical); final limits require implementation-time measured confirmation.

---

## 1. Memory Classes

| Class | Hardware | Used for | Lifetime |
|---|---|---|---|
| Static RAM | SRAM | registry core, tables, rings (hot), VM images | boot → reboot |
| Heap (bounded pools) | SRAM | request buffers, transient payloads | scoped/leased |
| NVS | internal flash | configuration, credentials, device identity, rule canonical records, counters/watermarks | durable |
| Flash ring (partition) | internal flash | telemetry aggregates, event/log retention beyond RAM, compiled rule images | durable, rotated |
| Flash (code/const) | internal flash | firmware, UI assets (PROGMEM-style), schema constants | immutable |

Strategy: **everything predictable**. Static + fixed pool sizes are compile-time constants validated by `static_assert`/linker map review; heap is only used through bounded arenas with a defined worst-case.

---

## 2. RAM Budget (design targets)

Basis: 320 KB usable SRAM. Headroom target ≥ 20% free for networking + request bursts.

| Subsystem | Budget | Notes |
|---|---|---|
| Core runtime + HAL + timebase | 12 KB | fixed statics |
| Network stacks (lwIP/TCP/UDP) | 40 KB | platform-derived, not app-controlled |
| Device registry | 20 KB | see §2.1 |
| Rule store images (500 rules) | 60 KB | see §2.2 |
| Rule runtime state (500 × 64 B) | 32 KB | fixed per-rule blocks |
| Event ring (10,000 events) | 60 KB | see §3.3 |
| Telemetry RAM rings | 32 KB | see §3.1 |
| Analytics accumulators/cache | 8 KB | fixed table + bounded cache |
| Log ring (RAM) | 8 KB | see §3.2 |
| AI context cache | 8 KB | bounded LRU |
| Web/API request budget | 8 KB | streaming, chunked (pattern retained) |
| UDP RX + TX buffers | 8 KB | bounded datagram budget |
| **Total** | **296 KB** | ≈ 93% — leaves ~24 KB headroom on a 320 KB basis |

> Numbers are illustrative targets; the *structure* (explicit fixed budgets per subsystem, `static_assert` on every boxed size, linker-map diff checks in CI) is the requirement.

### 2.1 Device registry RAM

- Fixed capacity: **devices_table[256]** (design target; 100 sensors => 100 devices typical).
- Per device (packed): `device_id[24]` + `net_id` (16) + identity hash (4) + `last_seen` (4) + flags/state (4) + entity list refs — **~64 B**, 256 × 64 B = **16 KB**.
- Entity records: separate fixed table **entities_table[1024]** (multiple entities per device). Per entity ~40 B packed → **40 KB**. With 1024 entities this pushes total registry to ~20 KB + 40 KB; the budget row above (20 KB) assumes a *streamlined* entity record (~40 B) and 500 entities — a **tradeoff to confirm in §2.1.1**.

**2.1.1 Capacity tradeoff:** either (a) cap at ~500 entities (registry ~20 KB) and keep headroom, or (b) allow 1024 entities and sacrifice ~20 KB elsewhere (e.g. smaller event ring). Recommend (a) for v1.

### 2.2 Rule store RAM

- Compiled `RuleImage` budget ~120 B average (boxed ≤ 160 B). 500 rules → **60 KB**. Persisted canonical JSON lives in flash/NVS, not RAM; RAM holds the hot image plus name/notes are fetched on demand.
- Per-rule instance state fixed ≤ 64 B → **32 KB**.

### 2.3 Heap pools

- Shared event/request arena bounded (e.g. 6 KB); HTTP request handling streams (today's chunked `/calib` behavior is the pattern).
- AI request buffer capped (replaces today's `buildBody()` ~1.5 KB + `staged_prompt[113]`).
- Firmware never does unbounded `String` growth in request paths (today's code already avoids this; keep the discipline).

---

## 3. Bounded Ring Structures

### 3.1 Telemetry rings

Per monitored entity (configurable set; default monitor = all registered numeric sensors):

```
recent raw samples (RAM):  N=32 samples x 12 B = 384 B/entity
short-term history (RAM):  60 x 12 B (1-min bucket) = 720 B/entity
aggregated history (flash): 24h x 12 B (1-h bucket) = 288 B/entity (rotated)
```

- For **64 monitored entities**: RAM ≈ 64 × (384+720) ≈ 70 KB raw → too big. **Resolution policy:** raw ring capacity is a global pool (e.g. 32 KB) split across monitored entities; a *busy* entity gets raw+short, a *quiet* entity gets short-live aggregates from the flash ring only. Bucketing is lazy (sparse) so silent sensors consume ~0.
- Timestamp representation: `ts_ms` (uint64 monotonic+epoch pair, 8 B) + value (float 4 B) = 12 B sample.
- Retention (flash ring, per entity): 24–48 h hourly aggregates typical; configurable. 64 entities × 288 B × 2 days ≈ **36 KB** flash.
- Overwrite: oldest page reclaimed; watermarking + one notification.

**Answers the product questions:**
- "temperature last 6 h" → short-term/hourly buckets.
- "what happened before the relay turned on" → activation-relative window query on the event/log rings (routed to event ring, not telemetry).
- "average humidity" → analytic accumulator + aggregation buckets.
- "is temperature trending up" → trend over short-term bucket deltas.
- "how many times did actuator activate today" → activation counters (per-entity ring-unmaintained, incremented on `actuator.changed`; persisted daily counter in NVS).

### 3.2 Log ring

- RAM: 256 entries × (seq 4 + ts 4 + layer 2 + src 2 + msg 48 + refs 8) ≈ 68 B → **~17 KB** (budget row uses 8 KB for the *hot* slice + flash retention).
- Flash tail: additional rotating entries (e.g. 2,048) → ~140 KB flash. Oldest reclaimed; system watermark logged.
- Repeat coalescing (`repeat` counter) protects rules that log per event.

### 3.3 Event ring

- RAM: 10,000 events × 6–8 B boxed references into a shared payload pool ≈ **60 KB** (payload pool separate, e.g. 24 KB, shared serialized bytes).
- Priority bias (§DATA_MODEL 4.2): drop indexes oldest lowest-priority first.
- The event ring is also the history backbone for `query_events`/`get_recent_events` and event-replay into `test_rule`.

---

## 4. Persistent Storage (NVS + flash)

| Store | Backing | Budget | Contents |
|---|---|---|---|
| Config & credentials | NVS | ~4 KB | WiFi, transport, identity, feature flags |
| Device identity/registry mount | NVS | ~8 KB | provisioned devices (lightweight) |
| Rules (canonical JSON) | NVS + flash image | 500 × 1 KB ≈ 500 KB flash / hot 60 KB RAM | canonical rules + checksums |
| Compiled images | flash | in rules store | boxed RuleImage blobs |
| Telemetry aggregates | flash ring | ~36 KB+ (§3.1) | hourly aggregates |
| Log tail | flash ring | ~140 KB | rotated log pages |
| Event tail (optional) | flash ring | optional | older events |
| AI config | NVS | ~1 KB | provider/mode/endpoint (key write-only) |
| Counters & watermarks | NVS | ~1 KB | activation/day, drop counters, ring watermarks |

Rules persistence model:
- Atomic slot writes with checksums; NVS transactional semantics for multi-slot rule records.
- Written-on-change with diff check (pattern retained from `storage.cpp` `saveRules()` diff check) to protect NVS/flash endurance.
- Rules survive factory-reset-or-not per a **profile** (`keep_rules_on_reset`), because agent-authored automation is now user data.

`factoryReset()` is split into: erase *user data* (registry, rules, telemetry, logs, AI) vs. erase *everything* including credentials; the current all-or-nothing `prefs.clear()` is replaced with scoped resets.

---

## 5. Flash Layout (partition example, 2 MB app / 4 MB flash reference)

```
app0       2 MB      firmware + const (UI, schema)
nvs        48 KB     config/credentials/identity/mounted data
qy_data    ~1.8 MB   rules store + telemetry + log rings (page-rotated)
```
Real partition chosen at implementation; the rule is *a dedicated data partition exists* so RAM rings spill to flash without filesystem overhead.

---

## 6. Scalability Scenarios (worked estimates)

### S1 — Example from brief: **100 remote sensors**
- 100 devices × ~64 B ≈ 6.4 KB registry.
- 250 entities ≈ 10 KB.
- Telemetry pools: raw (20 KB) + short (8 KB) + aggregates (36 KB flash).
- Event ring nominal (60 KB) at 1 event/s ≈ 3 h of ring depth; at 4 events/s ≈ 40 min. (Full-history events go to flash tail.)
- RSSI: fits the §2 budget with headroom.

### S2 — **500 rules**
- Rule images 60 KB + state 32 KB = 92 KB. Subscriber index ≈ 4 KB.
- Timer wheel ≈ 2 KB. Event-driven evaluation: a 10 Hz aggregate sensor rate touches only rules subscribed to it (typical ≤ 5% of rules), not a full scan.

### S3 — **10,000 recent events**
- Event ring 60 KB + payload pool 24 KB = 84 KB (§3.3); high-priority slot oversubscription handled by drop-policy + counters.

### S4 — **N minutes/hours of telemetry**
- Per-entity: raw 32 samples / short 60 min / flash 24–48 h aggregate. For N hours on the flash ring: 64 entities × 1 bucket/hour × 24 h ≈ 36 KB, plus configurable depth.

---

## 7. Memory Tradeoffs (decision log)

| Decision | Tradeoff | Recommendation |
|---|---|---|
| Registry capacity 500 vs 1024 entities | RAM vs scale | 500 (v1), measured then raised |
| Event ring depth | RAM (60 KB) vs history depth | 10,000 ring + flash tail for long-view queries |
| Telemetry raw vs aggregate | fidelity vs RAM | global raw pool, sparse bucketing, flash aggregates |
| Rule images in RAM vs flash-only | speed of trigger vs RAM | hot RAM image for ≤500; cold load fallback for >500 (out of scope v1) |
| Log layer breadth (9 layers) | fidelity vs RAM | hot RAM slice + flash tail; layer filter on ingest retained |
| AI context cache | freshness vs RAM | 8 KB LRU, entity-based invalidation |

## 8. Unbounded-Growth Checklist (audit)

Every subsystem must answer **NO** to all:

1. Can an external input (UDP, HTTP, AI, clock) grow a data structure without an upper bound on its own? 
2. Is any heap allocation larger than a documented constant, outside a bounded arena?
3. Can a ring/table entry overflow silently without a counter/log?
4. Is any string concatenated into an unbounded buffer in a request path?
5. Does any consumer drop events without a counted watermark?

*(The rule engine also requires condition-graph budgets and per-rule boxes from the Automation Engine doc; they are enforced here too.)*
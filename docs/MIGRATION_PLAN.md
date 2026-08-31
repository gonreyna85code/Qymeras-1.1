# Qymera Dashboard — Migration Plan (from Qymera 1.1)

> Companion to the architecture contract. Concrete mapping of the current `feature/ai-experiments` tree (Qymera 1.1) into the Qymera Dashboard product: what to keep, rewrite, remove, replace, and build new — in dependency order.

---

## 1. Current-State Snapshot (repository analysis, branch `feature/ai-experiments`)

| File | Current role | Verdict |
|---|---|---|
| `platformio.ini` | Arduino framework; envs esp8266_generic / esp32_devkit / esp32c3_devkit | **REPLACE** (ESP32-only, no Arduino) |
| `src/config.h` | platform auto-detect + limits + EEPROM/PWM layout | **REWRITE** |
| `src/main.cpp`, `core.h/cpp` | cooperative boot/loop, OTA, WiFi | **REWRITE** |
| `src/sensors.h/cpp` | `Calibration[64]` flat array, local/remote, timezone | **REWRITE** (→ device registry + entities) |
| `src/mesh.h/cpp` | UDP + ESP-NOW dual transport, protocol v1–v5, `RemoteDevice[64]` | **REPLACE** (→ UDP-only v6 protocol) |
| `src/espnow_p2p.h/cpp` | ESP-NOW RX FIFO, peers | **REMOVE** |
| `src/automations.h/cpp` | `Rule[20]` fixed, EDGE/THRESHOLD/TIME/INTERVAL, 50 ms scan | **REWRITE** (→ Automation VM) |
| `src/ai.h/cpp` | prompt slots, DIGITAL/ANALOG/ANALYTIC/CONTROL, chat relay | **REPLACE** (→ Skill/API + inference provider) |
| `src/web.h/cpp` | Arduino `WebServer`, ad-hoc endpoints, basic auth + rate limit | **REWRITE** |
| `src/html.h/cpp` | embedded tabs UI, `AiPanel` | **REWRITE** (→ operational dashboard) |
| `src/log.h/cpp` | 3 layers × 3 levels, ring 30/layer, UDP log | **REWRITE** (→ 9-layer structured ring) |
| `src/storage.h/cpp` | EEPROM/Preferences, fixed offsets, diff-checked writes | **REWRITE** (→ NVS + flash rings; reuse diff-check + zero-fill lessons) |
| `tests/host_sanity.py` | Python mirror of firmware logic | **KEEP** (extend) |
| `docs/architecture-baseline.md` | current-1.1 baseline | **KEEP** as historical reference |

---

## 2. Classification

### KEEP (reusable logic & patterns; not code-as-is)

- **Diff-checked, validated persistence writes** (`storage.cpp`: compare-before-write, zero-fill on `get()`, magic/version/uid gating) → apply to NVS + rule store.
- **Strict input parsing** (`web.cpp`: `parseStrictUnsigned/Long/Float`, no silent coercion) → Skill/API validation primitives.
- **Bounded RX drain + batch discipline** (`mesh.cpp`: `MAX_RX_PACKETS_PER_TICK`, datagram batching under MTU, drain-after-read, oversize-drop) → UDP v6 transport.
- **Streamed HTTP responses** (`web.cpp` chunked `/calib`) → Skill/API + dashboard big outputs.
- **Wire-size `static_assert` discipline** (`mesh.cpp`) → protocol v6.
- **Anti-bounce / debounce (CONFIRM_READS) & pulse/fade actuation** → rule condition/action primitives.
- **UTC-clock + timezone-as-offset model** (`sensors.cpp`) with `timeValid()` gating → time service + calendar conditions.
- **Host-sanity Python test pattern** → extended suite.
- **`api_key` write-only policy** (`web.cpp`/`storage.cpp`: never echo key) → inference config.

### REWRITE

- `config.h` → `platform.h` (ESP32-class only), limits as compile-time constants + `static_assert`.
- `core.*` → runtime: boot, tick budget, health/watchdog, time base, service gating (keep the deferred-init-after-WiFi idea; no `delay()` blocking patterns).
- `sensors.*` → `devices/` registry + `entities` + telemetry ingest. Keep calibration semantics (min/max/correction/persist/pulse/fade) but re-home them as entity attrs behind stable IDs.
- `automations.*` → `rules/` model/compiler/vm per [AUTOMATION_ENGINE.md](./AUTOMATION_ENGINE.md).
- `log.*` → 9-layer structured ring per [DATA_MODEL.md](./DATA_MODEL.md) §8.
- `web.*` → thin HTTP layer over the Skill/API per [AI_AGENT_SKILL.md](./AI_AGENT_SKILL.md).
- `storage.*` → NVS + flash ring store; scoped factory reset.

### REMOVE

- **ESP8266 code**: all `#if defined(ESP8266)` branches (`config.h`, `core.cpp`, `ai.cpp`, `web.cpp`, `mesh.cpp`, `espnow_p2p.cpp`, `storage.cpp`), `ESP8266WiFi`, `ESP8266WebServer`, `ESP8266HTTPClient`, BearSSL special cases, the "ESP8266 20 s HTTP cap" workarounds, `esp8266_generic` env.
- **Arduino framework**: `<Arduino.h>` everywhere, `WebServer`/`ESP8266WebServer`, `HTTPClient`, `WiFiClientSecure`, `ArduinoOTA`, `EEPROM.h`/`Preferences` (moved to HAL), `String` in core code (replaced by bounded buffers), `delay()` in loops.
- **ESP-NOW**: `espnow_p2p.*`, `Transport` enum, `setTransport`, ESP-NOW branches in `mesh.cpp`, `RemoteDevice[MAX_SENSORS]` slot model, peer table.
- **Obsolete AI architecture**: `ai.cpp/h` prompt-slot engine (`AI_MAX_PROMPTS`, `OutType`, `staged_prompt`, `extractJsonString`, `CONTROL` flat-object parse, `performRequest`), `/ai/run`, `/ai/status`, prompt-slot EEPROM block.
- **Obsolete fixed-size assumptions**: `MAX_RULES=20`, `MAX_SENSORS=64`, `MAX_PERSISTED_SENSORS`, index-addressed rules (`sensor_idxs[]`/`actuator_idxs[]`), `EEPROM_RULES_SIZE`, remote-slot reclaim coupled to rule references (`isIndexReferenced`).
- **Fragmenting patterns**: `String` accumulation in request hot paths; `last_error[64]` textual error model.

### REPLACE

- `mesh.*` transport → `net/` UDP v6 stack ([NETWORK_PROTOCOL.md](./NETWORK_PROTOCOL.md)).
- `ai.*` → `skill.*` (agent Skill/API) + `ai/` provider abstraction ([AI_AGENT_SKILL.md](./AI_AGENT_SKILL.md)).
- Fixed EEPROM-layout storage → NVS + flash ring stores.
- Old UNARY rules → compiled rule VM.
- Old `/ai/chat` schema injection → versioned Skill schema + dedicated context tools.

### NEW

- **Event bus** (structured events, priority queues, fan-out) — [DATA_MODEL.md](./DATA_MODEL.md) §4.
- **Device registry + entities + capabilities** — [DATA_MODEL.md](./DATA_MODEL.md) §1–3.
- **Telemetry ring buffers** + aggregation/organization — [STORAGE_AND_MEMORY.md](./STORAGE_AND_MEMORY.md) §3.1.
- **Analytics** subsystem (accumulators, trend/rates, caching).
- **AI context engine** (targeted retrieval, budgets, LRU) — [AI_AGENT_SKILL.md](./AI_AGENT_SKILL.md) §8.
- **Inference provider abstraction** (LOCAL/REMOTE/HYBRID, timeout/failure/cache/rate-limit/fallback) — [AUTOMATION_ENGINE.md](./AUTOMATION_ENGINE.md) §9 + Skill §12.
- **Operational web dashboard** (Overview/Devices/Sensors/Automations/AI/Analytics/Events/Logs/Settings).
- **Flash ring partitions** + page rotation; scoped factory reset; **Matter adapter seam** (interface only).
- **`qym_hal`** (GPIO, PWM/LEDC, net, NVS, timers, reset) — the replacement for Arduino APIs.
- **`test_rule` dry-run**, rule versioning/revision store, activation counters.

---

## 3. Phased Implementation Plan (dependency order)

### Phase 0 — Decisions & scaffolding
- [D1] Confirm HAL depth interpretation (thin `qym_hal` over IDF toolchain primitives; cooperative loop). *(Open decision §18.1 in architecture doc.)*
- [D2] Confirm numbers: registry capacity (500 entities), rules (500), event ring (10,000), telemetry pools (§2 Storage doc).
- Scaffold: drop ESP8266 env; disable Arduino framework; create HAL skeleton; delete `espnow_p2p.*`; strip `#ifdef ESP8266`; lint for `Arduino.h`/`WiFi.h`/`WebServer`.
- Gate: **clean ESP32 build with zero ESP8266/Arduino/ESP-NOW symbols**.

### Phase 1 — Foundations (no rules yet)
- Core runtime (boot, tick budget, time base, watchdog), HAL GPIO/PWM/timers/NVS, storage layer (NVS + flash ring primitives), registry + entity model + capabilities, event bus, structured log ring.
- Keep control **as-is** behaviorally: relay/dimmer actuation through control API; UI reduced to minimal Devices/Settings placeholder.
- Gate: 6-hour soak, no growth in metrics, host-sanity suite extended (event model, registry, time).

### Phase 2 — UDP v6 + remote devices
- `net/` protocol v6 (frame, discovery/registration, telemetry, commands, acks, seq/dup, offline), batch/drain budgets, per-device rate budgets.
- Remove legacy v1–v5 parse path; migrate fleet semantics doc.
- Gate: Dashboard + N simulated devices (host harness) — 100+ devices, telemetry rate limits, offline detection.

### Phase 3 — Automation Engine
- Rule model/DSL, validation pipeline, compiler/normalizer, VM with event-driven dispatch + timer wheel, sustained-window state machine, historical/aggregate conditions, action executor through control API.
- Rule store + versioning/revision; `list_rules/get_rule`; boot compile + activation.
- Gate: host property tests (trigger/duration/cooldown/trend), 500 rules soak, E2E "Cooling" rule.

### Phase 4 — Telemetry, Analytics, Context
- Telemetry rings + aggregation + flash spill; analytics accumulators + `query_history`/`aggregate`/`trend`; AI context engine (budgets/LRU); event/log flash tails.
- Gate: "last 6 h", "before relay on", "average humidity", "trend up", "activations today" all answerable via Skill.

### Phase 5 — Skill/API + Inference
- Skill surface (full tool set, schemas, validation, errors, permissions, idempotency, rate limits), HTTP binding `/skill/v1`, schema constant in flash.
- Inference provider abstraction + fallback policies; `NO AI` default verified byte-identical.
- Browser agent binds the Skill (replaces `/ai/chat` shuffle).
- Gate: agent-authoring workflow E2E (discovery → create_rule → compile → activate → audit).

### Phase 6 — Dashboard UI
- Operational dashboard UX (9 areas), SSE live updates, rule inspector + dry-run, AI activity view.
- Gate: manual soak, memory/perf regressions green.

### Phase 7 — Matter seam + hardening
- Publish the adapter seam (interfaces only); Matter developed separately.
- Partition sizing, endurance testing, overload soak, flash-ring wear validation, security review (auth default state).

---

## 4. Ownership of Removed Code (trail)

- Deleted files: `src/espnow_p2p.*`, `src/ai.*`, legacy `src/mesh.*` (replaced by `net/`), `src/automations.*` (replaced by `rules/`), `src/log.*` (replaced by `logs.*`).
- Kept-at-HEAD-for-history: prior commits retain the ESAired code; no "Zombie" files remain in the build. Architectural references to removed subsystems are deleted from docs.

---

## 5. Risks & the Unknowns to Probe Early

1. **HAL depth** (D1) is the single highest-risk decision — prototypes `qym_hal` (WiFi/lwIP, GPIO/PWM, NVS) before Phase 2.
2. **RAM budget** (Storage doc §2) must be validated with a real link map early; the 93% basis is tight — contingency: shrink event ring or entity capacity.
3. **Event-driven rule VM** with per-entity subscriber index must be proven at 500 rules / 10 Hz sample rate before Phase 5 locks the interface.
4. **Flash ring wear**: page-rotation math verified with logging on soak; NVS endurance for config counters monitored.
5. **Registry entropy with 200 devices** on DHCP churn — protocol `src_uid`-based addressing must be proven with a simulated IP-change test.
6. **Inference availability p95** in `HYBRID/LOCAL` — the cache/fallback behavior must be soak-tested to avoid silent automations.

---

## 6. Effort & Sequencing Summary (rough)

| Phase | Focus | Est. share |
|---|---|---|
| 0 | scaffolding + decisions | 5% |
| 1 | foundations | 20% |
| 2 | UDP v6 + devices | 15% |
| 3 | automation engine | 25% |
| 4 | telemetry/analytics/context | 12% |
| 5 | Skill/API + inference | 13% |
| 6 | UI | 8% |
| 7 | seam + hardening | 2% |

Phases 1–3 are the critical path; 5–6 ride on the Skill surface defined in Phase 3.4/5.
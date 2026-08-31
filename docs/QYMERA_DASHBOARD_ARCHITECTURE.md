# Qymera Dashboard — Architecture Specification

> **Document status:** Architecture contract for the Qymera Dashboard implementation phase.
> **Scope:** Documentation and architectural specification only. No implementation is implied by this document.
> **Target repository:** `https://github.com/gonreyna85code/Qymeras-1.1`, branch `feature/ai-experiments` (Qymera 1.1 lineage).

---

## 1. Product Summary

**Qymera Dashboard** is a local-first, deterministic automation runtime and AI gateway for ESP32-class hardware.

It is *not* "Qymera with AI added". The architecture is redesigned around:

- AI-agent-created automations
- deterministic local execution
- large numbers of rules (hundreds, not a small fixed array)
- more complex automation logic (multi-condition, stateful, timed, historical)
- large numbers of remote sensor/actuator devices over **UDP**
- structured events
- persistent history (bounded ring buffers)
- analytics
- structured logs
- an agent-facing Skill/API as a first-class subsystem
- optional external AI inference (`NO AI`, `LOCAL AI`, `REMOTE AI`, `HYBRID`)
- a web UI that is an operational dashboard, not a config screen

---

## 2. Goals

| # | Goal |
|---|------|
| G1 | Local-first: automations keep running with no internet, no AI provider, no browser. |
| G2 | AI is the author, Qymera is the runtime: the LLM creates/edits *compiled, validated* automation artifacts; execution never requires an LLM. |
| G3 | Deterministic rule execution with event-driven evaluation, not "re-evaluate everything every loop". |
| G4 | Scale to hundreds of rules and tens/hundreds of remote devices with a bounded, predictable memory footprint. |
| G5 | UDP is the *only* transport to remote Qymera devices. ESP-NOW and ESP8266 are removed from the product entirely. |
| G6 | Persistent, bounded telemetry (ring buffers) plus lightweight analytics so questions about the past are answerable locally. |
| G7 | A full agent-facing Skill/API surface so external agents can discover, control, inspect, and program the system. |
| G8 | Matter exists only as an external/optional adapter; the Dashboard core has no Matter dependency. |
| G9 | Structured, auditable logs (including AI actions) in a bounded ring buffer. |

## 3. Non-Goals

| # | Non-goal |
|---|----------|
| N1 | No ESP8266 support. ESP8266 compatibility code is removed, not preserved. No abstractions that exist "for both". |
| N2 | No Arduino framework / Arduino library dependencies. No `Arduino.h` in the product. |
| N3 | No full-migration to ESP-IDF as an application framework. The lightweight cooperative runtime stays. (See §5.2.) |
| N4 | No ESP-NOW anywhere: no abstraction, no fallback, no compatibility layer. |
| N5 | Matter is **not** implemented here. Only its adapter boundary is specified. |
| N6 | No unbounded storage. Everything that can exceed a budget is a bounded ring or a fixed-capacity structure. |
| N7 | No cloud dependency. External inference is optional and must be replaceable by `NO AI`. |
| N8 | No direct GPIO execution by the LLM/agent. All actuation goes through the validated control path. |

## 4. Principles

```
AI is the author.        Qymera is the runtime.
AI can create intelligence.   Qymera guarantees execution.
AI is optional.          Automation is not.
Internet is optional.    Local control is not.
Matter is optional.      Qymera core is not dependent on it.
UDP is the remote-device transport.
Memory is spent primarily on:
rules + state + history + events + analytics + context.
```

Additional principles:

1. **Determinism first.** An automation's effect is fully reproducible from its compiled rule and the observed events, without any network or AI provider.
2. **Validation is a hard gate.** Nothing the agent produces reaches the rule engine without passing compile + validation.
3. **Bounded memory is a hard requirement.** Every subsystem declares a worst-case budget; growth is rejected by design (ring buffers, fixed tables, compile-time caps, explicit heap pools).
4. **One event, many consumers.** The event bus fans out to the rule engine, analytics, logger, AI context, and web UI. Consumers never block producers.
5. **Single-writer actuation.** There is exactly one control path to actuators (through the Skill/API control functions); the agent, UI, and rules all use it.
6. **Optionality by construction.** Rule/condition evaluation that depends on inference degrades according to a documented fallback policy; the rule still runs.

---

## 5. Platform & Framework

### 5.1 Hardware target

- **Supported:** ESP32-class modules (ESP32, ESP32-S2, ESP32-S3, ESP32-C3 minimum; ESP32-D0WD family reference).
- **Unsupported and to be removed:** ESP8266, ESP8285.
- **Memory reference:** ESP32 typical (SRAM ~520 KB total, ~320 KB usable heap; some modules ~160–250 KB usable). Numeric budgets in [STORAGE_AND_MEMORY.md](./STORAGE_AND_MEMORY.md) are derived from a 320-KB usable basis and are design targets, not guarantees.

### 5.2 Toolchain / framework decision

**Current state (inspected, branch `feature/ai-experiments`):**

- PlatformIO (`platformio.ini`) with the **Arduino framework**, three environments: `esp8266_generic`, `esp32_devkit`, `esp32c3_devkit` (`espressif32@6.5.0`, `espressif8266@3.30102.0`).
- Nearly every translation unit includes `<Arduino.h>`; OTA via `ArduinoOTA`; web via `ESP8266WebServer`/`WebServer`; AI HTTP via `HTTPClient`/`WiFiClientSecure`; storage via `EEPROM`/`Preferences`.
- Single-threaded cooperative loop: `core::loop()` drives web, mesh, sensors, automations, AI, OTA.

**Target decision for Dashboard:**

- Keep the **lightweight, cooperative, single-loop runtime** and the "spend memory on rules/state/history, not on a framework" philosophy.
- **Remove the Arduino framework dependency.** The product is compiled with a minimal **internal HAL (`qym_hal`)** that wraps only what the ESP32 platform requires (networking sockets/TCP stack, GPIO, PWM/LEDC, timers, NVS, panic/reset). The HAL is a thin, compile-time-y cheap layer; no Arduino-style `setup/loop` macros, `String`, `WiFi`/`WebServer` classes, or `delay()` semantics inside the core.
- **Do not adopt the ESP-IDF framework architecture** (no IDF event loop as the app model, no IDF `main` app entry beyond `app_main()`, no fat framework components). IDF is used only as the on-target toolchain/RTOS underneath the HAL (timers, lwIP sockets, NVS) — i.e., *compile with the IDF toolchain, but keep our own thin runtime*. This keeps the interpretation consistent: "lightweight framework, ESP32-only, no Arduino, no full IDF migration".

> **Tension note / decision record:** "no Arduino" and "no ESP-IDF" on ESP32 are jointly unsatisfiable at the flash/RTOS level; the interpretation above (thin HAL over IDF toolchain primitives, cooperative loop retained) is the recommended reading and is recorded as a deliberate, documented tradeoff. A project decision is required before implementation. See §13 "Open decisions".

### 5.3 Language and build

- C++17, freestanding-style (no RTTI/exceptions), compile-time constants, `constexpr` layout checks (`static_assert` on all wire formats — retained pattern from `mesh.cpp`).
- Build: PlatformIO `espressif32` platform, **Arduino framework disabled**, `lib_deps` containing only HAL prerequisites.
- Partition scheme: dedicated storage partition for logs/telemetry (see Storage doc).
- Sanity tests remain host-runnable (`tests/`), mirroring the firmware logic in Python as today.

---

## 6. System Architecture

```
                    ┌────────────────────────────────────────────┐
                    │              QYMERA DASHBOARD               │
                    │                                            │
   Browser/Agent ──►│  Web UI  ──►  Skill/API  ◄── Agent        │
   (Matter) ◄──────►│ (adapter)                                 │
                    │                                            │
                    │        ┌───────── Event Bus ─────────┐    │
                    │        │                             │    │
   Remote   ◄─────►│  UDP    │  Rule     Analytics   Logs  │    │
   Devices         │  STACK  │  Engine   /Telemetry  Ring  │    │
   (sensors/        │        │  (VM)     Buffers    Buffers│    │
    actuators)      │        │                             │    │
                    │        └─────┬───────────────────────┘    │
                    │              │                            │
                    │   AI Context Engine ◄── Skill/API ───► External AI
                    │              │                            │   (optional)
                    │   Device Registry / NVS / Flash / RAM     │
                    └────────────────────────────────────────────┘
```

Components at a glance:

| Component | Responsibility | Doc |
|-----------|----------------|-----|
| Core runtime | boot, cooperative loop, time base, scheduling | this doc |
| Event bus | structured event fan-out with bounded queues | this doc |
| Device registry | devices + entities, discovery, lifecycle, capability model | [DATA_MODEL.md](./DATA_MODEL.md) |
| Rule engine (Automation VM) | compiled rule evaluation, timers, state | [AUTOMATION_ENGINE.md](./AUTOMATION_ENGINE.md) |
| Telemetry / ring buffers | bounded sensor history | [STORAGE_AND_MEMORY.md](./STORAGE_AND_MEMORY.md) |
| Analytics | derived metrics from history | [DATA_MODEL.md](./DATA_MODEL.md) |
| Structured log ring | auditable, layered logs | [DATA_MODEL.md](./DATA_MODEL.md) |
| AI context engine | targeted retrieval for agents | [AI_AGENT_SKILL.md](./AI_AGENT_SKILL.md) |
| Agent Skill/API | first-class tool surface | [AI_AGENT_SKILL.md](./AI_AGENT_SKILL.md) |
| UDP stack | discovery/telemetry/commands to remote devices | [NETWORK_PROTOCOL.md](./NETWORK_PROTOCOL.md) |
| External inference | optional provider abstraction | this doc + AI_AGENT_SKILL |
| Web dashboard | operational UI | [AI_DASHBOARD_PRODUCT.md](./AI_DASHBOARD_PRODUCT.md) |
| Storage layer | NVS/config, flash ring paging, RAM budget | [STORAGE_AND_MEMORY.md](./STORAGE_AND_MEMORY.md) |

---

## 7. Core Runtime

### 7.1 Execution model

- Cooperative single loop with a **tick budget**. Per `loop_tick()`:
  1. drain UDP RX (bounded number of datagrams per tick — retained from `mesh.cpp` `MAX_RX_PACKETS_PER_TICK` pattern, but budget-based),
  2. publish decoded UDP packets → event bus,
  3. run event-driven rule dispatch (only rules subscribed to arriving events/timers),
  4. service timer wheel (rule timers, cooldowns, intervals — O(1) amortized),
  5. run periodic housekeeping (device staleness, ring compaction, flash paging, NTP refresh),
  6. serve one web/API request slice,
  7. update sensors that are polled (MCU-attached) and publish their samples.
- **Bounded work per tick**: a watchdog/health counter detects a starved tick and degrades (see §12).
- Time base: monotonic `ticks_ms` for durations + wall-clock epoch (NTP) for calendar conditions; both are explicitly modeled (the current code conflates `millis()` timestamps and epoch — a known defect to fix).

### 7.2 Boot sequence

```
boot →
  toolchain/low-level init (HAL)
  storage init (NVS; mount flash ring partitions)
  load dashboard config (identity, network, transports)
  init device registry (load persisted devices + capabilities)
  init event bus + subscriber registration
  load + validate + compile persisted rules  →  rule VM image
  restore rule state timers (persistent timers only where durable)
  init analytics/telemetry rings (zeroed, retention restore)
  init log ring
  init web + Skill/API
  init UDP stack (bind)
  → network (STA; AP fallback for provisioning)
  → first telemetry pass, announce discovery
  → steady-state loop
```

Rules are loaded/compiled **before** the network comes up: automation is operational from the first tick (matches "local-first").

### 7.3 Module map (new source layout)

```
src/
  main.cpp           app_main → core::run()
  core.cpp/.h        runtime: boot, loop, time base, tick budget
  hal/               qym_hal (gpio, pwm/ledc, net tcp, nvs, timers, reset)
  event_bus.*        event definitions, queues, fan-out
  devices.*          device registry, entities, capabilities, lifecycle
  rules/*            model, parser, compiler, vm (AUTOMATION_ENGINE)
  telemetry.*        sensor ring buffers
  analytics.*        derived metrics
  logs.*             structured log ring
  context.*          AI context engine (targeted retrieval, budgets)
  skill.*            agent Skill/API dispatch + validation
  net/{udp,discovery,proto}.*   UDP protocol
  web/{server,ui}.*  dashboard HTTP + UI assets
  ai/{provider,cache}.*  external inference abstraction
  storage/*          NVS + flash ring paging
  util/*             parsers, ring primitives, strict number parsing
```

Deleted by the migration: `espnow_p2p.*`, all `#if defined(ESP8266)` blocks, Arduino framework includes, the "AI prompt slot" engine (replaced by the agent skill), the fixed `MAX_RULES` rule table (replaced by the compiled rule store).

---

## 8. Data Flow

### 8.1 Telemetry data flow

```
MCU sensor / remote device (UDP)
        │ sample (structured: {device, entity, value, unit-ish, ts})
        ▼
   Event bus  "entity.changed" / "sample"
        ├──► Device registry (update state, last_seen)
        ├──► Telemetry ring (append + optional aggregate stage)
        ├──► Rule VM (dispatch subscribers for this entity)
        ├──► Analytics (rolling aggregates, counters)
        ├──► Log ring (audit: "sample recorded")
        └──► AI context (invalidates/refreshes cached context for that entity)
```

### 8.2 Control data flow (single path)

```
caller: agent (Skill) │ UI │ rule action │ UDP command
        ▼
  Control API: validate entity, capability, bounds, permission
        ▼
  Actuation primitive (relay/dimmer/…)  → GPIO via HAL  (local)
                                        → UDP command  (remote device)
        ▼
  event bus: "entity.command" + "actuator.changed"
        ▼
  registry state update + log audit + (if tracked) activation counter
```

### 8.3 AI authoring data flow

```
User request (natural language)
        ▼
Agent (LLM) ← context via AI Context Engine (targeted fetch)
        ▼
Tool call: create_rule / update_rule  (structured rule JSON)
        ▼
Skill/API  →  validation (schema, references, semantics)
        ▼
Rule Compiler / Normalizer  (AST → IR/bytecode)
        ▼
Persistent automation store (NVS/flash)  + activation
        ▼
Deterministic Local Rule Engine (no LLM required)
        ▼
Actuators (single control path)
```

---

## 9. Event Flow

```
Producer                    Event Bus               Consumers
─────────                   ─────────               ─────────
UDP RX ──────────────────►{ sample        }──► registry, telemetry,
MCU sensors ─────────────►{ entity.changed}      analytics, rule VM
rule VM ─────────────────►{ trigger      }──► log ring, web SSE, context
control path ────────────►{ command      }
AI://skill ──────────────►{ rule.lifecycle}
time service ────────────►{ schedule/alarm}
system ──────────────────►{ health }
inference (optional) ────►{ inference.result }
```

Event model, priorities, queue depth, overflow policy, ordering and timestamping are specified in [DATA_MODEL.md](./DATA_MODEL.md) §4 and bounded in [STORAGE_AND_MEMORY.md](./STORAGE_AND_MEMORY.md) §3.3. Rules are evaluated **event-driven**: the VM keeps a subscriber index keyed by entity / condition type; an event touches only affected rules.

---

## 10. AI Flow (authoring + inference)

1. **Context**: agent queries `Skill` → `Context Engine` returns *targeted* slices (devices, capabilities, states, rules, recent events, analytics, history) with per-tool size budgets. Never the whole system dump (the current `buildBody()` pushes an ad-hoc text macro; this is replaced by explicit, limited context tools).
2. **Authoring**: agent calls `create_rule` with a structured rule object → schema validation → reference resolution (entities must exist, types/capabilities must match) → semantic validation (conflicts, malformed logic) → compile → persist → activate → structured result returned to the agent.
3. **Inference (optional)**: a rule condition of type `inference` references an *inference entity*; at runtime the VM publishes a request to the inference provider with a bounded timeout. Result → event `inference.result` → the condition resolves → rule continues. On missing/invalid/expired result the condition evaluates per the rule's fallback policy (`fail_open`/`fail_closed`/`last_known`).
4. **Audit**: every authoring action and every inference request is written to the structured log ring (`AI` layer).

---

## 11. Automation Lifecycle

```
DRAFT (agent schema-op) ──► VALIDATED ──► COMPILED ──► PERSISTED ──► ACTIVE
   │ (in-request only)                            │                 │
   └──── reject (schema)          ┌───────────────┘  ┌──────────────┘
                                  ▼                   ▼
                          (version bump)      DISABLED (paused, kept)
                                  │                   │
                                  └───────────────────┴──► DELETED (archive log retained)
```

- Each rule carries `version` and `revision`; updates create a new revision; execution results reference the compiled `rev`.
- Activation is atomic: compile + write + switch pointer in the VM image.
- Full lifecycle details: [AUTOMATION_ENGINE.md](./AUTOMATION_ENGINE.md) §11.

---

## 12. Networking

### 12.1 Transport

```
Qymera Dashboard
        │
       UDP
        │
+───────┼───────┐
│       │       │
Remote  Remote  Remote
Device  Device  Device
```

- **UDP over Wi-Fi** is the only remote-device transport.
- Two ports (retained concept from current `BROADCAST_PORT`/`COMMAND_PORT`): **discovery/telemetry broadcast** and **directed command**, plus an **ack/telmetry-directed unicast** channel.
- No ESP-NOW, no serial-based inter-node transport, no fallback transport modes (`mesh::setTransport()` and the `Transport` enum are removed).

### 12.2 Scale targets

- Design target: **50–200+ remote devices**, thousands of entities. The current per-node `remote_devices[MAX_SENSORS]` + flat `calibrations[MAX_SENSORS]` model caps the system at 64 total entities and is explicitly documented as the limit to remove (see [MIGRATION_PLAN.md](./MIGRATION_PLAN.md) and §14).

### 12.3 Boundries the transport respects

- The UDP stack produces **structured events only**; it does not write device state directly.
- Commands to remote devices go through the same control API as local actuation (address translation agency), preserving one control path.
- Protocol v6 is versioned with seq/ack and a bounded retry budget; see [NETWORK_PROTOCOL.md](./NETWORK_PROTOCOL.md).

---

## 13. Storage & Memory Strategy (summary)

Full detail in [STORAGE_AND_MEMORY.md](./STORAGE_AND_MEMORY.md). Summary decisions:

| Structure | Placement | Bounding |
|---|---|---|
| Rules (compiled image) | RAM (hot) + flash (persisted) | size-boxed rule, fixed-count store with capacity policy |
| Rule runtime state | RAM | per-rule fixed block |
| Device registry | RAM (hot) + NVS (identity/religion) | fixed capacity, LRU/provisioning policy |
| Telemetry rings | RAM (recent) + flash ring (retention) | per-entity ring, retention windows |
| Event queues | RAM | fixed depth, priority-drop policy |
| Analytics accumulators | RAM | fixed set |
| Log ring | RAM (recent) + flash ring | bounded entries |
| AI context cache | RAM | bounded LRU |
| Web/HTTP | flash (assets PROGMEM) + small RAM | streaming responses |
| Config/credentials | NVS | versioned |

No unbounded or dynamically-growning structures are allowed. Dynamic allocation is confined to bounded pools/arenas with explicit worst cases.

---

## 14. Scalability

| Axis | Current (Qymera 1.1) | Dashboard target | Enabling change |
|---|---|---|---|
| Rules | 20, fixed array | hundreds (e.g. 500) | compiled rule store + capacity policy, no per-loop full scan |
| Rules per rule | max 5 sensors / 5 actuators | arbitrary bounded (e.g. ≤ 8 inputs, ≤ 8 actions) | normalized IR |
| Devices/entities | `MAX_SENSORS`=64 total, index-addressed | registry with stable entity IDs, hundreds of devices | device registry + entities |
| Remote devices | ≤64 slots, stale-reclaim hacks | 50–200+ devices, per-device last_seen independent of slot count | registry keyed by device/net identity |
| Events | none (logs only) | 10k buffered events (bounded) | event ring |
| Telemetry | none | hours of history per monitored entity | telemetry ring + flash paging |
| Transport | dual UDP/ESP-NOW, 8 datagrams/tick | UDP-only, budget-driven RX | protocol v6 |

---

## 15. Matter Adapter Boundary

Matter is developed on a separate branch/component. Dashboard core exposes a minimal seam:

```
               Qymera Core
                   │
        ┌──────────┴──────────┐
        │          │          │
       UDP        AI      Optional adapters
                          │
                        Matter
```

Required interfaces (specified only — no Matter implementation):

1. **Entity access seam** — processor-neutral "read entity / subscribe to entity" API so Matter can bridge attribute reads and subscribe to changes without touching the core.
2. **Control seam** — Matter writes go through the **same control API** as rules/agents/UI (single control path; permissions apply equally).
3. **Event subscription seam** — Matter adapter registers as an event-bus consumer (bounded; it must keep up or drop per policy).
4. **Registry seam** — Matter needs stable device/entity IDs, capability descriptors, and "online" state; core provides them with no Matter types in the interface.
5. **Config seam** — adapter enabling lives in Dashboard config (`optional_adapters`) but is *ignored* by the core; the core never imports Matter headers.

The core compiles and runs with Matter absent/disabled; there is no runtime dependency.

---

## 16. Web Dashboard (operational UI)

Redirected from "configuration interface" to "operations console". Conceptual areas:

```
Overview | Devices | Sensors | Automations | AI | Analytics | Events | Logs | Settings
```

- **Observability first**: live state, health, memory, transport metrics, last-seen.
- **AI activity visibility**: audit trail of agent actions (`AI` log layer), rule authorship, inference calls.
- **Automation inspection**: rule list, compiled representation, triggers, state, activation history.
- **Manual control** through the same control API as everything else.
- Event/log/history viewers backed by the rings.
- Streaming updates via bounded SSE (single consumer fan-out through the event bus), not polling every endpoint.
- The UI is a *view over the Skill/API*: the browser agent talks to the same tools the LLM agent talks to.

---

## 17. Failure Modes & Degradation

| Failure | Behavior |
|---|---|
| AI provider down | Inference conditions follow their fallback policy; all deterministic rules unaffected; agent Skill returns structured errors. |
| No internet | Local automations, UDP, UI, history all continue; NTP-only features degrade (calendar rules use last-known time basis). |
| Remote device offline | Marked offline in registry; rules with `device-state` conditions evaluate `offline` deterministically; telemetry marked unreliable. |
| Event queue congestion | Prioritized drop policy + counter (never blocks producer, never grows). |
| Flash log/telemetry full | Oldest pages reclaimed; watermark logged. |
| Rule compile error | Rule rejected at authoring time with structured error report; store keeps last-good revision. |
| Tick starvation | Health counter; heavyweight tasks yield; watchdog escalates to reboot with diagnostic log. |
| Power loss mid-persist | Versioned + checksummed records; atomic slot writes; NVS transactional semantics. |

---

## 18. Open Design Decisions

Decisions recorded for implementation-phase confirmation:

1. **HAL depth** (§5.2): exact mapping of "no Arduino, no full IDF" — recommended thin `qym_hal` over IDF toolchain primitives; requires explicit sign-off.
2. **Rule IR format**: tree (AST-like) versus flat bytecode; recommended flat contiguous "condition graph" in [AUTOMATION_ENGINE.md](./AUTOMATION_ENGINE.md) §7.3.
3. **Rules capacity policy**: hard cap with admission control vs. "soft" store with eviction; recommended hard cap + explicit activation fails.
4. **Flash ring wear**: page rotation + wear-leveling strategy; recommended fixed-page rotation with NVS counters (no over-the-top filesystem dependency).
5. **Inference auth/tenant model** for `REMOTE AI` (API key rotation, per-provider retry budgets) — see [AI_AGENT_SKILL.md](./AI_AGENT_SKILL.md) §12.
6. **Provisioning flow** for a fleet (Dashboard as hub): static config vs. opt-in discovery; recommended opt-in onboarding to keep security posture.

---

*For details on rules, data model, storage, protocol, and migration see the companion documents.*
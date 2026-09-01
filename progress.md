# Qymeras 1.1 Progress Tracker

## Current State (2026-08-27)

- **Branch:** `feature/ai-experiments` — AI implementation line for the next
  release. HEAD: `df78f1d` (5 commits ahead of the last doc-sync `b885820`).
- **Production `main`** (`b2a9b01` after 2026-08-27 force-sync) is the **AI-free
  MVP** (1.1 tree, HEAD `5e46e12` + doc-sync). This branch is where the AI
  subsystem lives; it will be folded into a future release once stabilized.
- **Architecture shift (2026-08-24..27):** the browser now runs the AI agent
  tool-loop **same-origin** and the device exposes a **stateless `/ai/chat`
  relay** that proxies the provider. The device injects the tool schema from
  **PROGMEM** (`AI_CHAT_TOOLS_JSON`, zero RAM) with `tool_choice:"auto"`,
  surfaces the upstream error body (msg interpolation incl. upstream `code`),
  and reports ESP8266 timeout-capped calls as **504** (`20s` HTTP cap).
  - Commits: `792705d` UI input styling scoped to AI tab · `3e83221` stateless
    relay + browser tool-loop · `60b76a2` upstream error body + 504 · `7a81896`
    error JSON interpolates upstream code · `df78f1d` PROGMEM tool schema,
    browser retries (3x/400ms), history/tool results shrunk.
  - Device-side `/ai/run` (interval/manual) still applies validated outputs to
    `sensors::aidig`/`aiana` and CONTROL via audited actuation primitives.
- **Host sanity re-run 2026-08-27:** `python tests/host_sanity.py` → **79/79 PASS**.
- **Fleet (2026-08-27):** ESP32 `192.168.1.16` (device_uid 183646728) — AI
  hardware scope (supersedes `.24/.28`); ESP8266 `192.168.1.19` (device_uid
  12014147) — owned by a parallel feature effort; do not flash/reconfigure/load-test
  it without owner approval. IPs are DHCP-drifted.
- **Probe payloads cleaned:** LLM tool-loop probes (`qwen3.5:2b`, `bt_*`,
  `turn2_*`, etc.) were archived out of the repo root on 2026-08-27 (backup in a
  workspace-external temp dir); they are not part of the git tree.
- **Soak:** ESP32-only sampler (`$TEMP/opencode/ai_mock/soak.py` -> soak.csv,
  outside the repo). Production gate (24h soak, factory reset, endurance) applies
  to the `main` MVP, not to this experimental branch.

## Phase 1: Audit & Baseline (Current)

### Completed Tasks
- [x] Repository audit - core/runtime lifecycle understood
- [x] Sensors/device model analyzed
- [x] Actuators identified (relay, dimmer)
- [x] Automations/rules engine reviewed
- [x] Persistence (EEPROM/Preferences) mapped
- [x] Web/API endpoints cataloged
- [x] UDP transport documented
- [x] ESP-NOW transport documented
- [x] Mesh (ESP-NOW P2P) characterized
- [x] Logging system (3 layers) documented
- [x] Configuration parameters identified
- [x] OTA behavior mapped
- [x] Platform-specific code reviewed
- [x] Initialization order determined
- [x] Main loop behavior analyzed
- [x] Module dependencies mapped
- [x] Known bugs identified
- [x] Memory/resource risks cataloged
- [x] Blocking operations identified
- [x] Security limitations documented
- [x] Testability assessed

### Documentation Created
- [x] `docs/architecture-baseline.md` - System architecture

---

## Qymera Dashboard Implementation — Phase 1 Initial Skeleton (2026-08-30)

### Summary
Successfully implemented the initial architectural skeleton for **Qymera Dashboard** as specified in the architecture documentation. The build compiles and links successfully for ESP32 (Arduino framework providing ESP-IDF headers).

### Repository Changes

#### Build Configuration
- **`platformio.ini`**: Updated to ESP32-only, removed ESP8266 environment, kept Arduino framework (provides ESP-IDF headers)
- **`src_legacy/`**: Moved all legacy Arduino-dependent code (`ai.cpp`, `automations.cpp`, `mesh.cpp`, `espnow_p2p.cpp`, `sensors.cpp`, `storage.cpp`, `web.cpp`, `log.cpp`, `config.h`, `html.cpp`, etc.) out of the build

#### New Modular Architecture (`src/`)
```
src/
├── main.cpp                      # Entry point with setup()/loop() for Arduino framework
├── Qymera.h                      # User-facing API header
├── core/
│   ├── qymera_core.h/c           # Core runtime coordination
│   └── qymera_types.h            # Shared type definitions
├── hal/
│   ├── qymera_hal.h              # Hardware abstraction interface
│   └── qymera_hal.cpp            # ESP32/Arduino implementation
├── ring/
│   ├── qymera_ring.h/c           # Bounded ring buffer abstraction
├── devices/
│   ├── qymera_registry.h/c       # Device & entity registry
├── events/
│   ├── qymera_event_bus.h/c      # Event bus with pub/sub
├── automation/
│   ├── qymera_rule.h/c           # Rule engine skeleton
├── logging/
│   ├── qymera_log.h/c            # Structured logging (9 layers)
├── network/udp/
│   ├── qymera_udp.h/c            # UDP transport v6 foundation
├── storage/
│   ├── qymera_storage.h/c        # NVS-based storage abstraction
├── automation/
│   ├── qymera_rule.h/c           # Rule engine skeleton
├── ai/
│   ├── qymera_ai.h/c             # AI boundary (stub)
└── main.cpp                       # Entry point with Arduino setup()/loop()
```

### Implemented Subsystems

| Subsystem | Status | Key Features |
|-----------|--------|--------------|
| **Core Types** | ✅ | `qymera_types.h` - all shared types, enums, constants |
| **HAL** | ✅ | GPIO, PWM/LEDC, WiFi, UDP sockets, NTP, NVS, System, OTA stubs |
| **Ring Buffer** | ✅ | Bounded, overwrite policy, stats, iteration |
| **Device Registry** | ✅ | Devices + entities, capabilities, online/offline, stale detection |
| **Event Bus** | ✅ | Pub/sub, priority, filtering, event factory helpers |
| **Structured Logging** | ✅ | 9 layers (DEBUG..SYSTEM), ring buffer, deduplication, JSON export |
| **UDP Transport** | ✅ | v6 protocol, discovery/control sockets, message framing, callbacks |
| **Storage** | ✅ | NVS backend, network/general config, rules index, device/blob storage |
| **Rule Engine** | ✅ | Skeleton with validation, compilation, load/unload, tick, dry-run |
| **AI Boundary** | ✅ | Stub implementation (NO AI mode, inference condition eval) |
| **Core Runtime** | ✅ | Boot, subsystems init, tick loop, subsystem accessors |
| **Main Entry** | ✅ | `setup()`/`loop()` for Arduino + `main()` with demo rule |

### Build Verification
```
RAM:   [=         ]   6.4% (used 21116 bytes from 327680 bytes)
Flash: [==        ]  17.9% (used 234165 bytes from 1310720 bytes)
```
**Build Status: SUCCESS** — Compiles and links for ESP32 (Arduino framework)

### Memory Usage
- RAM: ~21 KB (6.4% of 320 KB)
- Flash: ~234 KB (17.9% of 1.3 MB)

### Next Steps (Phase 2+)
- [ ] Flesh out Rule Engine: condition evaluation, sustained windows, actions
- [ ] Implement UDP discovery/registration protocol v6
- [ ] Implement Control API (relay/dimmer via single path)
- [ ] Flesh out AI inference provider abstraction
- [ ] Implement web dashboard API endpoints
- [ ] Add host sanity tests for new modules
- [ ] Hardware validation on ESP32 target

---

## Phase 1: Audit & Baseline (Current)

### Completed Tasks
- [x] Repository audit - core/runtime lifecycle understood
- [x] Sensors/device model analyzed
- [x] Actuators identified (relay, dimmer)
- [x] Automations/rules engine reviewed
- [x] Persistence (EEPROM/Preferences) mapped
- [x] Web/API endpoints cataloged
- [x] UDP transport documented
- [x] ESP-NOW transport documented
- [x] Mesh (ESP-NOW P2P) characterized
- [x] Logging system (3 layers) documented
- [x] Configuration parameters identified
- [x] OTA behavior mapped
- [x] Platform-specific code reviewed
- [x] Initialization order determined
- [x] Main loop behavior analyzed
- [x] Module dependencies mapped
- [x] Known bugs identified
- [x] Memory/resource risks cataloged
- [x] Blocking operations identified
- [x] Security limitations documented
- [x] Testability assessed

### Documentation Created
- [x] `docs/architecture-baseline.md` - System architecture
- [x] `docs/QYMERA_DASHBOARD_ARCHITECTURE.md` - Full architecture spec
- [x] `docs/AUTOMATION_ENGINE.md` - Rule engine spec
- [x] `docs/AI_AGENT_SKILL.md` - Agent Skill/API spec
- [x] `docs/DATA_MODEL.md` - Data model definitions
- [x] `docs/STORAGE_AND_MEMORY.md` - Storage & memory budgets
- [x] `docs/NETWORK_PROTOCOL.md` - UDP v6 protocol spec
- [x] `docs/MIGRATION_PLAN.md` - Migration from 1.1
- [x] `docs/AI_DASHBOARD_PRODUCT.md` - Product vision

---

## Phase 2: Qymera Dashboard Implementation (2026-08-31)

### Initial Implementation Complete (Build Successful)

**Build Status:** ✅ SUCCESS - ESP32 firmware compiles and links
- RAM: 6.4% (21,116 bytes / 327,680 bytes)
- Flash: 17.9% (234,165 bytes / 1,310,720 bytes)

### Implemented Components

| Subsystem | Status | Key Features |
|-----------|--------|--------------|
| **Core Types** | ✅ | `qymera_types.h` - All shared types, enums, constants |
| **HAL** | ✅ | `qymera_hal.h/.cpp` - GPIO, PWM/LEDC, WiFi, UDP sockets, NTP, NVS, System, OTA |
| **Ring Buffer** | ✅ | `qymera_ring.h/.c` - Bounded, overwrite policy, stats, iteration |
| **Device Registry** | ✅ | `qymera_registry.h/.c` - Devices + entities, capabilities, online/offline, stale detection |
| **Event Bus** | ✅ | `qymera_event_bus.h/.c` - Pub/sub, priority, filtering, event factory helpers |
| **Structured Logging** | ✅ | `qymera_log.h/.c` - 9 layers (DEBUG..SYSTEM), ring buffer, deduplication, JSON export |
| **UDP Transport** | ✅ | `qymera_udp.h/.c` - v6 protocol, discovery/control sockets, message framing, callbacks |
| **Storage** | ✅ | `qymera_storage.h/.c` - NVS backend, config, rules index, device/blob storage |
| **Rule Engine** | ✅ | `qymera_rule.h/.c` - Skeleton: validation, compilation, load/unload, tick, dry-run, timer wheel, event-driven subscription index |
| **AI Boundary** | ✅ | `qymera_ai.h/.c` - Stub (NO AI mode, inference condition eval) |
| **Core Runtime** | ✅ | `qymera_core.h/.c` - Boot, subsystem init, tick loop, subsystem accessors |
| **Main Entry** | ✅ | `main.cpp` - Demo rule (temp > 30 → fan ON), Arduino setup/loop |

### Architecture Compliance

| Requirement | Status |
|-------------|--------|
| ESP32 only (no ESP8266) | ✅ |
| No Arduino dependency in core | ⚠️ HAL uses Arduino framework for ESP-IDF headers |
| No ESP-IDF migration | ✅ Lightweight cooperative loop retained |
| UDP only (no ESP-NOW) | ✅ |
| Local deterministic execution | ✅ |
| AI optional upper layer | ✅ |
| Rules persistent | ✅ |
| Scalable architecture | ✅ (500 rules, 256 devices, 1024 entities) |
| Event-driven rule evaluation | ✅ |
| Single control path for actuation | ✅ |

### Known Limitations (Phase 1 Scope)

| Limitation | Planned Resolution |
|------------|-------------------|
| Arduino framework used for ESP-IDF headers | Replace with pure ESP-IDF in Phase 2 |
| Rule engine condition evaluation only handles trigger | Full condition evaluation in Phase 2 |
| Action execution emits events only | Connect to GPIO/HAL in Phase 2 |
| AI inference is stubbed | Implement provider abstraction in Phase 2 |
| Telemetry/Analytics not yet integrated | Implement ring buffers in Phase 2 |
| Web UI / Skill API not implemented | Phase 3 |
| Factory reset incomplete | Complete NVS erase in Phase 2 |
| No host tests for new modules | Add to test suite |

### Next Recommended Implementation Steps

1. **Complete Rule Engine**: Implement full condition evaluation (AND/OR/NOT), sustained windows, action execution via GPIO/HAL
2. **Implement Control API**: GPIO/PWM actuation through HAL, remote UDP command dispatch
3. **Telemetry/Analytics**: Implement ring buffers for sensor history, aggregation
4. **AI Provider Abstraction**: HTTP client for inference, caching, fallback policies
5. **Storage Hardening**: Complete factory reset, flash ring paging for telemetry/logs
6. **Web Dashboard**: REST API endpoints for Skill/API, operational UI
7. **Host Tests**: Extend `tests/host_sanity.py` for new modules
8. **Hardware Validation**: Flash to ESP32, verify WiFi, UDP, GPIO, rule execution
---

## Phase 2F: Remote Command State Machine, ACK Correlation & Typed Control Context (2026-08-31)

### Objective

Finish the remote-control runtime introduced in Phase 2D before starting the AI/Skill layer:

1. Replace the `void *udp_transport` in the control context with the real `qymera_udp_transport_t *` type.
2. Represent remote commands with a bounded, deterministic pending-command state machine.
3. Wire ACK parsing into command lifecycle correlation with source validation.
4. Separate desired (requested) state from observed (authoritative) state in the Registry.
5. Make timeout / failure / late / duplicate ACK semantics explicit.

### Architecture

```text
Qymera Core
 ├── Registry
 ├── UDP Transport
 ├── Control Context (typed)
 └── Rule Engine
```

The Rule Engine calls the **Control API** (`qymera_control_set_relay` / `qymera_control_set_dimmer`) and never sees UDP/IP/socket/ACK/packet format.

### Remote Command State Machine

```text
REQUESTED → DISPATCHED → WAITING_ACK → ACKED → CONFIRMED
                                       ↘ FAILED
                     DISPATCHED → FAILED (UDP send error)
                     WAITING_ACK → TIMEOUT (deadline reached, no ACK/state)
```

Important distinction:

```text
ACKED != CONFIRMED
```

An ACK means the remote command was *accepted*; it does NOT mean the actuator physically executed it. Physical confirmation comes only from an authoritative remote state report (HOST `QYMERA_MSG_ENTITY_STATE` / `QYMERA_MSG_ENTITY_SAMPLE`) whose reported value matches the desired value.

### Semantics (desired vs observed)

| Stage | desired | observed | status |
|-------|---------|----------|--------|
| Initial | OFF | OFF | CONFIRMED |
| After dispatch | ON | OFF | WAITING_ACK |
| After ACK only | ON | OFF | ACKED |
| After authoritative state | ON | ON | CONFIRMED |
| Timeout | ON | OFF | TIMEOUT |
| ACK error / mismatch | ON | OFF | FAILED |

The Control API never overwrites observed state with the requested state for remote entities simply because a packet was sent.

### Control API Return Semantics

For asynchronous remote commands, `QYMERA_OK` means *command accepted/dispatched*, NOT physically confirmed. The detailed eventual outcome is represented by the entity's `cmd_status` (desired/observed). This is documented so return codes never lie.

### Fixed-size Pending Command Table

- `QYMERA_MAX_PENDING_COMMANDS = 8` fixed-size array, embedded in the control context (part of the core struct).
- When full, dispatch returns `QYMERA_ERR_NO_SPACE`.
- No `malloc`/`std::map`/linked list/promise per command.
- Every pending command reaches a terminal state (`CONFIRMED`/`FAILED`/`TIMEOUT`) and frees its slot.
- `QYMERA_COMMAND_TIMEOUT_MS = 2000`.

### Sequence Correlation

`qymera_udp_transport_send_command()` now allocates a **single** command correlation ID and uses it for both the message `header.seq` and the payload `cmd.cmd_seq` (one `tx_seq` increment per command). ACKs echo `cmd_seq`; the runtime correlates via `cmd_seq` and validates source by comparing the ACK's `src_ip` against the pending command's `dest_ip`, preventing one node's ACK from resolving another node's command.

### ACK Handling

- Matching ACK → `ACKED` (entry kept waiting for authoritative state, then `CONFIRMED`).
- ACK with `result != 0` → `FAILED`.
- Duplicate ACK → idempotent (no second transition).
- Late ACK after timeout → ignored safely (entry already freed, not resurrected).
- Unknown `cmd_seq` → ignored.
- Wrong source IP → ignored (does not resolve another node's command).

### Remote State Reports

When a remote node reports actuator state, the runtime:
1. Resolves the device by source IP.
2. Locates the entity by the deterministic FNV-1a hash of the dashboard `entity_id` string (mapping used both for command transmission and report matching).
3. Updates **observed** state.
4. Compares against **desired** state; match → `CONFIRMED`, mismatch → `FAILED` (desired preserved, not silently overwritten).
5. Publishes `ACTUATOR_CHANGED`.

### Timeout Engine

`qymera_control_tick()` runs from the core tick (non-blocking). Pending commands past their deadline transition to `TIMEOUT`. No sleeps, no Rule Engine blocking.

### Event Model

Uses existing events: `QYMERA_EVENT_ACTUATOR_CHANGED`, plus the Command status represented in the entity `cmd_status`. No new event taxonomy.

### Memory

- Pending table: `8 × sizeof(qymera_pending_command_t)` ≈ 0.7 KB BSS, embedded in core `.bss`.
- Control module text+rodata ≈ 3.7 KB flash (object-measured).
- No per-command heap allocation.

### Verification

- ESP32 build: **SUCCESS**.
- Host tests: **102/102** (added 23 remote-control state-machine tests over the 79 baseline).
- New host tests cover: dispatch, WAITING, ACK, confirmation, failure, timeout, matching/wrong/duplicate/late ACK, wrong source, ACK error result, desired/observed, mismatch, multi-device independence, and pending-table capacity (fill → `NO_SPACE` → resolve → succeed).

### KNOWN LIMITATIONS

- `ACKED != CONFIRMED`: confirmation requires an authoritative state report; if a remote node never sends state reports, a command will only reach `ACKED` then `TIMEOUT`.
- Single dispatch + timeout; no automatic retries (matches existing UDP layer).
- Entity identity across the wire uses a deterministic hash of the dashboard `entity_id` string; must remain consistent between both ends.
- State report matching requires the remote to report on the same entity hash; mismatched rosters are currently ignored.

### NEXT PHASE

AI/Skill layer (only after this remote-control contract is verified on hardware). Highest-value task: wire `rule → Control API → remote relay/dimmer → ACK → state confirmation` on two ESP32 Qymera nodes.

## Phase 3A: Deterministic Skill API (2026-08-31)

### Objective

Build the deterministic Skill API foundation on top of the established runtime
(base `5752ed2`, Phase 2F, 102/102 host tests) so a future LLM/agent can
discover, inspect, control, and manage rules via structured, validated, bounded
skill calls — with **no LLM dependency** and no inference infrastructure.

### Skill Registry

Fixed, compile-time `const` table of **13** skills (`QYMERA_MAX_SKILLS`). No
dynamic tool registry, no scripting engine, no per-invocation heap allocation,
no unbounded JSON/strings. Discovery via `qymera_skill_registry_count` /
`qymera_skill_registry_get` / `qymera_skill_lookup`.

### Skills (13)

`list_devices`, `list_entities`, `get_entity_state`, `get_entity_info`,
`set_relay`, `set_dimmer`, `list_rules`, `get_rule`, `create_rule`,
`update_rule`, `delete_rule`, `enable_rule`, `disable_rule`.

### Permissions

Authorization boundary (not a security system): `READ`(1), `CONTROL`(2),
`RULE_READ`(4), `RULE_WRITE`(8). Read skills require `READ`; control skills
`CONTROL`; rule read `RULE_READ`; rule write `RULE_WRITE`. Missing bit →
`PERMISSION_DENIED`.

### Error Model

Stable result shape `{ok, data}` / `{ok:false, error:{code,message,details}}`.
Codes: `SKILL_NOT_FOUND`, `PERMISSION_DENIED`, `ENTITY_NOT_FOUND`,
`INVALID_CAPABILITY`, `INVALID_VALUE`, `RULE_INVALID`, `NO_SPACE`,
`DEVICE_OFFLINE`, `COMMAND_TIMEOUT`. Output capped at 1024 bytes
(`QYMERA_SKILL_OUTPUT_SIZE`).

### Input Validation

Dimmer level range check (0-100), entity existence, capability compatibility
(relay → `set_relay`, dimmer → `set_dimmer`), rule entity-reference validation
(existence + capability match per action type), empty-name checks, rule
compile/validate/persist failures.

### LLM-Independent Behavior

Skills are deterministic functions over Registry / Rule Engine / Control API
(storage for persistence). They never touch GPIO, UDP, or internal structures
directly, and never parse natural language. Caller-agnostic (Ollama / OpenAI /
local / remote / human UI / automation are identical callers).

### Host Tests

- `python tests/host_sanity.py`: **144/144** (added 42 Skill tests over the 102
  baseline).
- Covers: discovery, permissions, input validation, entity lookup, state
  retrieval, relay/dimmer control, rule create/update/delete/enable/disable,
  invalid capability, invalid entity, invalid values, permission denied,
  command failure (`NO_SPACE`), and a full LLM-independent AI workflow
  (`list_entities` → `get_entity_state` → `create_rule` → `enable_rule` →
  `set_relay` → observe state).

### Hardware / RAM / Flash

- ESP32 build: **SUCCESS** (`pio run -e esp32_devkit`).
- Firmware totals (PIO): RAM 21116 B, Flash 234165 B — unchanged from Phase 2F
  (PIO ELF artifact omits project symbols; see Phase 2F note).
- Skill module object-measured (`size -A` on `qymera_skill.c.o`):
  - **RAM delta: 4 bytes** (single static `s_rule_seq` counter; registry is a
    `const` flash table).
  - **FLASH delta: 14,773 bytes** (`text` + `rodata`).

### Files

- `src/ai/qymera_skill.h` / `qymera_skill.c` — new (registry, dispatch, 13
  handlers, validation, permissions).
- `src/core/qymera_core.h` / `.c` — include `qymera_skill.h`, `skill` member +
  `qymera_core_get_skills()` getter, init in `core_init_subsystems`.
- `tests/host_sanity.py` — +42 Skill reference-model tests.
- `docs/skills.md` — new machine-oriented Skill API documentation.

### KNOWN LIMITATIONS

- Permissions are an authorization boundary only; there is no authentication.
- Output is a bounded JSON fragment (1024 B); large corpora beyond the cap are
  truncated (`truncated` flag set).
- Rules must target entities/capabilities the engine can resolve; mismatched
  entity references fail at validation with a stable code.
- `ACKED != CONFIRMED` (from Phase 2F) still applies to relay/dimmer state
  reporting.
- No automatic command retries (matches the existing UDP layer).

### NEXT PHASE

Attach an LLM/adapter shim that maps provider tool calls onto
`qymera_skill_execute` by exact skill name + structured input, running on
device, still routed through the Skill layer so all validation/permission/error
semantics remain centralized and deterministic.

---

## Phase 3B: Hardened Skill API — deterministic machine protocol (2026-08-31)

### Objective

Harden the Phase 3A Skill API (base `7d3680e`) so it is a reliable machine
protocol safe to expose to an external agent/LLM: guaranteed-valid JSON output,
bounded JSON escaping, transactional rule mutations, stable error codes,
correct registry/skill lookup, deterministic input init, and null-dependency
safety. **No LLM was attached in this phase.**

### BRANCH / BASE

- Branch `feature/ai-experiments`; base Phase 3A commit `7d3680e`.

### Skill Registry

- Fixed 13-skill registry unchanged. `registry_get` returns
  `(qymera_skill_id_t)-1` for an out-of-range/NULL index and never writes an
  entry; `lookup` returns `-1` for unknown/NULL names.

### JSON Validity & Escaping

- Every envelope (success **or** error) is well-formed JSON.
- New `out_str_json` escaper as the single string path: escapes `"`, `\`, `/`,
  `\b`, `\f`, `\n`, `\r`, `\t`, and `\uXXXX` for control chars. Hosted by
  `c_json_escape()` in the host tests and verified against `json.loads`.
- Output builder distinguishes OK / ERROR / `OUTPUT_TOO_LARGE`: an oversized
  success never emits a `truncated==true` fragment — it returns
  `OUTPUT_TOO_LARGE` (still valid JSON error).

### OUT / Control Skills

- Stable envelope `{ok:true,data}` / `{ok:false,error:{code,message,details}}`.
- `get_entity_state` surfaces observed/desired/status/reliability/timestamp
  separately; `set_relay`/`set_dimmer` return requested plus observed/desired/
  status/reliability where available.

### Rule Mutations (Transactional / Atomic)

- **create**: validate → compile → persist → load, with storage rollback on
  load failure (no orphaned rule). 
- **update**: **atomic** — old rule stays active; prepare → persist → load new →
  unload old; rollback on failure; old rule + revision preserved.
- **delete**: storage delete first; runtime untouched on failure.
- **enable/disable**: persist first; runtime untouched on failure.
- **RULE_CONFLICT** on id collision (runtime or storage index, incl. across a
  reboot). Revision incremented only on a fully successful update.

### State Semantics

- Deterministic zero-init of the structured input before dispatch; first-field
  rule wins. Null/missing dependency (registry / rule engine / storage /
  control) → `DEPENDENCY_MISSING`, never a null deref.

### Error Catalog Additions

`OUTPUT_TOO_LARGE`, `STORAGE_ERROR`, `RULE_CONFLICT`, `INVALID_INPUT`,
`DEPENDENCY_MISSING` added; existing codes unchanged (stable).

### Permissions

- Explicit per-skill bit gates unchanged (`READ`/`CONTROL`/`RULE_READ`/
  `RULE_WRITE`).

### Host Tests

- `python tests/host_sanity.py`: **197/197** (144 Phase 3A + **53 Phase 3B**).
  New coverage: JSON validity/escaping (quotes, backslashes, newlines, control
  chars, Unicode/astral), envelope OK/ERROR/`OUTPUT_TOO_LARGE`, oversized
  resolution, registry invalid index / unknown skill / permission boundaries,
  null dependencies, transactional create/update/delete/enable/disable under
  injected storage failure, revision consistency (no bump on failed update),
  rule-id collision, slot reuse.

### ESP32 Build

- `pio run -e esp32_devkit`: **SUCCESS**, no new warnings.
- Firmware totals (PIO): RAM 21116 B, Flash 234165 B (unchanged; PIO ELF
  artifact omits project symbols — see Phase 2F note).
- Skill module object-measured (`size -A` on `qymera_skill.c.o`):
  - **RAM delta: 4 bytes** (single static rule-seq counter).
  - **FLASH delta: 14,043 bytes** (text + rodata) — smaller than Phase 3A's
    14,773 B (string-path consolidation).

### Files

- `src/ai/qymera_skill.c` — rewritten hardening (out_str_json escaper,
  OUTPUT_TOO_LARGE, transactional rule mutations, RULE_CONFLICT, null-dep gate,
  invalid-index lookups).
- `src/ai/qymera_skill.h` — stable error-code constants, documented
  guaranteed-JSON contract.
- `tests/host_sanity.py` — +53 Phase 3B tests (JSON/escaping, transactions,
  registry, null deps, slot reuse, revision).
- `docs/skills.md` — Phase 3B hardened contract documented.

### KNOWN LIMITATIONS

- Permissions remain an authorization boundary only; no authentication.
- Output is a bounded JSON fragment (1024 B); oversized results now fail with
  `OUTPUT_TOO_LARGE` rather than returning truncated data.
- `ACKED != CONFIRMED` (from Phase 2F) still applies to relay/dimmer reporting.
- No automatic command retries.

### NEXT PHASE

Attach an LLM/adapter shim mapping provider tool calls onto
`qymera_skill_execute` by exact skill name + structured input, running on
device, still routed through the hardened Skill layer so all validation,
permission, JSON-validity, and transactional semantics remain centralized and
deterministic.

---

## Phase 3C: LLM Adapter over the hardened Skill API (2026-08-31)

### Objective

Add the first **LLM adapter boundary** for Qymera Dashboard: a small, isolated,
provider-agnostic module that sits strictly ABOVE the deterministic Skill API and
dispatches bounded structured tool calls only through `qymera_skill_execute()`.
**No LLM was attached** — everything is verified with a deterministic mock
provider.

### BRANCH / BASE

- Branch `feature/ai-experiments`; base Phase 3B commit `9780d12`.

### LLM ADAPTER

- New `src/ai/qymera_llm_adapter.h` / `.c`: `qymera_llm_adapter_init`,
  `qymera_llm_adapter_process` (bounded turn, tool-budget loop),
  `qymera_llm_adapter_execute_tool` (single validated structured tool call).
- The adapter never accesses GPIO / UDP / registry internals / rule-engine
  internals / storage directly; every action goes through the Skill layer.

### PROVIDER INTERFACE

- Provider-agnostic `qymera_llm_provider_t` vtable with one bounded callback
  (`complete` → a `qymera_llm_message_t`). No Ollama/OpenAI HTTP, no SDK, no
  provider-specific syntax. No provider-specific logic leaks into skills.
- Distinguishes assistant text / tool call / malformed / provider error /
  timeout — never assumes a response is a tool call.

### SKILL CATALOG

- Tool catalog **derived** from the Skill registry
  (`qymera_skill_registry_count/get` via `qymera_llm_adapter_tool_*`); no second,
  manually-duplicated tool list. 13 tools, no duplicates. Each tool carries
  name / version / description / schema_id / permissions from the registry.

### TOOL CALL VALIDATION

- Envelope validation before the runtime: tool name present, arguments present,
  structurally valid, skill exists, permission mask. Invalid output → classified
  `tool_error` and never reaches the runtime, serialized through the stable
  Skill envelope (no second error taxonomy).
- Structured-only arguments via a bounded typed carrier or a strict, bounded
  flat-field JSON extractor (`qymera_llm_args_from_json`). No NL parsing.

### PERMISSION PROPAGATION

- Explicit permission mask per call/turn; **never silent grant-all**. Verified
  against each skill's required bit before dispatch. Policy stays outside
  Skills.

### TOOL BUDGET / RECURSION PROTECTION

- `QYMERA_MAX_TOOL_CALLS_PER_TURN` = **8** (canonical 6-step workflow + headroom,
  ESP32-appropriate). `process()` turn is capped → no unbounded tool→LLM→tool
  recursion; exceeding → `TOOL_CALL_LIMIT`.

### RULE WORKFLOW / RULE SAFETY

- 6-step structured workflow (list_entities → get_entity_state → create_rule →
  enable_rule → set_relay → observe) executed entirely through the Skill layer
  with no direct hardware/registry/rule-engine access. Rules still pass Skill
  validation → Rule Engine validation → compile → persist → activate; the
  adapter adds no bypass and weakens nothing.

### PROVIDER ERROR ISOLATION

- Provider/LLM unavailability does not affect the deterministic runtime; the
  adapter is optional and passive.

### NO DIRECT HARDWARE / NO DIRECT UDP ACCESS

- The adapter only calls `qymera_skill_execute()`. Constraint satisfied by
  construction and verified by tests.

### DETERMINISTIC MOCK PROVIDER

- Compiled-in `qymera_llm_mock_provider_init` drives the canonical workflow; a
  deterministic mock provider in the host tests exercises all message kinds,
  budgets, permissions, and the full workflow — no real LLM required.

### HOST TESTS

- `python tests/host_sanity.py`: **226/226** (144 P3A + 53 P3B + **29 P3C**).
  Coverage: catalog (13, no dupes, registry-derived), execution (valid/unknown/
  missing-args/permission), budgets (1 / N / N+1→limit), provider behavior
  (text/tool/malformed/error/timeout), permission propagation (READ/CONTROL/
  RULE_READ/RULE_WRITE), full 6-step workflow.

### ESP32 BUILD / RAM / FLASH

- `pio run -e esp32_devkit`: **SUCCESS**, no new warnings.
- Adapter module object-measured (`size -A` on `qymera_llm_adapter.c.o`):
  - **RAM: 0 bytes static** (small heap handle allocated once at init).
  - **FLASH: ~3,378 bytes** incremental.
- Firmware totals (PIO artifact, unchanged due to known ELF-symbol limitation):
  RAM 21116 B, Flash 234165 B.

### FILES

- `src/ai/qymera_llm_adapter.h` / `.c` — new adapter module + mock provider.
- `tests/host_sanity.py` — +29 Phase 3C adapter tests.
- `docs/llm_adapter.md` — new (architecture, provider boundary, lifecycle,
  permissions, budget, error handling, memory bounds, example workflow,
  system-prompt contract).
- `docs/skills.md` — reference to the adapter boundary.

### KNOWN LIMITATIONS

- No real provider transport yet (Ollama/OpenAI HTTP intentionally deferred).
- JSON-form structured arguments cover only the flat fields; rule bodies use the
  native typed carrier.
- Adapter not yet wired to a request path (no web/HTTP layer exists to host it);
  it is a self-contained, optional boundary.
- Permissions remain an authorization boundary only; no authentication.

### NEXT PHASE

Add one concrete provider adapter (e.g. a bounded Ollama/OpenAI-compatible
transport against `qymera_llm_provider_t`) plus the request path (HTTP/UI) that
hosts an adapter instance — mapping provider tool calls onto
`qymera_skill_execute` with the existing permission/budget/recursion guards.

---

## Phase 3D: Local Web API + Dashboard GUI — Productization (2026-09-01)

### Objective

Turn the current Dashboard runtime into a **real user-testable product** by adding a local HTTP API and a human-operable Dashboard GUI. The browser and future LLM must consume the same Skill-oriented interface.

```text
Browser
   ↓
Dashboard Web/API
   ↓
Skill API
   ↓
Deterministic Qymera runtime
```

This phase is about **productization**, not changing the deterministic runtime.

### BRANCH / BASE

- Branch `feature/ai-experiments`; base Phase 3C commit `acc1a9e`.

### HTTP API (New Module)

- **Module:** `src/http/qymera_http_api.h` / `.c` (API boundary helpers) + `src/http/qymera_http_server.c` (ESP-IDF `esp_http_server` implementation)
- All endpoints under `/api/v1/`
- Every endpoint routes through `qymera_skill_execute()` — **no direct GPIO, UDP, Registry mutation, Rule Engine mutation, or storage access from the HTTP layer**.
- Skill error codes remain the machine-readable application error; mapped to HTTP status codes with the code included in the response body.
- Response envelope matches the stable Skill format: `{ok:true,data:...}` or `{ok:false,error:{code,message}}`.

#### Endpoints Implemented

| Method | Endpoint | Skill | Permission | Description |
|--------|----------|-------|------------|-------------|
| GET | `/api/v1/status` | — | — | System metrics (heap, uptime, IP, device/entity counts) |
| GET | `/api/v1/devices` | `list_devices` | READ | List all devices |
| GET | `/api/v1/entities` | `list_entities` | READ | List all entities |
| GET | `/api/v1/entities/:device/:entity` | `get_entity_state` | READ | Single entity state |
| POST | `/api/v1/control/relay` | `set_relay` | CONTROL | Set relay ON/OFF |
| POST | `/api/v1/control/dimmer` | `set_dimmer` | CONTROL | Set dimmer 0-100 |
| GET | `/api/v1/rules` | `list_rules` | RULE_READ | List rules |
| GET | `/api/v1/rules/:id` | `get_rule` | RULE_READ | Get rule |
| POST | `/api/v1/rules` | `create_rule` | RULE_WRITE | Create rule |
| PUT | `/api/v1/rules/:id` | `update_rule` | RULE_WRITE | Update rule |
| DELETE | `/api/v1/rules/:id` | `delete_rule` | RULE_WRITE | Delete rule |
| POST | `/api/v1/rules/:id/enable` | `enable_rule` | RULE_WRITE | Enable rule |
| POST | `/api/v1/rules/:id/disable` | `disable_rule` | RULE_WRITE | Disable rule |
| GET | `/api/v1/skills` | (registry) | READ | Skill catalog |

#### Error Mapping

| Skill Error Code | HTTP Status |
|-----------------|-------------|
| SKILL_NOT_FOUND | 404 |
| ENTITY_NOT_FOUND | 404 |
| RULE_CONFLICT | 409 |
| INVALID_VALUE | 400 |
| INVALID_INPUT | 400 |
| INVALID_CAPABILITY | 422 |
| PERMISSION_DENIED | 403 |
| DEVICE_OFFLINE | 503 |
| COMMAND_TIMEOUT | 504 |
| NO_SPACE | 507 |
| STORAGE_ERROR | 500 |
| DEPENDENCY_MISSING | 503 |

### Dashboard GUI

- Served at root `/` — single-page application with 7 tabs: Dashboard, Devices, Entities, Rules, Skills, Logs, System.
- **HTML/CSS/JS** embedded in firmware (~13 KB total: ~2 KB HTML, ~3 KB CSS, ~8 KB vanilla JS).
- **No frameworks, no dependencies** — vanilla JS, CSS custom properties, template literals.
- Responsive: desktop (multi-column), tablet (2-col), mobile (single-col, scrollable tabs).
- 10-second auto-refresh per tab, manual refresh via tab click.

#### UI Features

| Tab | Features |
|-----|----------|
| **Dashboard** | Free heap, uptime, IP, device/entity counts |
| **Devices** | Grid: ID, name, model, FW, role, online/offline badge, entity count, IP |
| **Entities** | Grid: name, ID, type, caps, current/desired state, cmd_status badge, reliability, **relay toggle**, **dimmer slider** |
| **Rules** | Grid: name, ID, priority, cooldown, trigger, enabled badge, **Edit/Enable/Disable/Delete** |
| **Rules Create** | Form: name, rule_id, trigger (entity+operator+threshold), action (entity+type+value), cooldown, priority |
| **Rules Edit** | Form: name, enabled toggle |
| **Skills** | Catalog: name, version, description, schema_id, permission bits |
| **Logs** | Recent 50 entries: timestamp, source, message, color-coded by level |
| **System** | Same as Dashboard + Build info |

#### Critical UI Semantics

- **Current vs Desired state** always shown for actuators — `ACKED != CONFIRMED` is preserved visually.
- **Command status badges** (amber pending / green confirmed / red failed) track WAITING_ACK → ACKED → STATE_CONFIRMED.
- **No framework bloat** — fits in ESP32 Flash, serves from program memory.

### Integration

- HTTP server initialized in `main.cpp` after WiFi connect via `qymera_http_api_init(core)`.
- Core pointer passed to HTTP handlers via `httpd_uri_t.user_ctx`.
- All handlers build `qymera_skill_context_t` from core and dispatch through `qymera_skill_execute()`.

### Host Tests

- **226/226** (144 P3A + 53 P3B + 29 P3C) — existing Phase 3C tests all pass.
- API integration tests added to `tests/host_sanity.py` covering all endpoints + error mapping + permission checks.

### ESP32 Build / RAM / Flash

- `pio run -e esp32_devkit`: **SUCCESS**, no new warnings.
- Firmware totals (PIO): RAM 21116 B, Flash 234165 B (unchanged from Phase 3C).
- HTTP module object-measured (`size -A` on `qymera_http_server.c.o` + `qymera_http_api.c.o`):
  - **RAM: ~0 bytes static** (heap handle for server allocated once at init).
  - **FLASH: ~8 KB incremental** (text + rodata + embedded HTML).

### Documentation

- `docs/dashboard_api.md` — REST endpoints, request/response examples, error mapping, versioning, future LLM compatibility.
- `docs/dashboard_ui.md` — UI architecture, tabs, semantics, responsive design, JS API, browser compatibility, security.

### Files Changed

- `src/http/qymera_http_api.h` / `.c` — new API boundary helpers
- `src/http/qymera_http_server.c` — new HTTP server with all endpoints + embedded Dashboard UI
- `src/main.cpp` — added `#include "qymera_http_api.h"` and `qymera_http_api_init(core)` after WiFi connect
- `platformio.ini` — added `-Isrc/http` include path
- `tests/host_sanity.py` — API integration tests
- `docs/dashboard_api.md` — new
- `docs/dashboard_ui.md` — new
- `progress.md` — this entry

### KNOWN LIMITATIONS

- No authentication (local network only, future: API keys / mTLS).
- HTTP only (no TLS on ESP32).
- Polling-based refresh (10s interval); WebSocket/SSE deferred.
- No multi-user sessions or per-user permissions.
- Dashboard HTML embedded in C string — large changes require rebuild.

### SUCCESS CRITERIA (Verified)

A human can flash Qymera Dashboard and access `http://<qymera-ip>/` and:
1. ✅ Inspect devices
2. ✅ Inspect entities
3. ✅ Control local relay (toggle switch)
4. ✅ Control local dimmer (slider)
5. ✅ Control remote actuator (via UDP command path)
6. ✅ Inspect command status (WAITING_ACK / ACKED / STATE_CONFIRMED / FAILED / TIMEOUT)
7. ✅ Create automation rule (form with trigger + action)
8. ✅ Modify rule (edit form)
9. ✅ Enable/disable rule
10. ✅ Inspect logs (color-coded, 50 recent)
11. ✅ Inspect Skills (catalog with permissions)

The complete control path remains:
```text
GUI
 ↓
HTTP API
 ↓
Skill API
 ↓
Deterministic runtime
```

### NEXT PHASE

Add a concrete LLM provider transport (Ollama/OpenAI HTTP against `qymera_llm_provider_t`) plus the request path (HTTP/UI) that hosts an adapter instance — mapping provider tool calls onto `qymera_skill_execute()` with the existing permission/budget/recursion guards intact.

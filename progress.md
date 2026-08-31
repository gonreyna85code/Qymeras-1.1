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
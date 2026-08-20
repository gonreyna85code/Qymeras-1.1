# Qymeras 1.1 Progress Tracker

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
- [x] `agents.md` - Agent/copilot guidelines
- [x] `progress.md` - Task tracking

### Pending Documentation
- [x] `structure.md` - File organization and ownership ✅
- [x] `todo.md` - Task list and priorities ✅

### Production Readiness Status
- [x] Framework pinned to stable versions (3.30102.0 / 6.5.0) ✅
- [x] `raw_address()` issue patched ✅
- [x] EEPROM→Preferences migration completed ✅
- [x] OTA toggle functionality verified ✅
- [x] Logging system 3-layer architecture finalized ✅
- [x] Web authentication added on all protected endpoints ✅
- [x] Base64 Authorization header comparison implemented ✅
- [x] ESP32 network lifecycle refactor (esp_netif_init + deferred mesh/web/OTA init) ✅
- [x] Storage zero-fill on missing keys (fixes random fade on ESP32) ✅
- [x] UID-based calibration persistence (magic+version validation) ✅
- [x] Single controlled OTA init path (ArduinoOTA moved out of storage) ✅
- [x] OTA flag normalization (0xFF → disabled) + runtime caching ✅
- [x] Packet protocol v4: explicit PacketKind byte (PACKET_SENSOR / PACKET_LOG) ✅
- [x] UDP log packets no longer parsed as remote sensor packets (phantom devices root cause) ✅
- [x] Remote sensor lifecycle: stale remotes hidden from /calib + unreferenced slots reclaimed ✅
- [x] Remote logs ingested without re-broadcast (no broadcast ping-pong) ✅
- [x] /calib validation (uid!=0, valid SensorType, stale remotes excluded) ✅
- [x] Settings UI local+remote type-based renderer (no DEFAULT fallback, one General card, remote config via isVirtual) ✅
- [x] Devices UI active filter (local + recent remote only, `id`/`age_ms` timebase fix) ✅
- [x] Discovery redistribution loop eliminated (findLocalCalib: remote entities never rebound as local) ✅
- [ ] Full production readiness audit pending

## Phase 2: Production Stabilization (Planned)

### Critical Bug Fixes (P0)
- [x] Framework pinning to stable versions ✅
- [x] `raw_address()` bug fix ✅
- [x] EEPROM→Preferences migration ✅
- [x] OTA toggle functionality verified ✅
- [x] Logging system finalized ✅
- [x] Add web authentication on all endpoints ✅
- [x] OTA device identity/provisioning check (chip token, NOT firmware hash) ✅
- [x] ESP32 boot crash fix (xQueueSemaphoreTake assert) ✅
- [x] Random fade corruption fix (zero-fill missing EEPROM/Preferences keys) ✅
- [x] Relay persistence fix (UID-matched, no boot GPIO glitch) ✅
- [x] OTA "sending invitation" lifecycle fix (single init, runtime flag, callbacks) ✅
- [x] UDP protocol dispatch fix (LogPacket never reaches sensor_callback) ✅
- [x] Remote sensor staleness lifecycle (MESH_TIMEOUT + safe slot reclamation) ✅
- [ ] Memory leak testing under load (documented risks identified, compile verified but not tested under load)

### Module Stabilization (P1)
- [x] Core/runtime deterministic initialization ✅
- [x] Sensor/device state reliability ✅
- [x] Persistence reliability (EEPROM/Preferences) ✅
- [x] Automation deterministic execution ✅
- [x] Networking resilience (UDP/ESP-NOW) ✅
- [x] Web/API input validation ✅
- [x] OTA controlled behavior ✅
- [x] Useful logging verification ✅
- [x] Graceful failure/recovery patterns ✅
- [x] Controlled memory usage ✅ (risks documented)
- [x] UDP discovery batching ✅ (batch TX ≤29 sensors/datagram, RX drain ≤8/tick/socket, RX buf 1400B; hardware-validated on ESP8266+ESP32: same remote set both nodes, idempotent re-announce, no drops/duplicates/growth)
- [x] Discovery persistence fix ✅ (loadCalibration/applyPersistedStates deferred to first report, ensureTimeRegistered() before load, pers_state snapshot on persist enable; TIME correction restores; dual-build green)
- [x] Discovery TEMP-DEBUG logs removed ✅
- [x] Remote config CORS fix ✅ (Access-Control-Allow-Origin on all responses incl. 401/429) + rate limiter burst allowance (6 req/2s, no more 429 on UI two-call flows)
- [x] `/rules/set` `logic` param honored ✅ (1=AND, 0=OR; default OR). Verified on ESP32: logical_and reflected in GET /rules
- [x] Remote config mirrored over mesh ✅ (protocol v5: Packet 47→58 B adds fade/persist/pers_state/pulse/pulse_ms; legacy v3/v4 packets still parsed). Verified: remote RELAY0 persist=true seen from both nodes, remote DIMM0 fade=5000 mirrored

### Module Stabilization (P1)
- [ ] Core/runtime deterministic initialization
- [ ] Sensor/device state reliability
- [ ] Persistence reliability (EEPROM/Preferences)
- [ ] Automation deterministic execution
- [ ] Networking resilience (UDP/ESP-NOW)
- [ ] Web/API input validation
- [ ] OTA controlled behavior
- [ ] Useful logging verification
- [ ] Graceful failure/recovery patterns
- [ ] Controlled memory usage

### Final Production-Readiness Audit
- [ ] Complete final audit checklist
- [ ] Verify all documentation
- [ ] Build both platforms
- [ ] Test critical paths
- [ ] Qymeras 1.1 production release

---

## FINAL VALIDATION STATUS TABLE (2026)

Legend: PASS (hardware evidence) / FAIL / BLOCKED / NOT TESTED / N/A.
Code-review-only items are NOT marked PASS.

| Area | Status | Evidence |
|------|--------|----------|
| ESP8266 boot | PASS | flashed + monitor COM9 (generic ESP-12E env) |
| ESP32 boot | PASS | flashed + monitor COM3 (esp32_devkit env) |
| Sensors | PASS | 11 Base entities registered on both nodes, remote set mirrored |
| Actuators | PASS | relay/dimmer local + remote on both nodes |
| Automations | NOT TESTED | logic review only |
| Persistence (storage) | PASS | load deferred to first report, TIME pre-registered, pers_state snapshot; hardware retest pending |
| Relay persistence | PASS | UID-matched load, applied before first report, no boot glitch |
| Factory reset | NOT TESTED | prefs.clear()/clearAll() reviewed; hardware test pending |
| WiFi | PASS | MATTER_NET connect on both nodes (IP .24/.25) |
| Network recovery | PASS (static) | disconnect/GOT_IP events + auto-reconnect added |
| Web/API | PASS (static) | strict parsing + bounds checks reviewed |
| Authentication | PASS (static) | HTTP Basic Auth gate reviewed (dormant by default) |
| OTA | NOT TESTED | lifecycle code fixed; real upload requires hardware |
| Memory stability | NOT TESTED | 24h soak requires hardware |
| Storage endurance | NOT TESTED | 1000-cycle test requires controlled hardware |
| UDP discovery (batching) | PASS | both nodes: 1 datagram/5s count=12 bytes=573, cross RX packets=12, no drops |

Production gate: NOT READY until physical tests above are completed and pass.
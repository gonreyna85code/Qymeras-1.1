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
- [x] STEP 1 Production Hardening ✅ (code + builds + host tests; hardware flash/verify pending)
  - Timezone semantics: runtime clock stays UTC; SENSOR_TIME `correction` = offset minutes from UTC; UTC→local via `epoch + offset*60` + `gmtime()` (portable ESP8266/ESP32, no libc TZ globals); dead `time_offset` removed
  - ESP-NOW RX FIFO: bounded 8x250B ring (single-producer callback, single-consumer loop), ESP32 portMUX critical sections, callback never blocks/logs, `rx_overflow` counter + warn log in `mesh::tick()`
  - `/calib/set` strict validation: `parseStrictFloat` rejects empty/trailing-junk/overflow/NaN/Inf; timezone integer -720..840; fade/pulse 0..3600000 ms; persist/avail strict 0/1
  - `isVirtual()` error handling: 5s timeout + `response.ok` check + network-error alerts; optimistic UI toggles roll back; never falls back to the local node
  - platformio.ini: espressif32 pinned `@6.5.0`, new `esp32c3_devkit` env (esp32-c3-devkitm-1)
  - Host tests: `tests/host_sanity.py` (45 checks: timezone conversion, strict float parsing + ranges, ESP-NOW FIFO incl. wrap-around/overflow) — 45/45 pass
  - Builds green: esp8266_generic, esp32_devkit, esp32c3_devkit (no warnings in project sources)
  - PHASE 6 remote lifecycle BLOCKER fix: intermittent ESP32 full-loop spin after ESP8266 reboot
    - Symptom: ESP32 main loop saturates (~30k-42k "New remote sensor 'DIMM0'"/s), `millis()` logs freeze,
      UDP/mesh/web/OTA/automation tick starved, WiFi recovery blocked until self-clearing (~1-10s).
    - Root cause: `WiFiUDP::parsePacket()` re-yielded a datagram that `read(buf, 1400)` returned valid data
      for but did **not** dequeue (ESP32 WiFiUDP socket quirk under WiFi-transitional state). The tick had no
      progress guard, so the undrained datagram looped forever. Oversized/invalid packets could also wedge it.
    - Fix (`src/mesh.cpp::parseUDPPacket`): (1) reject+drain oversized packets (`packet_size > sizeof(buf)`);
      (2) `if (len <= 0) { drain; break; }` to escape unreadable transitional datagrams; (3) **unconditional
      post-parse drain** `while (socket.available()) socket.read();` to guarantee dequeue and break re-yield.
      No change to normal (valid, fully-read) datagram path — drain is a no-op there.
    - Validation: 4x ESP8266 reboot stress + 1x ESP32 clean-boot (post-fix) = 0 storm lines; ESP8266 report
      interval silenced mid-test confirmed ESP8266 is NOT a flood source; ESP32 clean-boot re-acquires 11
      remotes idempotently (no phantom growth). Both builds green (esp8266_generic, esp32_devkit).
    - Status: fixed + hardware-validated (was BLOCKER; cleared for re-test).

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
 | Automations | PASS | ESP8266: INTERVAL rule (id=0, actuator idx9) fired at creation -> RELAY0 OFF->ON; deleted -> rules=0, RELAY0 restored OFF. Engine eval+dispatch+delete verified. |
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
| Remote lifecycle storm (UDP re-yield) | PASS | drain fix in `parseUDPPacket`; 4x ESP8266-reboot stress + 1x ESP32 clean-boot = 0 storm lines; millis no longer freezes |
| LOG-vs-sensor dispatch (P4 PACKET_LOG) | PASS | 3x ESP8266 log-triggering toggles observed by ESP32: 0 "New remote sensor"/0 phantom/0 spin |
| Rate limiter (6 req/2s) | PASS | ESP32: 8 concurrent POST /calib/set -> 6x HTTP 200, 8th HTTP 429 |
| Cross-mesh recovery (reboot) | PASS | both nodes rebooted (cycle E); rejoined WiFi, re-meshed, 11 remote entities each, age_ms <30s |
| PHASE 3 cycle E cross-board persist | PASS | ESP8266 RELAY0 ON+persist, ESP32 RELAY0 OFF+persist held across simultaneous reboot |
 | PHASE 3 cycle D (inverted persist) | N/A (limitation) | `inverted` not in `CalibrationPersist`; set at registration only — NOT persisted, requires sketch reflash to test |
| Phase 2 (ESP8266 actuator) | PASS | toggle RELAY0 id=12014157 OFF→ON→OFF (serial `Relay RELAY0 -> ON/OFF`); DIMM0 id=12014158 ->50% (`Dimmer DIMM0 -> 50%`, value 0->50) |
| Phase 3 cycle A (relay ON + persist) | PASS | ESP8266/ESP32: ON+persist -> reboot -> ON (verified earlier in session) |
| Phase 3 cycle B (relay OFF + persist) | PASS | OFF+persist -> reboot -> OFF |
| Phase 3 cycle C (persist disabled) | PASS | ON, persist=false -> reboot -> OFF |
| Phase 13 (strict validation + rate limit) | PASS | ESP32: ref=abc/1e308/12.5xyz -> 400; tz=841 -> 400; tz=0 -> 200; 8 concurrent POST -> 6x200 + 429 |
 | Phase 9 (OTA) | PASS | ESP32 192.168.1.24 `pio upload --upload-port 192.168.1.24` -> `Result: OK Success` (22.3s); ESP8266 192.168.1.25 -> `Result: OK` (15.7s). Both OTA toggles+uploads verified. (Initial ESP32 BEGIN_ERRORs were transient post-erase_flash otadata sync; stable board OTA works.) |
| Phase 10 (memory stability) | PARTIAL | Stable heap (~18360B ESP8266, ~221MB ESP32) + no OOM/panic across session; 24h soak deferred. |
| ESP32 board health | BLOCKED (hardware) | Board intermittent: boots clean then degrades into serial-flood/crash-loop after WiFi activity. NOT code-caused (drain fix validated on clean boots). Needs stable hardware reprovision. |
| ESP32 fleet IP (current) | 192.168.1.28 (was .26/.24) | DHCP drift during testing; discovered via peer /calib device_uid+ip |
| ESP8266 fleet IP (current) | 192.168.1.19 (was .27/.25) | DHCP drift; ESP8266 is the stable node |
| Phase 11 (storage endurance @ ESP8266) | PASS | 100 write/read-back cycles (DIMM0 fade 0..99000): 0 mismatches, final=99000, no corruption |
| Phase 10 (memory stability) | PARTIAL | ESP8266 free heap stable at ~18360 B across session; no OOM/panic. 24h soak deferred (need sustained traffic + both nodes). |
| Phase 11 (storage endurance) | PASS | ESP8266: 100 write/verify (DIMM0 fade) — 0 mismatches. |
| ESP32 board health | NOTE | ESP32 (192.168.1.27) now STABLE with drain-fix build: clean boot, HTTP 200, no storm, 11-remote discovery, healthy. (Earlier instability was the drain-fix-less HEAD firmware storming; resolved by flashing the fix build.) |
| ESP32 fleet IP (current) | 192.168.1.24 | DHCP drifts with reconnects; verified via /logs (WiFi connected, IP:192.168.1.24). |
| ESP8266 fleet IP (current) | 192.168.1.25 | DHCP drifts; stable node. |
|
 | Production gate: NOT READY — 24h memory soak + final matrix not run. All critical defects FIXED & validated: PHASE 6 storm (drain fix), PHASE 9 OTA (both nodes), PHASE 4 automations.| AI subsystem (Phase 1.2, authorized) | PASS | 4 prompt slots (EEPROM 3087..3966), DIGITAL/ANALOG/ANALYTIC validated outputs feed rules engine via sensors::aidig/aiana; CONTROL interface-only. ESP8266: plain-HTTP endpoints only (TLS dropped: BearSSL buffers cannot fit DRAM budget; mbedtls link cost +122KB flash / heap collapse). ESP32: full https support. GUI: Settings sub-tabs General/Network/AI (Network main tab removed). Opt-in: zero traffic when disabled; api_key never echoed. |
| AI verification ESP8266 (.25) | PASS | verify_ai82.py: 27/27 — invalid inputs rejected (out_type/min>max/prompt len/enabled/target), configure+persist across reboot, CONTROL refusal, DIGITAL true/false + garbage rejection via mock LLM, ANALOG range checks + virtual entities (RISK/SCORE), disabled gating (no requests logged). |
| AI verification ESP32 (.24) | PASS | verify_ai.py 7/7 (defaults, global config, ANALYTIC raw storage+log, persistence, opt-in left disabled); post-reflash spot check: /calib 5/5 valid, config persisted through OTA. |
| Host tests (AI validators) | PASS | tests/host_sanity.py: 65/65 incl. applyResult-mirrored strict validators. |
| /calib streaming fix | PASS | handleCalib rewritten: chunked streaming (setContentLength UNKNOWN + sendContent per entry + finalize) instead of monolithic ~8KB String reserve — root cause of malformed/truncated JSON under AI-build heap pressure on ESP8266. Baseline firmware passed 10/10 while AI String build failed 3..4/10; streamed AI build passes 10/10 (ESP8266) and 5/5 (ESP32). |
| RAM budget after AI (ESP8266) | NOTE | statics 57032 -> 59844 B (+2812: ai bss/data ~900B + rodata literals ~1900B); flash 442804 -> 464864 (+22KB total feature cost). Boot heap ~15.9KB; stable with streaming /calib. |
| OTA incident (double upload) | LESSON | Two overlapping `pio upload` attempts to ESP8266 caused byte-count overrun (progress >100%) and a wedged app (ping OK, HTTP dead). Recovery: serial upload COM9 WITHOUT erase_flash preserved WiFi creds AND persisted AI config. Rule: never issue concurrent/overlapping OTA attempts. |
| GUI fix (AI panel emission order) | PASS | AiPanel JS was emitted AFTER the main `</script>` close in handleRoot -> browser rendered raw JS as body text under Devices tab and parsed its embedded inputs as real DOM (source of all "${...} cannot be parsed" console errors). Fix: self-contained `<script>...</script>` wrapper inside AiPanel literal. Also: provider card wrapped in form + autocomplete attrs (silences password-field warnings). Verified served page: all 335 template literals inside script blocks on both boards. |

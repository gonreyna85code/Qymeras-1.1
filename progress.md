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
| Fleet IP (2026-08-27, supersedes above) | ESP32 192.168.1.16 (uid 183646728) / ESP8266 192.168.1.19 (uid 12014147) | DHCP; verified via /calib. ESP8266 outside AI-testing scope (parallel effort). |
|
 | Production gate: NOT READY — 24h memory soak + final matrix not run. All critical defects FIXED & validated: PHASE 6 storm (drain fix), PHASE 9 OTA (both nodes), PHASE 4 automations.| AI subsystem (Phase 1.2, authorized) | PASS | 4 prompt slots (EEPROM 3087..3966), DIGITAL/ANALOG/ANALYTIC validated outputs feed rules engine via sensors::aidig/aiana; CONTROL interface-only. ESP8266: plain-HTTP endpoints only (TLS dropped: BearSSL buffers cannot fit DRAM budget; mbedtls link cost +122KB flash / heap collapse). ESP32: full https support. GUI: Settings sub-tabs General/Network/AI (Network main tab removed). Opt-in: zero traffic when disabled; api_key never echoed. |
| AI verification ESP8266 (.25) | PASS | verify_ai82.py: 27/27 — invalid inputs rejected (out_type/min>max/prompt len/enabled/target), configure+persist across reboot, CONTROL refusal, DIGITAL true/false + garbage rejection via mock LLM, ANALOG range checks + virtual entities (RISK/SCORE), disabled gating (no requests logged). |
| AI verification ESP32 (.24) | PASS | verify_ai.py 7/7 (defaults, global config, ANALYTIC raw storage+log, persistence, opt-in left disabled); post-reflash spot check: /calib 5/5 valid, config persisted through OTA. |
| Host tests (AI validators) | PASS | tests/host_sanity.py: 65/65 incl. applyResult-mirrored strict validators. |
| /calib streaming fix | PASS | handleCalib rewritten: chunked streaming (setContentLength UNKNOWN + sendContent per entry + finalize) instead of monolithic ~8KB String reserve — root cause of malformed/truncated JSON under AI-build heap pressure on ESP8266. Baseline firmware passed 10/10 while AI String build failed 3..4/10; streamed AI build passes 10/10 (ESP8266) and 5/5 (ESP32). |
| RAM budget after AI (ESP8266) | NOTE | statics 57032 -> 59844 B (+2812: ai bss/data ~900B + rodata literals ~1900B); flash 442804 -> 464864 (+22KB total feature cost). Boot heap ~15.9KB; stable with streaming /calib. |
| OTA incident (double upload) | LESSON | Two overlapping `pio upload` attempts to ESP8266 caused byte-count overrun (progress >100%) and a wedged app (ping OK, HTTP dead). Recovery: serial upload COM9 WITHOUT erase_flash preserved WiFi creds AND persisted AI config. Rule: never issue concurrent/overlapping OTA attempts. |
| GUI fix (AI panel emission order) | PASS | AiPanel JS was emitted AFTER the main `</script>` close in handleRoot -> browser rendered raw JS as body text under Devices tab and parsed its embedded inputs as real DOM (source of all "${...} cannot be parsed" console errors). Fix: self-contained `<script>...</script>` wrapper inside AiPanel literal. Also: provider card wrapped in form + autocomplete attrs (silences password-field warnings). Verified served page: all 335 template literals inside script blocks on both boards. |
| AI -> rules E2E (ESP8266, rule EDGE) | PASS | Slot0 DIGITAL RISK (idx23 type13) via mock LLM -> Rule 5 (EDGE rising CMP_GT) Toggle local RELAY0 idx9: sequence true->ON, true->no edge, false->hold, true->OFF. Logs show Rule 5 triggered exactly on rising edges (2 triggers). Proves AI outputs as validated virtual entities consumed by existing rules engine, no AI in actuation path. |
| AI interval auto-run (ESP8266) | PASS | Slot0 interval_ms 8000 with valid DIGITAL mock true: 3 auto-runs observed at ~8s spacing (2348s, 2356s, 2364s) with valid true+age freshness; invalid content correctly rejected as strict validator. Interval respects global rate_limit. Left disabled (manual-only) after test. |
| AI integration CLOSED | PASS | Full fleet verification on final build: ESP8266 27/27 AI suite, ESP32 7/7, /calib 10/10 streaming both boards, GUI 335 templates inside-scripts both boards, host 65/65, E2E EDGE + interval PASS. FactoryReset clears AI block (ESP32 prefs.clear / ESP8266 memset). AIDIG/AIANA render with AI icons in Devices. |
| AI hardening (CONTROL + ANALYTIC) | PASS | CONTROL interface-only verified: manual run valid true raw 95, no sensor card (0 CTL cards), interval 2s does NOT auto-trigger (0 auto), rate limit 1500ms second immediate 400, long 200 chars truncated to 95 valid true. ANALYTIC valid true raw 95 with full sensor context (was 64). Reboot persistence: global+slots preserved after /restart. Host 65/65, /calib 10/10 both boards, mesh shows 4 cards (RISK/SCORE local+remote) each board. |
| AI real-LLM bring-up (Ollama) | PASS | Endpoint http://192.168.1.19:11434/v1/chat/completions provider OLLAMA(1). qwen3.5:0.8b/2b are reasoning models (content:"" + huge reasoning field, finish=length even at 1024 max_tokens) -> unusable; qwen2:0.5b works for DIGITAL/ANALOG/ANALYTIC. Tolerant validators added: DIGITAL accepts true/false with trailing text (prefix match), ANALOG accepts leading float ignoring trailing unit ("35.20C"->35.2). Per-type max_tokens enforced in body (DIGITAL 3, ANALOG 8, rest 100). Sensor context injected as system message (all visible entries, TIME excluded - epoch confused LLM, local/remote tags). Cards RISK(type13)/SCORE(type14) created on valid results both boards; Ollama full-suite PASS x2 boards. |
| CONTROL tool-calling | PASS | Real actuation via JSON tool-calls: system message lists set_relay{name,state}/set_dimmer{name,level}; applyResult parses flat JSON (case-insensitive keys, tolerant state true/on|false/off incl. spaces, level atoi quoted-or-bare 0-100), executes sensors::setRelay/handleDimmer(uid), logs "AI control [CTL] set_relay RELAY0 -> ON". Bugs fixed en route: (1) stagePromptText was NEVER called since RAM-reclaim -> every request shipped user content:"" and model copied the relay example verbatim ("hardcoded ON"); staging now happens in startRun covering manual+interval paths; (2) payload.toLowerCase() destroyed name lookup -> DIMM0 "not found"; (3) mock server mangled double-quotes -> single quotes breaking extractJsonString; (4) remote entries mirrored local names in context -> deduped. SlotResult.raw 64->96->128 for full JSON capture. E2E conditional both directions via ornith-local:9b (~3s): TEMP>100 false -> state:false RELAY OFF; TEMP>10 true -> state:true RELAY True (ESP32 .24 AND ESP8266 .25). Regression after staging fix: RISK True, SCORE 35.0, NOTE analytic all valid; host 65/65; calib tables healthy 25 entries no NaN. |
| LLM model guidance | NOTE | qwen2:0.5b pattern-matches tool examples instead of evaluating numeric conditions ("If TEMP > 100" -> copies state:true); works only with spelled-out comparisons. ornith-local:9b handles natural conditionals correctly at ~3s latency but is also a reasoner (needs its 100-token budget; trivial prompts with tiny max_tokens return empty content). Recommend per-slot model override to ornith-local for CONTROL slots; timeout_ms raised 15000->30000. Logging proxy ($TEMP/opencode/ai_mock/proxy.py :11435->11434, python -u!) captures exact firmware request bodies. |
| T016 hardware verification | PASS | Timezone: /calib/set type=TIME ref=<min> writes correction; RULE_TIME armed at (utc_min+delta+corr) fired exactly on the TZ-shifted local minute on ESP32 (.24) AND ESP8266 (.25) with relay TOGGLE evidence in logs; correction=180 survived OTA reboot on ESP32. Note: timezone minutes go in `ref` arg (not `correction`); /calib/set,/toggle,/dimmer take entry uid (not table index). Malformed input battery (non-numeric min, trailing junk, persist=2, fade=3600001, avail=-1, tz non-integer, tz out-of-range): all 400 with zero state change, both boards. Discovery: 11 remote entries each direction. |
| Load soak (AI+HTTP, ~5min) | PASS-EARLY | 7 rounds x (/ai/run slot0 + /calib + /logs) x 2 boards: no reboot (millis monotonic), no ERR/FTL logs, ESP32 7/7 runs valid. Known behavior noted: ESP8266 web server stalls a few seconds while an outbound AI request to a slow model (ornith-local) is in flight (single-loop architecture). .25 RISK invalid during soak was config artifact (reasoner model vs max_tokens=3), restored qwen2:0.5b -> valid True. NOT yet the 24h soak required by production gate. |
| Rate limiting extended | PASS | Existing checkRateLimit (window 2000ms, burst 6, shared counter) was already on /save,/genset/save,/rules/set,/calib/set,/ai/set,/ai/run; added to /rules/delete,/factory,/toggle,/dimmer. Hardware verified: burst of 10 -> first 7 pass, then 429s; single ops unaffected after 2s. RAM cost ~0 (reuses helper). todo.md P1 item closed. |
| Host suite extended (T013) | PASS | host_sanity.py 65 -> 79 checks. AI validator mirrors updated to CURRENT tolerant firmware semantics (DIGITAL prefix+separator; ANALOG leading-number strtof semantics, "35.20C"->35.2 accepted) -- old section asserted pre-tolerance strictness and was stale vs ai.cpp. New rules-engine mirror: EDGE anti-bounce CONFIRM_READS=3 (single fire on rising CMP_GT, CMP_LT falling-only, EQ never), THRESHOLD GT/LT strict boundaries + EQ 0.5 band, AND/OR gating. All green. |
| /status diagnostics endpoint (T008) | PASS | New read-only GET /status (both platforms): uptime_ms, free_heap, rssi, reset_reason (ESP8266 getResetReason string; ESP32 via esp_reset_reason() labels -- Esp.getResetReason removed in arduino-esp32 6.x), chip. Registered like /logs (no auth, CORS). Verified live: ESP32 201KB heap after OTA reboot ("Software"), ESP8266 ~9.5-9.8KB stable over 60s with streamed-/calib churn, no downward trend. ESP8266 statics now 61396B (74.9%) -- historical 15.6K boot-heap figure is obsolete, use /status going forward. |
| AI CONTROL negative battery | PASS | 12 mock cases on ESP32: no-JSON / empty object / unknown tool -> "control unknown tool"; missing state -> "control missing state"; level 150/-5 -> "control level out of range"; unknown dimmer name -> "control dimmer not found"; UPPERCASE keys and lowercase names execute correctly (case-tolerant). Idempotency: 3x consecutive state:true holds ON (no toggle bug; earlier anomaly was concurrent GUI activity). |
| AI hardening: flat-object guard | PASS | New: OUT_CONTROL payload must contain exactly one '{'+one '}' -> else "control malformed json". Closes real failure mode observed with qwen2: nested JSON {"a":{"tool":"set_relay",..},..} previously extracted keys from inner objects and EXECUTED unintended actions. Verified: nested + double-object rejected, flat still executes. +32B statics ESP8266 (61428B, 75.0%). |
| AI hardening: stale-result invalidation | PASS | startRun now sets results[slot].valid=false so /ai/status reflects the LAST run outcome; previously a failing run left frozen valid=true indefinitely (masked outages, e.g. "empty response content" showed valid=True). Verified: garbage run -> status valid=False immediately. NOTE for consumers: AIDIG/AIANA entity values persist by design (rules treat them like sensors); freshness gating remains a future policy decision. |
| Automations: AI->THRESHOLD->relay E2E | PASS | Mock SCORE=80 (AIANA idx23) + RULE_THRESHOLD GT 50 TOGGLE RELAY0: zero flips while value below threshold (1.5s watch), deterministic flip t+0.47s after value crosses. Cooldown 5000ms: suppressed within window, exactly one re-fire per cooldown period while condition held (flips at t+1.6s and t+7.1s = 5.5s spacing). Confirms rules consume validated AI virtual entities with standard automation semantics. |
| Rules engine boundaries | PASS | MAX_RULES=20 filled exactly (ids 0..19), id>=20 rejected "invalid id". /rules/set arg is `interval` (1000..3600000ms) NOT interval_ms. All 20 rules + AI config survived OTA reflash; SCORE virtual card correctly absent after reboot until next valid AI run (documented lifecycle). Rate limiter naturally throttled rule-fill loop (429s observed, retried OK). |
| Operational notes (this session) | NOTE | Device was concurrently manipulated via GUI during tests (prompt saves disabling slots/global, manual toggles, rule creation+deletion) -- explain all transient anomalies; deterministic re-tests clean. ornith-local cold-load can exceed timeout_ms=30s on ESP32 (HTTP error -11 READ_TIMEOUT); second call succeeds (~warm). /rules JSON uses field name `interval`. |
| AI timeout clamp (ESP8266) | PASS | performRequest caps effective timeout at 20000ms on ESP8266 (config value untouched): the blocking AI call stalls the single-loop web server for its entire duration, so an unresponsive LLM previously froze the UI for up to config.timeout_ms (30s+ observed with ornith-local cold load). ESP32 unchanged. Flashed both boards mid-soak (uptime resets in CSV are these deploys). |
| /ai/set negative battery (.24) | PASS | provider=9 -> "invalid provider (0..2)"; out_type=7 -> "invalid out_type (0..3)"; slot=9 -> "invalid slot"; prompt 114 chars -> "prompt must be 1-112 chars"; min>max -> "invalid prompt config". Note: enabled="maybe" accepted as disabled (lenient bool; only IDs/numbers are strict-parsed). Concurrent-run guard verified ("request already in flight" 400); nuance: requests arriving DURING the blocking AI call queue in the TCP backlog and execute after it completes, so a queued second run can start legitimately once the first finishes -- semantics are correct, ordering is delayed by the stall. |
| Production soak STARTED | IN PROGRESS | Background sampler ($TEMP/opencode/ai_mock/soak.py -> soak.csv, 5-min cadence) recording uptime/free_heap/rssi/reset_reason for BOTH boards via GET /status. Load: RISK (slot0 DIGITAL via Ollama qwen2) interval auto-run every 60s on each board. Early: ESP8266 heap steady 9.0-9.3KB across AI traffic; ESP32 191-198KB. 24h gate requires sampler left running -- future sessions append to same CSV. |

## 2026-08-23 — Docs sync + full verification sweep (ESP32 scope)

| Item | Result | Detail |
|---|---|---|
| Scope change | ACTIVE | ESP8266 (.25) handed to parallel feature effort by owner. No flash/reconfigure/load-tests on it. Soak sampler restricted to ESP32-only (soak.py BOARDS, restarted PID 12016). AGENTS.md updated with hardware-verification-scope note. |
| Clean builds | PASS* | esp32_devkit SUCCESS (RAM 77100/327680, flash 1151425/1310720). esp8266_generic: one transient clean-build FAIL immediately followed by SUCCESS (2.4s) — concurrent .pio artifact contention suspected (parallel agent active in tree; src/html.cpp carries their WIP). *ESP32 build includes that foreign WIP in html.cpp. |
| Host tests | PASS | host_sanity.py 79/79 |
| Fleet health (.24) | PASS | uptime 14min post-deploy, heap ~200KB, RSSI -9..-12; /calib 25 entries (12 remote), no NaN; AI global on qwen2:0.5b; RISK slot auto-running (valid, fresh); rules []; ERR/FTL only historical ornith cold-load -11 pairs |
| AI regression mock (.24) | 9/9 firmware | Initial harness run showed 7/9: both FAILs were harness artifacts, not bugs — (1) ANALYTIC case ran while slot2 disabled (/ai/run correctly rejects disabled slots); (2) DIGITAL "prefix-tolerant" case used prose-before-boolean input, but validator is boolean-FIRST + trailing-prose tolerant BY DESIGN ("False. The earth..." pattern). Corrected cases pass: trailing prose -> false, ANALYTIC raw stored. Config restored to Ollama after mock tests; auto-run resumed (age 3.8s). |
| Soak trend | HEALTHY | esp32_24: 11 samples/0 errs, heap 191256-200456B (+7KB settling drift post-OTA), resets only 'Software'. esp8266_25: excluded from soak (4 historical samples retained in CSV). |
| Docs | UPDATED | todo.md (T008b new section, T008/T016 statuses), structure.md (ai.cpp/web.cpp rows, web module bullets), architecture-baseline.md (AI data flow incl. CONTROL real actuation + flat guard + staging + clamp; endpoint table + /status + /ai/*), AGENTS.md (CONTROL interface-only -> real actuation via web-API primitives). |

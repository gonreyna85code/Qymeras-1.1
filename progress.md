# Qymeras 1.1 Progress Tracker

## Current State (2026-08-27)

- **Branch `main` = production 1.1 tree.** HEAD `5e46e12` carries NO AI code
  (`ai.cpp`/`ai.h`, `sensors::aidig`/`aiana`, QMAI EEPROM block all removed).
  Deterministic core intact, builds green on 3 envs, host suite 45/45.
- **Fleet:** ESP32 = 192.168.1.16 (device_uid 183646728) — in-hardware-scope;
  ESP8266 = 192.168.1.19 (device_uid 12014147) — owned by a parallel feature
  effort (no flash/reconfig without owner approval). OTA flag off on both.
- **Remaining before the production gate (all hardware-required):** 24h memory
  soak, factory-reset hw test, longer endurance, ESP32-C3/S2/S3 hw validation.
- **AI subsystem:** authorized (see `AGENTS.md` "Scope Change Authorization
  2026-08"), developed ONLY on `feature/ai-experiments`. LLM tool-loop experiments
  against the device API (`qwen3.5:2b`/Ollama probe payloads) were cleaned from
  the repo root on 2026-08-27 (archived to a workspace-external backup); they
  are not part of the 1.1 tree.

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
| ESP32 fleet IP (previous) | (superseded 2026-08-27) | .28 (was .26/.24) — DHCP drift during testing; see current row below |
| ESP8266 fleet IP (previous) | (superseded 2026-08-27) | .19/.27/.25 — DHCP drift; ESP8266 is the stable node; see current row below |
| Phase 11 (storage endurance @ ESP8266) | PASS | 100 write/read-back cycles (DIMM0 fade 0..99000): 0 mismatches, final=99000, no corruption |
| ESP32 board health | NOTE | ESP32 (192.168.1.27) now STABLE with drain-fix build: clean boot, HTTP 200, no storm, 11-remote discovery, healthy. (Earlier instability was the drain-fix-less HEAD firmware storming; resolved by flashing the fix build.) |
| ESP32 fleet IP (current) | 192.168.1.16 | 2026-08-27: verified via /calib (device_uid 183646728, local:true). DHCP drift (history .28/.26/.24/.27). HTTP 200; /logs shows clean discovery: 11 ESP8266 remotes re-acquired, 10 stale slots reclaimed, no storm lines. |
| ESP8266 fleet IP (current) | 192.168.1.19 | 2026-08-27: verified via /calib (device_uid 12014147, local:true). DHCP drift (history .27/.25). Matches turn2_real.json entity uids (1201414x). |
| Build matrix (2026-08-27) | PASS | `pio run` full link green on esp8266_generic (RAM 69.6% / Flash 42.2%), esp32_devkit (RAM 22.6% / Flash 73.7%), esp32c3_devkit (RAM 20.9% / Flash 72.8%). Header maps still produced (extra_scripts). |
| Host sanity suite | PASS | `python tests/host_sanity.py` → 45/45 (timezone UTC conversion, strict float parsing + ranges, ESP-NOW bounded RX FIFO incl. wrap/overflow). |
|
 | Production gate: NOT READY — 24h memory soak, factory-reset hw test, and endurance remain (ESP32 .16 within scope; ESP8266 .19 owned by parallel effort). All critical defects FIXED & validated: PHASE 6 storm (drain fix), PHASE 9 OTA (both nodes), PHASE 4 automations. Builds + host suite re-verified 2026-08-27 (3 envs green, 45/45).

---

## GUI Stabilization Pass (feature/GUI, 2026-08-30)

Target: Qymeras 1.1 web GUI on branch `feature/GUI`. Not firmware logic —
front-end/UI contracts only. All changes verified by 3-env build matrix.

### Root causes found
1. **Modal never displays** — `.modal` / `.modal.open` overlay CSS was dropped
   when `.content` CSS was added (`33a5be2`); only `.modal-content` remained, so the
   Automation wizard rendered as a plain unstyled block instead of an overlay dialog.
2. **Logs page unstyled + crash-prone render** — no `.log-grid/.log-panel/.log-entry`
   CSS; `e.v.toLowerCase` missing `()` so severity classes were never applied and
   echo'd the function body; no handling for non-array/invalid/missing-field data.
3. **Double tab init** — `web.cpp sendStartupJS` injected `show(savedTab)` before the
   later `<script>` blocks were parsed (functions undefined → ReferenceError, unhandled
   rejection), then the end-of-bundle IIFE ran `show()` a second time.
4. **Relay persistence UI regressed to DOM state** — `togglePersist`/`togglePulse` read
   `#pulseRow<i>` `style.display` / `classList.contains('on')` instead of `sensors[i]`;
   `togglePersist` always sent `persist=1` (could never disable persistence).
5. **Filter empty states de-scoped** — `document.querySelector('.empty.sm')` global;
   Devices and Settings filters could remove each other's empty state.
6. **Wizard save-time validation never aborts** — `forEach(...){ alert(); return; }` returns
   from the callback only; invalid level/action still POSTed to `/rules/set`.
7. **`sendDimmer` and wizard still used `alert()`** — task requires toast-only errors.
8. **Rule list / empty states unstyled** — `.rule-card`, `.rule-info`, `.empty`, `.d-none`
   CSS missing; `#auto_empty` had a duplicate `class` attribute.

### Files changed
- `src/html.cpp` — CSS (modal overlay, logs grid/panels/entries + severity colors,
  rule cards, `.empty`/`.d-none`); Logs markup (Clear button + `#logStatus`); i18n keys
  (es/en `btn.clear`, `log.*`, `dev.noMatch`, `cfg.noMatch`); rewritten
  `refreshLogs`/`renderLogs` + new `showLogStatus`/`clearLogs`; single tab-init IIFE;
  scoped filter empty states; model-driven `togglePersist`/`togglePulse`/`toggleRelayAvail`
  + `updateRelayUI`/`setPersistence`; model sync in `toggleMatterSwitch`; error handling in
  `toggleDevice`; `sendDimmer` alert→toast; `finishWizard` validation now aborts correctly
  (alert→toast); `loadRules` renders readable action strings.
- `src/web.cpp` — `sendStartupJS` sets `window.startupTab` (single init); new
  `POST /logs/clear` endpoint → `logger::clearBuffer()`.

### Behavior now
- `show()` visibility via `.content.active` only; one `show()` on load from
  `window.startupTab` (AP mode forces `config`).
- Modal visibility via `.modal.open` only; overlay close + Escape preserved.
- Log messages rendered with `textContent`, layers CORE/EVNT→panels, level `inf/wrn/err`
  colors, defensive per-entry parsing, 200-entry cap/panel, empty-state placeholders,
  `#logStatus` line (updated time+count / HTTP error / invalid JSON / network error),
  Clear button calling the new endpoint.
- Relay persist/pulse/avail toggles read `sensors[]`, enforce mutual exclusion,
  keep DOM in sync from the model.
- Filter empty states scoped to `#devices_cards` / `#cards`; `textContent` (no i18n HTML).
- No `alert()` left in the GUI (all toasts).

### Test matrix (builds)
| Env | Status |
|-----|--------|
| esp8266_generic | PASS (RAM 69.8% / Flash 46.9%) |
| esp32_devkit | PASS |
| esp32c3_devkit | PASS |

Remaining: hardware flash + on-device UI walkthrough (logs clear/refresh, modal,
relay toggles, wizards, filters) — not performed in this pass.

---

## Final Fix + Size-Reduction Pass (feature/GUI, 2026-08-30)

Merged final pass on top of the stabilization work above. Goals: close the
remaining functional bugs (toast i18n), remove confirmed dead code, unify
duplicated sensor formatting, cut unnecessary network/DOM work, and shrink the
embedded web payload — all without changing feature behavior or appearance.

### Functional fixes
- **Toast texts showed raw keys**: `tf('alert.failed')` and `tf('alert.saveError')`
  were used by `toggleRelayAvail`/`isVirtual`/`updateRelayUI` but the keys did not
  exist in `I18N` (es/en) → toasts rendered the literal key. Added
  `alert.failed` and `alert.saveError` in both languages.
- **Dead `saved` notice**: `location.search` `saved=1` branch targeted a
  `#savedNotice` element that no longer exists (and `saved.notice` i18n keys).
  Removed the dead branch + keys.
- **Health badge duplication**: `loadDevices()` rebulids sensor state every control
  poll; the poll also re-computed the `#systemHealth` badge → moved badge update
  into `loadDevices()` (runs on boot + every control poll) and removed the copy.

### Dead code removed (each verified 0-referenced first)
- `renderAutomationTable()` (`editRule(i)` array-index version) — `loadRules()`
  renders `#auto_table`; `newRule()` kept.
- `getTotalSteps()` / `getStepNumber()` (wizard) — the wizard indexes via
  `getRelevantSteps()` directly in `showStep()`/`nextStep()`.
- `availableSensors` duplicate of `sensors`. `sensorByIndex()` now reads the
  shared `sensors` cache; `loadSensorsAndActuators()` calls `getCalib(false)`
  (no per-wizard-open `/calib` fetch when cached).
- CSS: `.card`, `.notice`+`.notice.success`, `.dash-section`, `.time-value`,
  `.gradient-bg` (3 rules — `input[type=range]` already styles sliders),
  `.dot.warn`; unused slider `class="gradient-bg"` attr.
- i18n keys (es+en): `title`, `stat.updated`, `rule.condition`,
  `rule.actuators`, `alert.isVirtualTimeout`, `saved.notice`.

### Consolidations (no behavior/appearance change)
- New shared `formatSensorValue(type, value, state)` used by `deviceCard`,
  `updateSettingsValues` and the wizard step-2 condition list, replacing three
  byte-identical TEMP/HUMI/PRESS/LEVEL/LUMI/AIRQ/RAIN/CONTACT/GENERIC chains
  (removed `SENSOR_DISPLAY` map).
- New `calHead(label, s, i)` settings-card header helper; 6 duplicated
  `settings-card-head` blocks (AIRQ/RAIN/CONTACT/DIMM/REL + `sensorCalibCard`)
  now render via the shared helper.

### Reduced runtime work
- 5s poll now tab-gated: `config` → `updateSettingsValues()` (cached); `control`
  → `loadDevices()` (fresh); `auto/logs` → nothing. Previously every 5s while
  visible the GUI rebuilt the (possibly hidden) device DOM and fetched `/calib`
  regardless of tab.

### Server trim (verified: 0 GUI references)
- `/calib` response no longer emits unused `pers_state`, `min`, `max`, `pin`,
  `last_update` per sensor (smaller JSON on every poll; `age_ms`/`correction`
  retained — both are read by the GUI).

### Size results
Embedded web payload in `src/html.cpp` (PROGMEM chunks):

| Chunk | Before | After |
|-------|--------|-------|
| Styles | 19,889 | 18,845 |
| Tabs | 18,293 | 18,035 |
| Rules | 1,495 | 228 |
| CardsSettings | 9,819 | 8,159 |
| DeviceCards | 5,805 | 5,496 |
| JS | 27,261 | 26,369 |
| AutoWizJS | 33,863 | 32,053 |
| **Total** | **116,425** | **109,185** |

Saved **7,240 B (6.22 %)**. `html.cpp` file 116,936 → 109,696 B.

### Verification
- Extracted GUI JS syntax-checked with `node --check` (PASS, incl. `°C` UTF-8
  integrity) and leftover-grep scan for every removed symbol (0 hits).
- Build matrix (no ESP32 flash — in use by a parallel branch test):
  esp8266_generic PASS (RAM 69.7% / Flash 46.2%); esp32_devkit + esp32c3_devkit
  compile+link PASS (not flashed).
- **Hardware verification (ESP8266, 2026-08-30):** flashed COM9 (`pio run
  --target upload`, 486,816 B, hash verified) → clean boot, WiFi 192.168.1.19,
  heap 18,384 B. Live checks: `/` HTTP 200 (page 109,535 B; new helpers
  `formatSensorValue`/`calHead` present, dead symbols `renderAutomationTable`,
  `availableSensors`, `SENSOR_DISPLAY`, `gradient-bg`, `getTotalSteps`,
  `savedNotice` absent; `alert.failed`/`alert.saveError` keys live in es/en);
  `/calib` 12 sensors with `age_ms`+`correction` and NO
  `pers_state/min/max/pin/last_update`; `/logs` returns boot entries normally.
## Search inputs: fix + styling (2026-08-30)
- Root cause: `loadDevices()` (5 s tab poll) and `loadCalib()` rebuilt their
  whole container `innerHTML`, recreating the `#devSearch` / `#settingsSearch`
  inputs every time → cursor/focus loss and an auto-applied filter on each poll
  ("solo enters" themselves).
- Fix: inputs moved OUT of the rebuilt containers into static markup
  (`#devices_cards` gains static `#dash_stats`, `.search-box`, `#dash_lists`;
  config gains a static `.search-box` before `#cards`). Because they are never
  re-rendered, focus/cursor persist across polls.
- Filtering split: `filterDevices()`/`filterSettings()` read the input and
  store the APPLIED term; `applyDeviceFilter()`/`applySettingsFilter(term)`
  re-apply the applied term after a list re-render. Polls no longer read the
  input value, so a half-typed query is never auto-triggered.
- Placeholder i18n: inputs use `data-i18n-ph` extended in `setLang()` (es/en).
- Styling: added `.input-sm` rule (border-radius `var(--radius-sm)`, border
  `var(--border)`, bg `var(--bg-2)`, focus ring) — the class previously had no
  CSS at all, hence inputs had square unstyled corners.
- Payload 109,185 → 109,392 B (+207 B). `node --check` PASS. Build + flash
  esp8266_generic SUCCESS (flash this round: 2026-08-30). Live `/` 200
  (109,810 B) — single `#devSearch`/`#settingsSearch`, no auto-read of inputs,
  `.input-sm` rule present.

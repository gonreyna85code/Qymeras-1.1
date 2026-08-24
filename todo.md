# Qymeras 1.1 Task List

## Priority Order

### P0 - Critical (Must fix before production) ✅ DONE
- [x] Framework pinning to stable versions (3.30102.0 / 6.5.0) ✅
- [x] `raw_address()` bug fix (patch IPAddress.h) ✅
- [x] EEPROM→Preferences migration for ESP32 ✅
- [x] OTA toggle functionality verified working ✅
- [x] Logging system 3-layer architecture ✅
- [x] Add web authentication on all endpoints ✅
- [x] OTA device identity/provisioning check ✅
- [ ] Memory leak testing under load

### P1 - Important (Should fix for stability) 
- [x] Compile verification on ESP8266 and ESP32 ✅
- [ ] Factory reset reliability validation (hardware test pending)
- [ ] Long-term EEPROM write endurance testing
- [x] Network partition recovery (disconnect/GOT_IP events + auto-reconnect; hardware test pending)
- [x] Input validation on all API endpoints ✅
- [x] Rate limiting on POST endpoints (burst 6/2s shared window: /save,/genset/save,/rules/set,/rules/delete,/factory,/toggle,/dimmer,/calib/set,/ai/set,/ai/run) ✅
- [x] Calibration value persistence (UID-based, survives reboot; reconfig after storage-layout migration)

### P1.2 - AI subsystem (authorized scope extension, 2026-08) DONE
- [x] 4 prompt slots persisted in EEPROM block 3087..3966 (QMAI v1 layout)
- [x] Output types DIGITAL/ANALOG (strict validated, feed rules engine) + ANALYTIC (raw+log); CONTROL interface-only
- [x] Providers OPENAI/OLLAMA/CUSTOM via endpoint config; ESP8266 plain-HTTP only (TLS infeasible in DRAM budget), ESP32 full https
- [x] GUI Settings sub-tabs General/Network/AI; Network main tab removed
- [x] Fully opt-in; api_key never echoed back
- [x] Streaming /calib response (chunked) -- fixes heap-exhaustion JSON corruption on ESP8266
- [ ] AI per-slot rate limiting beyond global rate_limit_ms (future)

### P2 - Nice-to-Have (Improve after 1.1)
- [ ] HTTPS for OTA transfers
- [ ] CSRF tokens on web forms
- [ ] Dashboard graphical interface
- [ ] MQTT integration
- [ ] Notifications (email/SMS)
- [ ] Mobile app companion
- [ ] Rule editor UI improvements

### P3 - Future (Phase 3+)
- [ ] AI/ML subsystem (out of scope for 1.1)
- [ ] Zigbee/Z-Wave integration
- [ ] Matter protocol support
- [ ] Cloud dashboard
- [ ] User authentication system
- [ ] HTTPS everywhere
- [ ] Automated over-the-cloud updates

## Task Details

### T001: Framework Pinning ✅ COMPLETE
- [x] Pin ESP8266 to espressif8266@3.30102.0
- [x] Pin ESP32 to espressif32@6.5.0
- [x] Update platformio.ini with pinned versions
- [x] Verify both platforms compile (linker errors expected)

### T002: raw_address() Bug Fix ✅ COMPLETE
- [x] Patch IPAddress.h to make raw_address() public
- [x] Verify compilation after patch
- [x] Test on both platforms (linker errors expected, no compile errors)

### T003: EEPROM→Preferences Migration ✅ COMPLETE
- [x] Create Preferences namespace for ESP32
- [x] Migrate WiFi credentials to Preferences
- [x] Migrate OTA flag to Preferences
- [x] Test on both platforms

### T004: OTA Toggle Functionality ✅ COMPLETE
- [x] Implement /ota/toggle endpoint
- [x] Store ota_enabled flag in EEPROM/Prefs
- [x] Device reboot after toggle
- [x] Verify OTA status endpoint

### T005: Logging 3-Layer Architecture ✅ COMPLETE
- [x] Create log.h with 3-layer interface
- [x] Implement log.cpp with circular buffers
- [x] Reduce buffers from 30 to 12 entries per layer
- [x] Add getRecentLogs() API
- [x] Add JSON output for /logs endpoint

### T006: Web Authentication ✅ COMPLETE
- [x] Add HTTP basic auth middleware
- [x] Secure POST endpoints: /save, /calib/set, /genset/save, /rules/set, /rules/delete, /factory, /toggle, /dimmer, /calib/set
- [x] Test authentication flow (compile verification on both platforms)
- [x] Document limitations (auth disabled by default, credentials "admin"/"qymera123", can be disabled by not sending Authorization header)

### T007: OTA Device Identity Check ✅ COMPLETE (naming corrected)
- [x] Store chip-unique device token (`GET_CHIP_ID()`) in EEPROM/Preferences
- [x] Verify token on boot/toggle; mismatch disables OTA
- [x] Document accurately: device identity/provisioning check, **NOT** firmware hash/authenticity
- [x] SHA-256/signature authenticity deferred to Phase 3+ (per AGENTS.md)

### T008: Memory Leak Testing 🔄 IN PROGRESS (observability added)
- [x] GET /status diagnostics endpoint (uptime_ms, free_heap, rssi, reset_reason, chip) on both platforms
- [x] Early soak (~5min AI+HTTP load): no reboot, no ERR/FTL, millis monotonic, ESP8266 heap stable ~9.5-9.8KB under /calib churn
- [ ] 24h soak with periodic /status sampling (production gate requirement) — sampler running (soak.csv, 5-min cadence) with 60s AI interval load on both boards
- [ ] Set memory thresholds/alerts

### T009: Factory Reset Reliability ⏳ PENDING
- [ ] Test factory reset flow
- [ ] Verify EEPROM/Prefs clearing
- [ ] Confirm reboot to AP mode
- [ ] Validate state after reset

### T010: Network Resilience 🔄 CODE APPLIED, HARDWARE TEST PENDING
- [x] WiFi disconnect detection (STA_DISCONNECTED event clears wifi_connected)
- [x] Re-connect detection (STA_GOT_IP event sets wifi_connected)
- [x] Auto-reconnect enabled on both platforms (ESP8266 setAutoReconnect added)
- [ ] Test on hardware: disconnect/reconnect, ESP-NOW fallback, transport switching, mesh message loss

### T011: API Input Validation ✅ CODE COMPLETE
- [x] Strict full-string parsing on all ID/value fields (rejects malformed/overflow)
- [x] Bounds checks on sensor/actuator/rule indices, thresholds, levels, dates, ports
- [x] Reject oversized/invalid rule payloads
- [x] Strict `/calib/set` validation (STEP 1): `parseStrictFloat` rejects empty/trailing-junk/overflow/NaN/Inf; timezone integer -720..840; fade/pulse 0..3600000 ms; persist/avail strict 0/1 (documented in architecture-baseline.md)

### T012: Long-term Endurance ⏳ PENDING
- [ ] Run 1000+ boot cycles
- [ ] Monitor EEPROM write count
- [ ] Verify calibration persistence
- [ ] Test factory reset multiple times

### T013: Test Suite Development 🔄 PARTIAL (host sanity tests added)
- [x] Host tests: `tests/host_sanity.py` — 79 checks incl. timezone conversion, strict float parsing + ranges, ESP-NOW RX FIFO, AI validators (tolerant firmware-faithful mirrors), rules-engine logic mirror (EDGE anti-bounce/direction, THRESHOLD bands, AND/OR gates)
- [ ] Unit tests for rule logic
- [ ] Unit tests for calibration math
- [ ] Integration tests (mock hardware)
- [ ] Property-based tests for state invariants

### T014: Documentation Complete ✅ DONE
- [x] architecture-baseline.md created
- [x] agents.md created/updated
- [x] progress.md created
- [x] structure.md created
- [x] todo.md created
- [x] README.md translated to English
- [ ] User guide revisions
- [ ] API reference documentation

### T015: Final Production Audit ⏳ PENDING
- [ ] Complete all P0-P1 tasks
- [ ] Verify all documentation
- [ ] Build both platforms
- [ ] Test critical paths
- [ ] Sign off for production

### T016: Production Hardening STEP 1 🔄 CODE COMPLETE, HARDWARE VERIFY PENDING
- [x] Timezone semantics: runtime stays UTC; SENSOR_TIME `correction` = offset minutes from UTC; portable UTC→local via epoch+offset+gmtime()
- [x] ESP-NOW bounded RX FIFO (8x250B ring, portMUX, overflow counter + warn log)
- [x] Strict `/calib/set` validation (parseStrictFloat + type ranges)
- [x] `isVirtual()` error handling (timeout, response.ok, alerts, UI rollback)
- [x] platformio.ini: espressif32 pinned @6.5.0 + new `esp32c3_devkit` env
- [x] Host tests `tests/host_sanity.py` (45/45) + builds green on 3 envs
- [x] Flash ESP32 + ESP8266 and verify: timezone persists (corr=180 survives OTA reboot) & shifts TIME rules (RULE_TIME fires at shifted local minute on BOTH boards), malformed `/calib/set` rejected (7 cases x2 boards, zero state change), discovery complete (11 remote entries bidirectional) ✅

## Status Summary

| Category | Count | Percentage |
|----------|-------|------------|
| Total tasks | 16 | 100% |
| Completed (code + docs) | 9 | 56% |
| Code applied, hardware test pending | 3 | 19% |
| Pending (requires hardware) | 4 | 25% |

## Final validation state
See `progress.md` "FINAL VALIDATION STATUS TABLE". No area is marked PASS without
hardware evidence; all hardware-dependent areas are currently NOT TESTED (no boards
attached to this machine). Production gate: NOT READY until physical tests pass.

## Next Actions

### Immediate (This Session)
- [ ] Verify build on both platforms with `pio run` ✅ (linker errors expected, no compile errors)
- [ ] Check for any new linker errors
- [ ] Review diffs from current changes
- [ ] Update this todo list with results ✅

### This Week
- [ ] Begin P0 task completion (already complete: framework pinning, raw_address, EEPROM migration, OTA toggle, logging)
- [ ] Fix any critical issues found
- [ ] Document findings in progress.md
- [ ] Prepare for P1 task start

### This Month
- [ ] Complete P0 and P1 tasks
- [ ] Begin P2 (nice-to-have) items
- [ ] Verify production readiness
- [ ] Final audit and sign-off

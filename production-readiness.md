# Qymeras 1.1 - Production Readiness

State as of 2026-08-27 (branch `main`, HEAD `5e46e12`).

## Summary

- **No active BLOCKERs.** All critical production defects reported across the
  1.1 cycle have been fixed and hardware-validated on at least one node.
- Remaining items are **validation-only** (long-duration / controlled-hardware
  tests), not code fixes:
  - 24h memory soak.
  - Factory-reset reliability hardware test.
  - Longer storage-endurance cycles (100-cycle test passed).
  - ESP32-C3 / ESP32-S2 / ESP32-S3: build-verified only, no hardware validation.
- Static code review and build matrix are green (3 envs). Host test suite 45/45.

## Verified (hardware evidence — see `progress.md` FINAL VALIDATION STATUS TABLE)

| Area | Result |
|------|--------|
| ESP8266 + ESP32 boot | PASS (flashed + serial monitor) |
| Sensors/actuators local + remote (mesh mirror) | PASS |
| Automations (edge/threshold/time/interval, AND/OR, cooldown, delete) | PASS |
| Relay persistence (UID-matched) + persist cycles A/B/C | PASS |
| UDP discovery batching (+ remote lifecycle + stale slot reclamation) | PASS |
| PHASE 6 remote-lifecycle storm (UDP re-yield wedge) | FIXED + hw-validated |
| LOG-vs-sensor dispatch (packet kind PACKET_LOG) | PASS |
| Rate limiter 6 req / 2 s burst; strict `/calib/set` validation | PASS (hw) |
| Cross-mesh recovery across reboots | PASS (both nodes) |
| OTA upload real (ESP32 and ESP8266) + OTA toggle | PASS |
| Storage endurance (ESP8266, 100 write/verify cycles) | PASS |
| Memory stability | PARTIAL — stable across session (ESP8266 ~18.36 kB heap, ESP32 no OOM); 24h soak not run |

## Current fleet

| Node | IP (2026-08-27) | device_uid | Build env | Owner |
|------|-----------------|-----------|-----------|-------|
| ESP32 | 192.168.1.16 | 183646728 | `esp32_devkit` | main hardening scope |
| ESP8266 | 192.168.1.19 | 12014147 | `esp8266_generic` | parallel feature effort — no flash/reconfig without owner approval |

IPs are DHCP-assigned and drift. Verify before acting via `GET /calib`
(`device_uid` + `ip` fields). ESP32 `/logs` verified clean 2026-08-27: 11
remotes re-acquired, 10 stale slots reclaimed, no storm lines.

## Hardening history (production fixes)

### [FIXED + VALIDATED] OTA identity token & EEPROM overlap
- The OTA enable flag and identity-token slot originally lived at offsets 9/10,
  overlapping relay-state (0..9) and the WiFi credentials block (starts at 10) —
  "saving the OTA flag corrupts memory".
- Fix: relocated to a dedicated non-aliased slot after the rules block
  (`config.h`: `EEPROM_OTA_FLAG_ADDR`, `EEPROM_OTA_HASH_ADDR`); 4-byte `put`/`get`
  helpers; provisioning detection (all-0xFF / all-0x00 = unprovisioned).
- Flashed and OTA-upload verified on both nodes.

### [FIXED] Storage zero-fill (ESP32 random-fade bug)
- `Preferences.getBytes()` leaves the buffer untouched on missing keys →
  `storage::get()` zero-fills first + validates magic/version/uid → deterministic
  defaults instead of garbage (e.g. random fade).

### [FIXED] Relay persistence boot glitch
- UID-based persistence (magic+version); applied once at boot before the first
  report; no OFF→ON GPIO glitch at registration.

### [FIXED + VALIDATED] PHASE 6 remote-lifecycle storm (ESP32 full-loop spin)
- Symptom: after an ESP8266 reboot the ESP32 loop saturated (~30k-42k
  "New remote sensor"/s), starving UDP/mesh/web/OTA; millis() froze.
- Root cause: `WiFiUDP::parsePacket()` re-yielded a datagram that
  `read(buf, 1400)` returned data for but did not dequeue (ESP32 socket quirk
  under WiFi-transitional state). No progress guard in the tick.
- Fix (`mesh.cpp::parseUDPPacket`): reject+drain oversized packets;
  `if (len <= 0) { drain; break; }`; unconditional post-parse drain;
  bounded per-tick RX (<=8 datagrams/socket).
- Validated: 4x ESP8266-reboot stress + ESP32 clean boot = 0 storm lines.

### [FIXED + VALIDATED] Discovery batching + remote lifecycle
- One datagram per sensor caused queue overflow on ESP32 under bursts.
- Fix: batch <=29 packets/datagram (1400 B, below MTU); RX drain bounded to 8/
  tick/socket; stale remotes hidden from `/calib` + unreferenced slots reclaimed;
  `PACKET_LOG` never parsed as a sensor packet; remote logs never re-broadcast.
- Discovered remote config routed to owner IP (`isVirtual()`), no local fallback.

### [FIXED] Protocol v4 → v5
- v4 added an explicit `kind` byte (`PACKET_SENSOR` / `PACKET_LOG`).
- v5 (packet 47→58 B) mirrors fade/persist/pers_state/pulse/pulse_ms to remote
  nodes; legacy v3/v4 packets still parsed. Both builds green.

### [FIXED] Web/API input validation + rate limiting
- Strict full-string parsing on all IDs/values; bounds checks on indices,
  thresholds, levels, dates, ports; `/calib/set` uses `parseStrictFloat`
  (rejects empty/trailing-junk/overflow/NaN/Inf) + type ranges
  (timezone -720..840, fade/pulse 0..3600000 ms, persist/avail strict 0/1).
- Rate limiting: burst-tolerant 6 req / 2 s window on all state-changing
  endpoints (malformed → 400, burst → 429).

### [FIXED] HTTP Basic Auth infrastructure (dormant by design)
- Initially auth was forced `true` with no client-sent `Authorization` header →
  every state-changing call 401. Fix: `auth_enabled=false` by default, the GUI
  works, and the auth gate is a dormant Phase-3 facility.
- Hardcoded placeholder credentials (`admin:qymera123`) stay in the binary,
  never embedded in served client JS.

## Known accepted limitations (1.1)

- HTTP only (no HTTPS for OTA/web). ESP32 firmware retains https support for the
  AI subsystem (external); base web/OTA is plain HTTP.
- OTA "integrity" = device identity/provisioning token (`GET_CHIP_ID()`), NOT a
  firmware hash / cryptographic authenticity. SHA-256 deferred to Phase 3+.
- Authentication dormant; recommended for trusted local networks only.
- THRESHOLD rules have no hysteresis (use `cooldown_ms`); TIME `time_s` is
  truncated to minutes; TIME/INTERVAL have no catch-up after boot.
- `inverted` actuator flag is set at registration only — not persisted.

## Next steps before the production gate

1. 24h memory soak on both nodes (sustained mesh traffic + web polling).
2. Factory reset hardware test on ESP32 (.16): verify `/factory` clears
   credentials/rules/calibration and returns to `QymeraSetup` AP mode.
3. ESP8266 (.19) re-verification **requires owner approval** (parallel feature
   effort owns that unit).
4. ESP32-C3/S2/S3 hardware validation (build-verified only today).
5. Environment endurance (>=1000 cycles) on a controlled board.

## Build matrix (2026-08-27)

| Env | Result | Footprint |
|-----|--------|-----------|
| `esp8266_generic` | SUCCESS (full link) | RAM 69.6%, Flash 42.2% |
| `esp32_devkit` | SUCCESS (full link) | RAM 22.6%, Flash 73.7% |
| `esp32c3_devkit` | SUCCESS (full link) | RAM 20.9%, Flash 72.8% |

Host suite: `python tests/host_sanity.py` → 45/45 (timezone UTC conversion,
strict float parsing + ranges, ESP-NOW bounded RX FIFO incl. wrap/overflow).
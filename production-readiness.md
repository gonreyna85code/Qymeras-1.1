# Qymeras 1.1 - Production Readiness

> **Branch context (2026-08-27):** this file lives on `feature/ai-experiments`
> (AI line). The **production gate applies to the AI-free 1.1 MVP on `main`**
> (HEAD `b2a9b01`), where the equivalent doc is kept current. On this branch the
> content below is a historical record; the live status table and the soak
> state are tracked in `progress.md` (host 79/79, ESP32-only soak IN PROGRESS,
> OTA uploads PASS, T016 timezone verified on both boards).

Historical notes:

# BLOCKER (superseded — see progress.md for the live gate status)

Hardware validation on physical ESP8266/ESP32 is still required before the
1.1 production gate can be passed. Static/code review is green; soak, storage
endurance, and real OTA upload tests are NOT TESTED.

## Blocker: HTTP Basic Auth
- Auth was enforced server-side (`checkAuth()` forced `auth_enabled=true` because the hardcoded creds are non-empty), but the served GUI (`html.cpp`) never sends the `Authorization` header on any `fetch()` request → every state-changing call returned `401 Authentication required`.
- Fix: `checkAuth()` initialization now keeps `auth_enabled = false`, so the gate short-circuits and the GUI works again. The auth infrastructure is retained but dormant, awaiting a future Phase-3 login flow.
- Hardcoded credentials (`admin:qymera123`) remain in the firmware binary; they are NOT embedded into the served client JS (avoids exposing secrets in view-source).

## [FIXED] OTA identity token & EEPROM overlap
- Root cause (reported defect): the OTA enable flag and the identity-token slot
  were originally stored at offsets 9 and 10, which **overlapped the relay-state
  region (0..9) and the WiFi credentials block (starts at offset 10)**. Saving
  the flag / provisioning the token corrupted relay state and the SSID length
  byte — the reported "saving the OTA flag corrupts memory" bug.
- Fix: flag and token relocated to a dedicated, non-aliased slot in the
  reserved region after the rules block (`config.h`: `EEPROM_OTA_FLAG_ADDR`,
  `EEPROM_OTA_HASH_ADDR`). The 4-byte token is now persisted/verified with
  the 4-byte `put`/`get` helpers (no 1-byte truncation) and correct
  provisioning detection (all-0xFF or all-0x00 = unprovisioned).
- Builds verified green on both ESP8266 and ESP32.

## [FIXED] Storage zero-fill (ESP32 random-fade bug)
- `Preferences.getBytes()` left the destination untouched on missing keys;
  `storage::get()` now zero-fills first and validates magic/version/uid, so
  unprovisioned calibration slots fall back to deterministic defaults.

## [FIXED] Relay persistence
- UID-based persistence with magic+version validation; persisted state applied
  once at boot before the first report; no GPIO OFF->ON glitch at registration.

## Blocker: ID Validation
- HTTP endpoint ID parsing now rejects malformed input using strict full-string parsing.
- Range checks are applied before acting on sensor, actuator, or rule IDs.

## Platforms
- ESP8266: source compiles; linker errors are expected when `setup()`/`loop()` are absent.
- ESP32: source compiles; linker errors are expected when `setup()`/`loop()` are absent.

## Status
- Code compiles on both platforms.
- Authentication is DISABLED (known open limitation); the GUI operates without auth. Deferred to Phase 3+.
- ID parsing is stricter on write paths.
- Remaining P0/P1 items are test/validation tasks, not code fixes.

# WARN
- No HTTPS for OTA (HTTP only).
- OTA "integrity" is a **device identity / provisioning token** (`GET_CHIP_ID()`),
  NOT a firmware hash and NOT cryptographic authenticity. Full SHA-256/authenticity
  deferred to Phase 3+ (see AGENTS.md).
- Authentication infrastructure is present but dormant (`auth_enabled=false`) so
  the GUI works; hardcoded credentials remain in the binary as a placeholder.

# FINAL
Code review and dual-platform builds are green. Physical validation (boot, relay
persistence, factory reset, OTA upload, network reconnect, 24h soak, storage
endurance) is required before declaring production-ready.

## STEP 1 Production Hardening (code + builds + host tests done, hardware verify pending)

### Timezone semantics
- Runtime clock always stays UTC (NTP syncs UTC; `time()` never shifted).
- SENSOR_TIME `correction` IS the timezone offset in minutes from UTC (persisted).
- UTC→local = `epoch + offset_min*60` decomposed with `gmtime()` (portable
  ESP8266/ESP32; avoids libc TZ globals). Dead `time_offset` removed.
- Affected: `getTime()`, `getMinutesOfDay()`, `updateNTPTime()`; TIME rules fire
  on local minutes-of-day.

### ESP-NOW RX FIFO (removes single-slot RX buffer)
- Bounded 8x250B ring, single-producer (callback) / single-consumer (loop).
- Callback never blocks/allocates/logs; ESP32 guarded by portMUX critical sections.
- Full queue → drop new message + `rx_overflow` counter; `mesh::tick()` logs the
  delta. `espnow_get_rx_overflow()`/`espnow_get_rx_queue_depth()` exposed.

### `/calib/set` strict validation
- `parseStrictFloat` rejects empty, trailing junk, overflow (ERANGE), NaN, Inf.
- Ranges: timezone integer -720..840; fade/pulse 0..3600000 ms; persist/avail
  strict "1"/"0". No silent coercion (e.g. `abc -> 0` is rejected).

### `isVirtual()` remote-config error handling
- 5s timeout (AbortController), `response.ok` verification, network-error alerts.
- No false success: a 4xx/5xx from the owner shows an alert; never falls back to
  the local node. Optimistic UI toggles (relay button, persist/pulse checkboxes)
  roll back on failure.

### Build/tooling
- `platformio.ini`: espressif32 pinned `@6.5.0`; new `esp32c3_devkit` env
  (esp32-c3-devkitm-1). Builds green on esp8266_generic / esp32_devkit /
  esp32c3_devkit with no warnings in project sources.
- Host tests: `tests/host_sanity.py` (45 checks, 45/45 pass).

### Remaining before production gate (unchanged)
- Physical validation on ESP8266 + ESP32 (boot, persistence, OTA, soak, endurance).
- ESP32-C3: flashed-hardware validation pending (build-verified only).

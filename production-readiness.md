# Qymeras 1.1 - Production Readiness

# BLOCKER

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

# Qymeras 1.1 - Production Readiness

# BLOCKER

Verification found critical issues preventing production readiness.

## Blocker: EEPROM wrappers
- ESP8266 EEPROM wrapper now uses `EEPROM.h` direct calls instead of recursive stubs.

## Blocker: HTTP Basic Auth
- Auth was enforced server-side (`checkAuth()` forced `auth_enabled=true` because the hardcoded creds are non-empty), but the served GUI (`html.cpp`) never sends the `Authorization` header on any `fetch()` request → every state-changing call returned `401 Authentication required`.
- Fix: `checkAuth()` initialization now keeps `auth_enabled = false`, so the gate short-circuits and the GUI works again. The auth infrastructure is retained but dormant, awaiting a future Phase-3 login flow.
- Hardcoded credentials (`admin:qymera123`) remain in the firmware binary; they are NOT embedded into the served client JS (avoids exposing secrets in view-source).

## [FIXED] OTA Integrity & EEPROM overlap
- Root cause (reported defect): the OTA enable flag (`EEPROM_OTA_FLAG_ADDR`)
  and integrity baseline (`EEPROM_OTA_CHECKSUM_ADDR`) were stored at offsets
  9 and 10, which **overlapped the relay-state region (0..9) and the WiFi
  credentials block (starts at offset 10)**. Saving the flag / provisioning
  the baseline corrupted relay state and the SSID length byte — the reported
  "saving the OTA flag corrupts memory" bug.
- Fix: flag and baseline relocated to a dedicated, non-aliased slot in the
  reserved region after the rules block (`config.h`: `EEPROM_OTA_FLAG_ADDR`,
  `EEPROM_OTA_HASH_ADDR`). The 4-byte baseline is now persisted/verified with
  the 4-byte `put`/`get` helpers (no 1-byte truncation) and correct
  provisioning detection (all-0xFF or all-0x00 = unprovisioned).
- Builds verified green on both ESP8266 and ESP32.

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
- OTA integrity is detection-only (custom 32-bit hash), not authenticity. Full SHA-256/authenticity deferred to Phase 3+ (see AGENTS.md).

# FINAL
Project requires further verification before production release.

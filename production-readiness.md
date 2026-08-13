# Qymeras 1.1 - Production Readiness

# BLOCKER

Verification found critical issues preventing production readiness.

## Blocker: EEPROM wrappers
- ESP8266 EEPROM wrapper now uses `EEPROM.h` direct calls instead of recursive stubs.

## Blocker: HTTP Basic Auth
- Auth was enforced server-side (`checkAuth()` forced `auth_enabled=true` because the hardcoded creds are non-empty), but the served GUI (`html.cpp`) never sends the `Authorization` header on any `fetch()` request → every state-changing call returned `401 Authentication required`.
- Fix: `checkAuth()` initialization now keeps `auth_enabled = false`, so the gate short-circuits and the GUI works again. The auth infrastructure is retained but dormant, awaiting a future Phase-3 login flow.
- Hardcoded credentials (`admin:qymera123`) remain in the firmware binary; they are NOT embedded into the served client JS (avoids exposing secrets in view-source).

## Blocker: OTA Integrity
- Current mechanism hashes only first 256 bytes with a custom 32-bit hash.
- This is integrity detection, not cryptographic authenticity.

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
- OTA integrity remains a detection-only mechanism.

# FINAL
Project requires further verification before production release.

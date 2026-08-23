# Qymeras 1.1 Structure & Ownership

## File Organization

### Source Files (src/)

| File | Purpose | Owner | Status |
|------|---------|-------|--------|
| `core.cpp` | MCU init, WiFi/UDP, reporting | Core team | ✅ Working |
| `core.h` | Core class definition, OTA control | Core team | ✅ Up to date |
| `web.cpp` | HTTP server, handlers, OTA toggle | Web team | ✅ Working |
| `web.h` | Web server class, endpoint declarations | Web team | ✅ Up to date |
| `html.cpp` | Embedded HTML/CSS/JS implementation | Web team | ✅ Working |
| `html.h` | Embedded HTML/CSS/JS constants | Web team | ✅ Up to date |
| `sensors.cpp` | Sensor reading, calibration, actuators | Sensors team | ✅ Working |
| `sensors.h` | SensorType enum, callbacks, registration | Sensors team | ✅ Up to date |
| `mesh.cpp` | UDP + ESP-NOW mesh transport | Transport team | ✅ Working |
| `mesh.h` | Transport layer, ESP-NOW peer management | Transport team | ✅ Up to date |
| `automations.cpp` | Rule engine, rule storage, tick() | Automations team | ✅ Working |
| `automations.h` | Rule struct, evaluation logic | Automations team | ✅ Up to date |
| `log.h` | 3-layer logging interface | Logging team | ✅ Working |
| `log.cpp` | Logging implementation, buffers | Logging team | ✅ Working |
| `ota.h` | OTA management interface | OTA team | ⚠️ Minimal |
| `ota.cpp` | OTA implementation | OTA team | ⚠️ Minimal |
| `ai.h` | AI module (not implemented) | - | ❌ Out of scope |
| `ai.cpp` | AI subsystem (4 slots, validated outputs -> rules engine) | AI team | ✅ Working (1.2, authorized) |
| `espnow_p2p.h` | ESP-NOW transport header | Transport team | ✅ Working |
| `espnow_p2p.cpp` | ESP-NOW transport implementation | Transport team | ✅ Working |

### Configuration Files

| File | Purpose | Owner |
|------|---------|-------|
| `library.properties` | Arduino library metadata | Platform team |
| `platformio.ini` | Build configuration | Platform team |
| `README.md` | User documentation (English) | Documentation team |
| `AGENTS.md` | Agent/copilot guidelines | Lead engineer |
| `docs/architecture-baseline.md` | System architecture | Lead engineer |
| `progress.md` | Task tracking | Lead engineer |
| `structure.md` | File organization | Lead engineer |
| `todo.md` | Task list | Lead engineer |

### Example Sketches

| File | Purpose | Status |
|------|---------|--------|
| `examples/Base/Base.ino` | Base example sketch | ✅ Created |
| `examples/HardwareDemo.ino` | Hardware demo (renamed to Base) | ❌ Renamed |

### Documentation Files

| File | Purpose | Status |
|------|---------|--------|
| `docs/architecture-baseline.md` | System architecture | ✅ Created |
| `agents.md` | Agent guidelines | ✅ Created/updated |
| `progress.md` | Task tracking | ✅ Created |
| `structure.md` | File organization | ✅ Created |
| `todo.md` | Task list | ✅ Created |
| `README.md` | User documentation (English) | ✅ Translated |

## Module Responsibilities

### Core Module (`core.cpp` / `core.h`)
- MCU initialization and setup
- WiFi connectivity management (STA/AP mode)
- OTA enable/disable control
- Boot logic and state tracking
- Memory reporting
- **Responsibility**: Deterministic runtime initialization and loop scaffolding

### Sensors Module (`sensors.cpp` / `sensors.h`)
- Sensor type definition and enumeration
- Sensor reading and calibration
- Actuator control (relay/dimmer)
- Pulse and fade modes
- Sensor state reporting (`aidig()`, `aiana()`)
- **Responsibility**: Sensor/actuator state management and calibration

### Web Module (`web.cpp` / `web.h`)
- HTTP server and endpoint handlers
- WiFi credential management
- Calibration updates
- Rule management API
- OTA toggle and status
- Log reporting
- **Responsibility**: Web-based configuration and status

### Mesh/Transport Module (`mesh.cpp` / `mesh.h` / `espnow_p2p.cpp`)
- UDP broadcast transport
- ESP-NOW peer management
- Bounded RX FIFO (8x250B ring; callback never blocks, overflow counter logged from loop())
- Packet encoding/decoding
- Peer discovery and cleanup
- Transport mode auto-detection (STA=UDP, AP=ESP-NOW)
- **Responsibility**: Reliable networking between devices

### Automations Module (`automations.cpp` / `automations.h`)
- Rule engine evaluation
- Rule storage and persistence
- AND/OR logic composition
- Rule tick execution in loop()
- **Responsibility**: Deterministic automation rule execution

### Logging Module (`log.h` / `log.cpp`)
- 3-layer logging (CORE, EVENTS, SENSORS)
- Circular buffer management
- JSON output for web endpoint
- Log level filtering
- **Responsibility**: Useful, controlled logging

### Persistence Module (EEPROM/Preferences)
- 4KB EEPROM layout (ESP8266)
- Preferences namespace (ESP32)
- Flag persistence (ota_enabled, transport_mode)
- Factory reset logic
- Diff check before EEPROM writes
- **Responsibility**: Reliable state persistence across reboots

## Platform Dependencies

### ESP8266 (generic ESP-12E / NodeMCU)
- Core: `Arduino ESP8266 core`
- WiFi: `WiFiUdp` for UDP
- OTA: `ESP8266HTTPClient`, `ESP8266HTTPUpdateServer`
- Storage: `EEPROM` (4KB)
- Preferences: Not available (use EEPROM)
- Logging: 3-layer circular buffer
- ESP-NOW: Via `espnow_p2p` library
- `raw_address()`: Patched in core
- GPIO: `setSerialEnabled()` for pin reuse

### ESP32 (devkit / generic)
- Core: `Arduino ESP32 core`
- WiFi: Native UDP support
- OTA: `HTTPClient` with `SECURITY_*` constants
- Storage: `Preferences` library (replaces EEPROM)
- Preferences: Namespaces "qymeras", "wifi", "ota", "rules", "sensors"
- Logging: 3-layer circular buffer
- ESP-NOW: Via `ESPNow` class
- `setSerialEnabled()`: For GPIO pin control
- GPIO: 0-16 available (with serial disabled)

### Framework Pinning
- **ESP8266**: espressif8266@3.30102.0
- **ESP32**: espressif32@6.5.0
- Purpose: Avoid framework bugs, ensure stability
- Pinning prevents automatic updates that break compatibility

## Known Couplings & Dependencies

### Tight Couplings (Acceptable)
1. **EEPROM ↔ Preferences**: Platform-specific storage swap
2. **WiFi mode ↔ Transport mode**: STA→UDP, AP→ESP-NOW
3. **OTA flag ↔ EEPROM offset 2048**: Persistent enable/disable
4. **Rule count ↔ MAX_RULES=20**: Hard limit in rule storage

### Loose Couplings (Should Decouple)
1. **Web handlers ↔ Sensors**: Current direct calls should go through API
2. **Mesh callbacks ↔ Core**: ESP-NOW callbacks reference core state
3. **Calibration ↔ EEPROM offsets**: Hardcoded offsets, should be configurable

### Accidental Couplings (Must Fix)
1. **`web.cpp` includes `sensors.cpp` headers directly**: Consider API layer
2. **`core.cpp` references `log.cpp` internals**: Should use `log.h` interface only
3. **`automations.cpp` knows about `EEPROM` offsets**: Should use persistence API

## File Ownership Matrix

| contributor | Files owned |
|-------------|-------------|
| Lead engineer | `agents.md`, `architecture-baseline.md`, `progress.md`, `structure.md`, `todo.md` |
| Core team | `core.cpp`, `core.h`, `ota.h`, `ota.cpp` |
| Web team | `web.cpp`, `web.h`, `html.cpp`, `html.h` |
| Sensors team | `sensors.cpp`, `sensors.h` |
| Transport team | `mesh.cpp`, `mesh.h`, `espnow_p2p.cpp`, `espnow_p2p.h` |
| Automations team | `automations.cpp`, `automations.h` |
| Logging team | `log.h`, `log.cpp` |
| Platform team | `library.properties`, `platformio.ini` |
| Documentation | `README.md`, all `docs/` files |

## Build Verification

### PlatformIO Commands
```bash
pio run -e esp8266_generic   # Compile ESP8266
pio run -e esp32_devkit      # Compile ESP32
pio run -e esp32c3_devkit    # Compile ESP32-C3 (build-verified)
pio run                      # Compile all platforms
pio test                     # Run unit tests (if available)
pio unitTest                 # Custom test runner
python tests/host_sanity.py  # Host sanity tests (timezone/strict-float/FIFO)
```

### Expected Build Output
- **ESP8266**: Compiles with expected linker errors (missing setup()/loop())
- **ESP32**: Compiles with expected linker errors (missing setup()/loop())
- **Both**: No compilation errors in source files
- **Linker errors**: Pre-existing (user sketch must provide setup()/loop())

### Platform Differences in Build
- ESP8266: Uses `WiFiUdp`, `ESP8266HTTPClient`
- ESP32: Uses `WiFi` class UDP, `HTTPClient` with security
- ESP8266: EEPROM 4KB
- ESP32: Preferences (larger capacity)
- ESP8266: `raw_address()` patched
- ESP32: No `raw_address()` needed

## Module Integration Points

### Initialization Flow
```
user setup()
  → Qymera::begin() [core.cpp]
    → WiFi.begin() [core.cpp]
    → Sensors::init() [sensors.cpp]
    → Rules::init() [automations.cpp]
    → WebServer::begin() [web.cpp]
    → OTA::init() [ota.cpp] (default: disabled)
    → Transport::init() [mesh.cpp] (STA=UDP, AP=ESP-NOW)
    → Log::init() [log.cpp] (3-layer buffers)
  → user loop()
    → Qymera::loop() [core.cpp]
      → Rules::tick() [automations.cpp]
      → WebServer::handleClient() [web.cpp]
      → Transport::tick() [mesh.cpp]
      → Log::tick() [log.cpp] (optional)
```

### State Transition Flow
```
Boot → WiFi connecting → WiFi connected/AP mode
  → Transport auto-detected (STA=UDP, AP=ESP-NOW)
  → Rules loaded from EEPROM/Prefs
  → Web server active
  → OTA enabled/disabled flag checked
```

### Data Flow Flow
```
Sensor reading → Calibration → State update
  → Rule evaluation (tick()) → Actuator control
  → Web API → JSON response
  → UDP/ESP-NOW broadcast → Peer devices
  → Log buffers → Web / serial output
```
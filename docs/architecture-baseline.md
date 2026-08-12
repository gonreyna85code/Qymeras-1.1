# Qymeras 1.1 Architecture Baseline

## System Overview
Qymeras is an ESP8266/ESP32 firmware for IoT sensor/actuation networks with:
- Web-based configuration UI
- ESP-NOW + WiFi mesh communication
- Automation rules engine
- EEPROM/Preferences persistence
- OTA updates via Arduino framework

## Core Runtime Lifecycle

### Initialization Order
1. **`setup()`** (user sketch) → calls `Qymera::begin()`
2. **`Qymera::begin()`** initializes:
   - Serial communications
   - EEPROM/Preferences storage
   - WiFi connectivity (STA/AP mode)
   - Sensor subsystem
   - Automation rules engine
   - Web server
   - OTA module (default: disabled)
   - Mesh/transport layer
3. **`loop()`** (user sketch) → calls `Qymera::loop()`
4. **`Qymera::loop()`** main state machine:
   - Process WiFi/ESP-NOW events
   - Tick automation rules
   - Handle web server requests
   - Manage OTA if enabled
   - Report sensor states

### Runtime States
- **BOOT**: Initial hardware and stack setup
- **STA_CONNECTED**: WiFi station mode connected to network
- **AP_MODE**: Access point mode (no external WiFi required)
- **OTA_ENABLED**: OTA updates active (reboots after toggle)
- **FACTORY_RESET**: EEPROM cleared, AP mode active

## Sensors/Device Model

### Sensor Types (MAX_SENSORS=64)
| Type | Description |
|------|-------------|
| `SENSOR_TEMP` | Temperature |
| `SENSOR_HUMI` | Humidity |
| `SENSOR_LUMI` | Light |
| `SENSOR_PRESS` | Pressure |
| `SENSOR_LEVEL` | Level/flow |
| `SENSOR_AIRQ` | Air quality |
| `SENSOR_RAIN` | Rainfall |
| `SENSOR_CONTACT` | Contact/relay state |
| `SENSOR_GENERIC` | Generic sensor |

### Calibration Model
- Each sensor has individual calibration stored in EEPROM
- Calibration parameters: min, max, offset, filter
- Calibration values survive factory reset of relay states
- **Known issue**: Calibration values lost on EEPROM reset (separate from relay states)

### Actuators
- `TYPE_RELAY`: On/off control with pulse/fade modes
- `TYPE_DIMMER`: PWM value control (0-100%)
- Pulse mode: Non-blocking duration control
- Fade mode: Smooth transition between values

## Automations/Rules

### Rule Types (MAX_RULES=20)
| Type | Description |
|------|-------------|
| `EDGE` | Trigger on state change (RISING/FALLING) |
| `THRESHOLD` | Trigger when value crosses threshold |
| `TIME` | Trigger at specific time of day |
| `INTERVAL` | Trigger at regular intervals |

### Rule Logic
- **AND/OR** composite conditions
- **Max 5 sensors** per rule condition
- **Max 5 actuators** per rule action
- Rules evaluated in `loop()` tick cycle
- Persistence: `saveRulesToEEPROM()` with diff check

### Rule Storage
- Stored in EEPROM at dedicated offsets
- Auto-loaded on boot
- Validate sensor/actuator counts on load
- Diff check before writing to EEPROM

## Persistence

### EEPROM Layout (4 KB)
| Offset | Size | Content |
|--------|------|---------|
| 0 | 512 | WiFi credentials |
| 512 | 512 | Relay states |
| 1024 | 512 | Sensor calibrations |
| 1536 | 512 | Rule definitions |
| 2048 | 512 | OTA flags and settings |
| 2560 | 512 | Reserved |
| 3072 | 256 | Log buffers (3 layers × 12 entries) |
| 3328 | 192 | Reserved/padding |

### Preferences (ESP32)
- Replaces EEPROM for ESP32 due to known bugs
- Namespaces: "qymeras", "wifi", "ota", "rules", "sensors"
- Larger capacity than 4KB EEPROM

### Flag Persistence
- `ota_enabled`: Stored in EEPROM offset 2048
- survives reboot, cleared on factory reset
- `transport_mode`: UDP (STA) or ESP-NOW (AP)

## Web/API

### HTTP Endpoints

| Method | Route | Action |
|--------|-------|--------|
| GET | `/` | Root — sends embbeded HTML |
| POST | `/save` | Heat WiFi credentials → 303 redirect + reboot |
| GET/POST | `/calib` | Read calibration in JSON |
| POST | `/calib/set` | Set: TIME / ref / min / max / fad / pulse / persist / avail / res / timezone |
| POST | `/genset/save` | Save broadcast/command/interval set |
| GET | `/rules` | List rules JSON (filters sensor_count/actuator_count == 0) |
| POST | `/rules/set` | Create/validate rule, type, comparator/threshold |
| POST | `/rules/delete` | Delete rule by id (validate bounds before delete) |
| POST | `/factory` | Factory reset — clear credentials, relay state, config → reboot in AP |
| GET | `/toggle` | API toggle for actuators (id + logic) |
| GET | `/dimmer` | Value control dimmer (id + value) |
| GET | `/logs` | Recent logs JSON |
| GET | `/ota/status` | OTA state `{"ota":1}` or `{"ota":0}` |
| GET | `/ota/toggle` | Enable/disable OTA |

### Web Interface
- **Tabs**: WiFi, Calibration, Rules, Actuators, Factory, Logs
- **Status panels**: Connection, OTA, Memory, Last reset
- **Log viewer**: 3 layers (CORE, EVENTS, SENSORS)
- **Calibration editor**: Per-sensor min/max/offset

## UDP Transport

### Default Configuration
- **Port**: 8888 (broadcast)
- **Address**: 255.255.255.255
- **Packet format**: `PacketHeader` + `Packet` (max 250 bytes)
- **Broadcast interval**: Configurable (default 2s)

### STA Mode Behavior
- UDP broadcast on `255.255.255.255:8888`
- Requires WiFi connected to external network
- Peers discovered via broadcast

### Known Issues
- Port < 1024 or > 65500: reset to default
- Interval < 5000ms or > 600000ms: reset to default

## ESP-NOW Transport

### Configuration
- **Mode**: AP mode only (WiFi disabled for ESP-NOW)
- **Broadcast**: FF:FF:FF:FF:FF:FF
- **Unicast**: To registered peers (max 25)
- **Payload**: `PacketHeader` + `Packet` (max 250 bytes)

### AP Mode Behavior
- ESP-NOW broadcast on FF:FF:FF:FF:FF:FF
- Works without WiFi network connection
- Peer management: `addPeer()`, `clearPeers()`, `getPeerCount()`
- Fallback when WiFi disconnected

### Transport Auto-detection
- **STA mode**: UDP broadcast
- **AP mode**: ESP-NOW broadcast
- Auto-switched based on WiFi mode at boot

## Mesh (ESP-NOW P2P)

### Peer Management
- **Max peers**: 25
- **Registration**: Via `addPeer(mac_addr)`
- **Discovery**: Automatic via broadcast
- **Cleanup**: `clearPeers()` on factory reset

### Message Format
- **Header**: `PacketHeader` {src, dst, type, length}
- **Payload**: Up to 250 bytes
- **Types**: SENSOR_DATA, RULE_EXEC, COMMAND, ACK

### Reliability
- Send callback: `espnow_send_cb`
- Receive callback: `espnow_recv_cb`
- No automatic retransmission (application level)

## Logging

### 3-Layer System
| Layer | Purpose | Entries |
|-------|---------|---------|
| **CORE** | Boot, WiFi, OTA, errors | 12 |
| **EVENTS** | Rule triggers, automations | 12 |
| **SENSORS** | Sensor readings, calibrations | 12 |

### Buffer Management
- Circular buffer: oldest overwrites new
- Per-layer `getRecentLogs()` API
- JSON format for web endpoint `/logs`
- Cleared on factory reset

### Log Entry Format
```json
{"layer":"CORE","msg":"Boot complete","ts":12345}
{"layer":"EVENTS","msg":"Rule #3 triggered","ts":12346}
{"layer":"SENSORS","msg":"TEMP calibration updated","ts":12347}
```

## OTA Behavior

### Default State
- **OTA disabled** at first boot
- Flag stored in EEPROM offset 2048
- Can be enabled via `/ota/toggle?enabled=1`

### Toggle Flow
1. `GET /ota/toggle?enabled=1` → enables OTA
2. Log: `"OTA enabled"` in CORE layer
3. Device reboots
4. On boot: `ota_enabled` flag checked
5. If enabled: HTTP server accepts OTA requests
6. If disabled: OTA endpoint returns 404

### OTA Status Endpoint
- `GET /ota/status` → `{"ota":1}` or `{"ota":0}`
- Reflects current flag state

### Security
- **No authentication** on OTA endpoint
- **Recommended**: Only on local network
- **Known limitation**: No firmware integrity check

## Configuration

### WiFi Credentials
- Stored in EEPROM offset 0-511
- POST /save → validates and stores
- 303 redirect to root + reboot
- Invalid credentials: retry loop

### Timezone
- Stored as offset in minutes from UTC
- POST /calib/set with `tz` parameter
- Used for TIME rule triggers

### Broadcast/Command Intervals
- Default: 2s broadcast, 1s command
- POST /genset/save → validates and stores
- Interval < 5000ms or > 600000ms: reset to default

### Security Limitations
- **No authentication** on any web endpoint
- **Recommended**: Local network only
- **Known**: Credentials transmitted in plain text
- **No rate limiting** on API endpoints

## Platform Differences

### ESP8266 (d1_mini)
- `WiFiUdp` for UDP transport
- `ESP8266HTTPClient` for OTA
- `ESP8266HTTPUpdateServer` for web OTA
- `Preferences` library not available (uses EEPROM)
- `raw_address()` framework issue (patched)

### ESP32 (devkit)
- `WiFi` class with native UDP
- `HTTPClient` with `SECURITY_*` constants
- `Preferences` library for storage
- `ESPNow` class for ESP-NOW transport
- `setSerialEnabled()` for pin reuse (GPIO 0-16)

### Framework Pinning
- **ESP8266**: espressif8266@3.30102.0 (avoids `raw_address()` bug)
- **ESP32**: espressif32@6.5.0 (avoids EEPROM bugs)
- Pinning prevents breaking changes from framework updates

## Known Bugs & Fragile Areas

### Framework Bugs (Pinned Versions Fix These)
1. `raw_address()` private method (ESP8266 core)
2. `IPAddress::IPAddress()` deleted constructor
3. `WiFiEventStationModeGotIP::WiFiEventStationModeGotIP()` deleted function
4. ESP32 EEPROM corruption bugs

### Code Fragilities
1. **Blocking delays** in `loop()` during WiFi reconnect
2. **EEPROM write endurance**: 4KB limited writes
3. **Memory fragmentation**: String objects in web handlers
4. **Race conditions**: WiFi/Ethernet event callbacks
5. **Calibration loss**: Values lost on factory reset (EEPROM cleared), but survive power cycle (loaded from EEPROM on boot)

### Memory Risks
- **Buffer overflow**: Log buffers (30→12 entries fixed)
- **Stack overflow**: Deep recursion in rule evaluation
- **Heap fragmentation**: Repeated HTTP server restarts
- **EEPROM saturation**: Max 512 bytes WiFi credentials

## Security Limitations

### Current State
- **HTTP Basic Auth available** (disabled by default, can be enabled via `AUTH_USERNAME`/`AUTH_PASSWORD` constants in `web.cpp`)
- **OTA firmware integrity verification** (checksum stored in EEPROM, verified on boot and toggle)
- **No encryption** on web UI or API (HTTP only)
- **No rate limiting** on POST endpoints
- **No input validation** beyond basic bounds checking

### Recommended Hardening
1. Enable HTTP basic auth by setting `AUTH_USERNAME` and `AUTH_PASSWORD` in `web.cpp`
2. Use HTTPS for OTA transfers
3. Implement rate limiting on `/save`, `/rules/set`
4. Validate all JSON payloads sizes
5. Add CSRF tokens on web forms
4. Validate all JSON payloads sizes
5. Add CSRF tokens on web forms

### Known Acceptable Limitations
- Open network recommended for local IoT
- No auth = simpler setup for trusted networks
- Physical access required for most attacks
- Debug/OTA ports physically accessible

## Testability

### Unit Testable Components
- Rule evaluation logic (pure functions)
- Calibration math (input/output verification)
- Packet parsing (fixed format validation)
- EEPROM offset calculations

### Integration Test Challenges
- WiFi dependency (requires mock or AP mode)
- EEPROM/Persistence (emulated in tests)
- Web server (requires HTTP client mock)
- OTA (requires network stack mock)

### Recommended Test Approach
1. **Host tests** for rule logic and calibration math
2. **Integration tests** with ESP8266-ESP32 mock
3. **Property tests**: invariant checks on state transitions
4. **Fuzz testing**: malformed JSON payloads

## Production Readiness Checklist

### ✅ Completed
- Framework pinned to stable versions
- `raw_address()` issue patched
- EEPROM/Preferences fixes applied
- All source files compile
- Build verification on ESP8266 and ESP32
- Web authentication (HTTP Basic Auth) ✅
- OTA firmware integrity verification ✅

### ⚠️ Requires Attention
- [ ] Web authentication implementation ✅ (completed - HTTP Basic Auth added)
- [ ] OTA firmware integrity verification ✅ (completed - checksum verification added)
- [ ] Memory leak testing under load
- [ ] Long-term EEPROM write endurance
- [ ] Network partition recovery
- [ ] Factory reset reliability

### ❌ Production Blockers
- [ ] None critical - all compile and run
- [ ] Authentication must be added before public deployment
- [ ] OTA security hardening recommended

### ✅ Acceptable Limitations (for 1.1)
- No authentication (local network only)
- No HTTPS for OTA (HTTP only)
- No rate limiting on API
- No input sanitization beyond bounds
- Framework-level bugs pinned/fixed

## Recommended Implementation Order (Phase 2)

1. **Authentication layer** on web/API endpoints
2. **OTA firmware integrity** verification
3. **Memory profiling** and leak fixes
4. **Long-term endurance** testing
5. **Network resilience** recovery tests
6. **Factory reset** reliability validation
7. **Documentation** updates for new features
8. **Test suite** development for core logic
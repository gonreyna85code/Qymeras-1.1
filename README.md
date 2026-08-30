# Qymera - Smart Automation Firmware

Qymera turns your ESP8266 or ESP32 into a complete IoT node: reads sensors, controls actuators, and executes automation rules — all from a built-in web UI with EEPROM persistence and zero internet dependency after initial setup.

**Status:** **CODE FREEZE / PRODUCTION BASELINE** (`main`, HEAD `c714e37`). Code is frozen for Qymera 1.1; only hardware validation remains (24h memory soak, factory-reset hw test, endurance, additional ESP32-family hardware validation). | Built-in web server | UDP + ESP-NOW mesh | EEPROM/Preferences persistence | Arduino Library

## Versions & Branches

| Version | Where | Status |
|---------|-------|--------|
| **Qymera 1.1** | `main` | **CODE FREEZE / PRODUCTION BASELINE** — this tree. ESP8266 + ESP32; UDP + ESP-NOW mesh; web server with basic UI; EEPROM/Preferences persistence; automations. |
| **Qymera 1.2** | `feature/GUI` | Next milestone: built-in web GUI overhaul (device cards, automation wizard, bilingual ES/EN). Not merged into 1.1. |
| **Dashboard / AI** | `feature/ai-experiments` (+ future) | Separate development direction: optional external AI assistant + cloud dashboard. Kept out of the 1.1 production tree. |

---

## Quick Start

### 1. What You Need

| Component | Required? |
|-----------|-----------|
| ESP8266 (NodeMCU, Wemos D1 Mini, etc.) or ESP32 (DevKit, etc.) | Yes |
| USB cable (data capable) | Yes |
| Optional sensors/actuators (DHT22, relays, etc.) | Optional |

### 2. Install Qymera as an Arduino Library

Qymera is distributed as an Arduino Library. There are three ways to install it:

#### Option A: Arduino IDE (Recommended)

1. Download this repository (click **Code → Download ZIP**)
2. In Arduino IDE: **Sketch → Include Library → Add .ZIP Library...**
3. Select the downloaded ZIP file
4. The library is now available as `#include <Qymera.h>`

#### Option B: Arduino IDE (Manual)

1. Clone or copy the `src/` folder and `library.properties` into your Arduino
   libraries directory (e.g. `~/Documents/Arduino/libraries/Qymera/`)

#### Option C: PlatformIO

```ini
[env:your_board]
platform = <your_platform>
board = <your_board>
lib_deps =
    https://github.com/gonreyna85code/Qymera.git
```

### 3. Create Your First Sketch

Use the built-in [Base example](examples/Base/Base.ino) as a starting point
(`main.cpp` is the PlatformIO entry point). The library handles WiFi, the web
server, UDP mesh, and automation logic — your sketch only needs to implement
three hooks under the `Qymera` namespace:

- `Qymera::init()` &mdash; initialize hardware libraries (Wire, I2C, etc.)
- `Qymera::report()` &mdash; read sensors and report values via `Qymera::xxx()`
- `Qymera::onCommand(...)` &mdash; handle custom commands from remote devices

```cpp
#include <Qymera.h>
#include <Wire.h>

void Qymera::init() {
  Wire.begin();
  // initialize your hardware here
}

void Qymera::report() {
  // read your sensors
  Qymera::temperature("Office", 23.5f);
  Qymera::humidity("Soil", 65);
}

void Qymera::onCommand(uint32_t, uint8_t, int, bool) {
  // optional: react to remote commands
}

void setup()   { Qymera::begin(); }
void loop()    { Qymera::loop(); }
```

Everything your sketch needs is exposed under `Qymera::` (lifecycle, sensors,
actuators, `Qymera::setSerialEnabled()`); no other namespace needs to be
spelled out in `main.ino`.

### 4. First-Time Setup

1. **Upload** the sketch to your device.
2. Connect to the WiFi network **`QymeraSetup`** (no password).
3. Open **`http://192.168.4.1`** in your browser.
4. Go to the **NETWORK** tab, enter your home WiFi SSID and password.
5. The device reboots and connects to your network.

### 5. Configure Sensors and Rules

- **SETTINGS** tab &mdash; calibrate sensors, set offsets, timezone, fade/pulse/persist
- **AUTOMATIONS** tab (Rules) &mdash; create automation rules
- **DEVICES** tab &mdash; control relays and dimmers in real time

---

## Supported Sensors

Qymera supports **10 sensor types** plus **2 actuator types**. Each entity has
individual calibration (offset, min/max, availability, persistence, pulse/fade
options).

| Sensor | API | Typical Hardware |
|--------|-----|-------------------|
| Temperature | `Qymera::temperature()` | DHT22, DS18B20, NTC |
| Humidity | `Qymera::humidity()` | DHT22, soil moisture |
| Light | `Qymera::luminosity()` | Photoresistor, BH1750 |
| Pressure | `Qymera::pressure()` | BMP280, BME280 |
| Level | `Qymera::level()` | Ultrasonic, float switch |
| Air Quality | `Qymera::airQ()` | MQ135, SDS011 |
| Rain | `Qymera::rain()` | Rain drop sensor |
| Contact | `Qymera::contact()` | Reed switch, door sensor |
| Generic | `Qymera::custom()` | Any analog/digital value |
| Time | `Qymera::rtc()` / `Qymera::ntp()` | RTC module or NTP (clock stays UTC; timezone is an offset per node) |
| Relay (actuator) | `Qymera::relay()` | Digital relay, latching |
| Dimmer (actuator) | `Qymera::dimmer()` | LED strip, fan, PWM |

Sensor type enum (`/calib` JSON `type` field): 1=LUMI, 2=HUMI, 3=TEMP, 4=PRESS,
5=LEVEL, 6=AIRQ, 7=RAIN, 8=DIMMER, 9=RELAY, 10=TIME, 11=GENERIC, 12=CONTACT.

---

## Automation: Up to 20 Rules

Create rules combining up to **5 sensors** and **5 actuators** per rule.

### Rule Types

- **Edge** &mdash; triggers on state change (RISING / FALLING) for boolean
  entities (contact, rain)
- **Threshold** &mdash; triggers when a calibrated sensor value crosses a
  threshold. Combine multiple conditions with AND/OR logic
- **Time** &mdash; triggers once per day at a local time (uses NTP or local
  RTC). Fires once per calendar day
- **Interval** &mdash; triggers every N ms while the date window holds

All rules support execution delay, per-rule cooldown, and logic composition.
Note: THRESHOLD has no hysteresis — always set a `cooldown_ms`; TIME
`time_s` is truncated to minutes; TIME/INTERVAL have no catch-up after a reboot.

---

## Actuator Control

**Relays:** ON / OFF / TOGGLE / PULSE (activate for X ms then release).
Supports state persistence across reboots (UID-matched restore before the first
report — no boot glitch).

**Dimmers:** Smooth fade transitions between 0-100% brightness for LEDs, fans,
and other PWM loads.

---

## Communication: Web + Local Network

### Zero USB Required

All configuration is done through the web UI. After initial setup, the device
operates independently — no internet needed. HTTP server runs on port 80,
reading all config from EEPROM / Preferences (ESP32).

**Main tabs:** DEVICES (actuators) · AUTOMATIONS (rules) · SETTINGS
(calibration + remote entities) · LOGS · NETWORK (WiFi, OTA, factory reset)

### Mesh

- **STA mode:** UDP broadcast discovery/announcement (batched datagrams,
  protocol v4/v5; up to 29 packets per datagram).
- **AP mode:** ESP-NOW broadcast (bounded RX FIFO, peer management).
- Remote entities are visible/controllable/calibratable across nodes by
  POSTing to the owning node's IP; remote config is verified over HTTP and never
  falls back silently to the local node.

### HTTP API (curl)

```bash
# Read everything (entities, one JSON array): resolve NAME → uid and index
curl http://<device-ip>/calib

# Toggle an actuator by its uid
curl -X POST http://<device-ip>/toggle -d "id=<uid>"

# Set dimmer level (0-100) by uid
curl -X POST http://<device-ip>/dimmer -d "id=<uid>&value=75"

# Read automation rules and device logs
curl http://<device-ip>/rules
curl http://<device-ip>/logs
```

Key facts:
- Commands address **entity `id` (uid)**; rules address **slot indexes**.
- Remote entities must be addressed on their **owner's IP** (the `ip` field in
  `/calib`).
- Rate limit: 6 requests / 2 s burst on state-changing endpoints (7th → `429`).
- Full API reference and architecture: see `docs/architecture-baseline.md`.

---

## Supported Platforms

| Board | Status |
|-------|--------|
| ESP8266 (NodeMCU, Wemos D1 Mini, etc.) | Fully Tested (hardware) |
| ESP32 (DevKit, WROOM, etc.) | Fully Tested (hardware) |
| ESP32-S2, ESP32-S3, ESP32-C3 | Build-verified (`esp32c3_devkit` env); hardware validation pending |

Platform auto-detection via preprocessor defines in `config.h`.

---

## Data Persistence

All web configuration survives power loss — only live sensor readings are lost
on reset:

| Region (4 kB EEPROM / Preferences) | Size |
|------------------------------------|------|
| WiFi credentials | 100 B |
| General settings (ports/interval) | 12 B |
| Sensor calibration (UID slot: magic+version+uid+pers_state+min/max+correction+avail+persist+pulse+ms+fade) | 40 × 34 B = 1360 B |
| Automation rules | 1600 B |
| OTA device identity token + enable flag | 4 B + 1 B |

Factory reset behaviour: `POST /factory` clears credentials, calibration, rules
and OTA flag, then reboots into `QymeraSetup` AP mode.

---

## Security

- **Authentication infrastructure present but dormant** (HTTP Basic Auth gate
  off by default; placeholder creds `admin:qymera123` stay in the firmware, not
  in client JS). Recommended for trusted local networks only.
- **OTA device identity check:** a chip-unique token (`GET_CHIP_ID()`) is stored
  and verified on boot/toggle — a provisioning check, NOT a firmware
  authenticity hash.
- **Rate limiting:** burst-tolerant 6 req / 2 s on state-changing endpoints.
- **Strict input validation:** rejects empty/trailing-junk/overflow/NaN/Inf and
  enforces type ranges on every write path.
- Known limitations: HTTP only (no HTTPS for OTA/web); no CSRF tokens; no
  full firmware signing (SHA-256 deferred to Phase 3+).

Use only on a trusted local WiFi network; block external access via router
firewall; use a VPN for remote access.

---

## Troubleshooting

**Device appears but won't join WiFi?**
- First-time flash: ensure the USB cable supports data (not power-only)
- Wait 10 seconds after power-on before scanning for networks
- Verify router allows unknown MAC addresses on the 2.4 GHz band

**Sensors not reporting values?**
- Register the sensor in the SETTINGS tab, add calibration values, and save
- Verify pin assignments match your hardware
- Remote entities only appear while their owner announces (< ~30 s — `MESH_TIMEOUT`)

---

## Advanced Configuration

Edit `config.h` for platform-specific settings and limits:

```cpp
#define MAX_SENSORS             64   // max entities in memory
#define MAX_PERSISTED_SENSORS   40   // max persisted calibration slots
#define MAX_RULES               20   // max automation rules
#define EEPROM_SIZE             4096 // storage size
#define BROADCAST_INTERVAL      5000 // mesh announce interval (ms)
```

---

## Contributing

Issues and PRs welcome. To build and verify:

```bash
pip install platformio
pio run -t upload --monitor -e esp32_devkit   # ESP32
pio run -e esp8266_generic                     # ESP8266
python tests/host_sanity.py                    # host test suite (45 checks)
```

---

## Roadmap

- **Qymera 1.2** (`feature/GUI`): built-in web GUI overhaul — device cards,
  automation wizard, bilingual ES/EN UI. Not merged into 1.1.
- **Dashboard / AI** (`feature/ai-experiments` + future): optional external AI
  assistant subsystem (authorized per `AGENTS.md`, kept out of the 1.1
  production tree) and a cloud dashboard.
- **Future:** MQTT · Zigbee/Z-Wave · Matter · graphing dashboard · email/SMS
  notifications · mobile app.

---

License: MIT — see `LICENSE` file (per `library.properties`).
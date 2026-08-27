# Qymera - Smart Automation Firmware

Qymera turns your ESP8266 or ESP32 into a complete IoT node: reads sensors, controls actuators, and executes automation rules — all from a built-in web UI with EEPROM persistence and zero internet dependency after initial setup.

**Status (this branch):** `feature/ai-experiments` — AI subsystem experimental line (optional `ai.cpp`, `/ai/chat` relay, browser agent tool-loop, virtual `AIDIG`/`AIANA` sensors feeding the rules engine). Opt-in: zero traffic when disabled.
**Production (1.1 MVP):** the AI-free tree lives on `main` (`b2a9b01`, HEAD `5e46e12`). Do not port AI code there without an explicit request.

| Branch | Purpose |
|--------|---------|
| `main` | **Production 1.1 MVP — no AI.** Hardened deterministic core, 3-env green, host suite 79/79. |
| `feature/ai-experiments` | **AI implementation line** (this branch): optional LLM prompt slots, browser chat agent, virtual sensors via `aidig()`/`aiana()`, CONTROL tool-calls through audited actuation primitives. |

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

Use the built-in [HardwareDemo example](examples/HardwareDemo/HardwareDemo.ino)
as a starting point. The library handles WiFi, the web server, UDP mesh, and
automation logic — your sketch only needs to:

- `initSatellite()` &mdash; initialize hardware libraries (Wire, I2C, etc.)
- `report()` &mdash; read sensors and report values via `sensors::xxx()` API
- `onCommandHook(...)` &mdash; handle custom commands from remote devices

```cpp
#include <Qymera.h>
#include <Wire.h>

void initSatellite() {
  Wire.begin();
  // initialize your hardware here
}

void report() {
  // read your sensors
  sensors::temperature("Office", 23.5f);
  sensors::humidity("Soil", 65);
}

void onCommandHook(uint32_t, uint8_t, int, bool) {
  // optional: react to remote commands
}

void setup()   { core::begin(); }
void loop()    { core::loop(); }
```

### 4. First-Time Setup

1. **Upload** the sketch to your device.
2. Connect to the WiFi network **`QymeraSetup`** (no password).
3. Open **`http://192.168.4.1`** in your browser.
4. Go to the **NETWORK** tab, enter your home WiFi SSID and password.
5. The device reboots and connects to your network.

### 5. Configure Sensors and Rules

- **SETTINGS** tab &mdash; calibrate sensors, set offsets, pin assignments
- **AUTOMATIONS** tab &mdash; create automation rules with the visual wizard
- **DEVICES** tab &mdash; control relays and dimmers in real time

---

## Supported Sensors

Qymera supports **12 sensor types** plus **2 actuator types** (enum values 1..14;
AIDIG=13, AIANA=14 are AI-produced virtual sensors). Each sensor has
individual calibration (offset, min/max, resolution, availability, persistence
options).

| Sensor | Range | Unit | API | Typical Hardware |
|--------|-------|------|-----|-------------------|
| Temperature | -50 to +150 | °C | `sensors::temperature()` | DHT22, DS18B20, NTC |
| Humidity | 0-100 | % | `sensors::humidity()` | DHT22, soil moisture |
| Light | 0-65535 | lux | `sensors::luminosity()` | Photoresistor, BH1750 |
| Pressure | 300-1100 | hPa | `sensors::pressure()` | BMP280, BME280 |
| Level | 0-100 | % | `sensors::level()` | Ultrasonic, float switch |
| Air Quality | GOOD / WARN / BAD | enum | `sensors::airQ()` | MQ135, SDS011 |
| Rain | ON / OFF | bool | `sensors::rain()` | Rain drop sensor |
| Contact | OPEN / CLOSED | bool | `sensors::contact()` | Reed switch, door sensor |
| Generic | any | float | `sensors::custom()` | Any analog/digital value |
| Time | N/A | epoch | `sensors::rtc()` / `sensors::ntp()` | RTC module or NTP |
| AI Digital (13) | TRUE / FALSE | bool | `sensors::aidig()` | LLM-validated digital output |
| AI Analog (14) | 0-100 | % | `sensors::aiana()` | LLM-validated analog output |
| Relay (actuator) | ON / OFF | bool | `sensors::relay()` | Digital relay, latching |
| Dimmer (actuator) | 0-100 | % | `sensors::dimmer()` | LED strip, fan, PWM |

---

## Automation: Up to 20 Rules

Create rules combining up to **5 sensors** and **5 actuators** per rule.

### Rule Types

- **Edge** &mdash; triggers on state change (RISING / FALLING) for booleans,
  useful for motion detection or contact sensor transitions
- **Threshold** &mdash; triggers when a sensor crosses a configurable
  threshold. Combine multiple conditions with AND/OR logic
- **Scheduled** &mdash; triggers at a specific time daily. Uses NTP or local
  RTC time
- **Periodic** &mdash; triggers every N seconds for regular data reporting

All rules support advanced options: execution delay, cooldown period, and
minimum ON/OFF duration.

---

## Actuator Control

**Relays:** ON / OFF / TOGGLE (invert state) / PULSE (activate for X ms then
release). Supports state persistence across reboots.

**Dimmers:** Smooth fade transitions between 0-100% brightness for LEDs, fans,
and other PWM loads.

---

## Communication: Web + Local Network

### Zero USB Required

All configuration is done through the web UI. After initial setup, the device
operates independently — no internet needed. HTTP server runs on port 80,
reading all config from EEPROM.

**Main tabs:**
- **DEVICES** &mdash; Real-time actuator control
- **AUTOMATIONS** &mdash; Visual rule creation wizard
- **SETTINGS** &mdash; Sensor calibration and device configuration
- **NETWORK** &mdash; WiFi management and factory reset

### HTTP API (curl)

Actuator endpoints take the **entry UID** (`id` from `/calib`, not the table index):

```bash
# Factory reset (clears WiFi, returns to AP mode)
curl -X POST http://<device-ip>/factory

# Toggle an actuator (use the uid from /calib)
curl -X POST http://<device-ip>/toggle -d "id=<uid>&state=1"

# Set dimmer value
curl -X POST http://<device-ip>/dimmer -d "id=<uid>&value=75"

# Diagnostics
curl http://<device-ip>/status
```

AI endpoints (this branch, opt-in — see "AI Subsystem" below):

```bash
# Configure a prompt slot (rate limited)
curl -X POST http://<device-ip>/ai/set -d "target=prompt&slot=0&out_type=0&prompt=...&enabled=1"

# Trigger a run for a slot (interval or manual)
curl -X POST http://<device-ip>/ai/run -d "slot=0"

# Per-slot last result
curl http://<device-ip>/ai/status
```

Full API reference and architecture: see `docs/architecture-baseline.md`.

---

## Supported Platforms

| Board | Status |
|-------|--------|
| ESP8266 (NodeMCU, Wemos D1 Mini, etc.) | Fully Tested |
| ESP32 (DevKit, WROOM, etc.) | Fully Tested |
| ESP32-S2, ESP32-S3, ESP32-C3 | Build-verified (`esp32c3_devkit` env); hardware validation pending |

Platform auto-detection via preprocessor defines. Add new boards by adding a
`#define` block in `config.h`.

---

## Data Persistence (EEPROM 4 KB)

All web configuration survives power loss — only live sensor readings are lost
on reset:

| What persists | Size |
|---------------|------|
| WiFi credentials, calibration rules, automation rules | ~2.2 KB total |
| Relay states (if persistence enabled) | 10 bytes |

**Factory reset:** SETTINGS tab → "Factory Reset" button, or HTTP `POST /factory`.

---

## AI Subsystem (this branch — `feature/ai-experiments`, opt-in)

An **optional, fully opt-in** data source: configured LLM prompt slots feed the
existing rules engine as validated virtual sensors. Disabled by default — zero
traffic, deterministic core unaffected.

- **4 prompt slots** in EEPROM block 3087..3966 (`QMAI v1`); providers
  OPENAI / OLLAMA / CUSTOM (configured endpoint URL).
- **Output types:** `DIGITAL` → `sensors::aidig()` (virtual sensor type 13),
  `ANALOG` → `sensors::aiana()` (type 14), `ANALYTIC` (raw text + log),
  `CONTROL` (real tool-calls `set_relay`/`set_dimmer` through the same audited
  actuation primitives as the web API — no direct hardware writes).
- **Browser chat agent:** the AI panel runs the tool-loop **in the browser**
  (same-origin) against a **stateless `/ai/chat` relay**. The device injects the
  tool schema from **PROGMEM** (zero RAM) with `tool_choice:"auto"`, edits
  nothing else, and relays the raw upstream response (upstream error body
  surfaced; ESP8266 timeout-cap reported as 504).
- Device-side runs (`/ai/run` interval/manual) apply strict-validated outputs;
  a failed run invalidates the previous slot result (no stale-valid masking).
- **Platform split:** ESP8266 uses plain HTTP only (TLS was dropped — BearSSL
  buffers exceed the DRAM budget); the effective AI HTTP timeout is clamped to
  20s on ESP8266. ESP32 supports full `https`.
- `api_key` is write-only — never echoed back over the API.

**Security:** open requests copy the provider `api_key` and can request actuation;
run this on trusted local networks only, and keep the HTTP Basic Auth gate
enabled when exposed off-LAN.

---

## Common Use Cases

### Smart Greenhouse
```
→ If temp > 30°C AND humidity > 80%  → turn on fan
→ If soil dry (<30%)                 → activate water pump for 5 min
→ Daily at 06:00                     → lights on
→ Daily at 18:00                     → lights off
```

### Smart Home
```
→ Motion detected 18:00-22:00       → lights to 70% with 1s fade
→ No motion after 23:00             → lights off after 3s
→ Temp < 18°C                        → heater on
```

### Smart Irrigation
```
→ Zone 1 dry AND no rain             → open valve + pump
→ Rain detected                      → close all valves (water saving)
→ Temp < 5°C                         → stop pump (anti-freeze)
```

---

## Security Notes

The 1.1 tree ships with **HTTP Basic Auth infrastructure (dormant by default)**:
all mutating endpoints check credentials when the gate is enabled, but out of the
box requests without an `Authorization` header are served — designed for **trusted
home local networks only**.

Active hardening (this branch): rate limiting on all mutating POSTs
(`/save`, `/genset/save`, `/rules/set`, `/rules/delete`, `/factory`, `/toggle`,
`/dimmer`, `/calib/set`, `/ai/set`, `/ai/run`, `/ai/chat`), OTA gated by a
chip-identity token, strict input validation, and CORS applied to all responses.
See `docs/architecture-baseline.md` → "Security Limitations" for the complete list.

Recommended for home use:

1. Use only on a trusted local WiFi network
2. Change the AP SSID from `QymeraSetup` to a random name via settings
3. Block external access through your router's firewall
4. For remote access, use a VPN or enable the HTTP Basic Auth gate
5. On `feature/ai-experiments`, keep AI slots disabled unless actively used

---

## Troubleshooting

**Device appears but won't join WiFi?**
- First-time flash: ensure the USB cable supports data (not power-only)
- Wait 10 seconds after power-on before scanning for networks
- Verify router allows unknown MAC addresses on 2.4 GHz band

**Sensors not reporting values?**
- Register the sensor in the SETTINGS tab, add calibration values, and save
- Verify pin assignments match your hardware

---

## Advanced Configuration

Edit `config.h` for platform-specific settings. Defaults work for most cases:

```cpp
#define MAX_SENSORS      64   // max sensors in memory
#define MAX_RULES        20   // max rules stored in EEPROM
```

---

## Custom Sensors

Register custom sensors by calling the appropriate API in your `report()` loop.
Values appear immediately in the web UI and can be used in automation rules:

```cpp
float raw = analogRead(A5);
sensors::temperature("MySensor", raw / 4.0f);
```

---

## Contributing

Issues and PRs welcome. To build and monitor via serial:

```bash
pip install platformio
pio run -t upload --monitor -e esp32_devkit
```

---

## Roadmap

**Production (1.1 MVP, `main`):** gate on the 24h soak, factory-reset and storage-endurance hardware checks.
**AI line (`feature/ai-experiments`):** relay hardening, freshness policy for `AIDIG`/`AIANA`, per-slot model override, then fold the stabilized AI subset into a future release.
**Later:** MQTT · Zigbee/Z-Wave · Matter · graphing dashboard · email/SMS notifications · mobile app. Driven by community needs.

---

License: MIT — see `LICENSE` file.

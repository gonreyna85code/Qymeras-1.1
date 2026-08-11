# Qymera - Smart Automation Firmware

Qymera turns your ESP8266 or ESP32 into a complete IoT node: reads sensors, controls actuators, and executes automation rules — all from a built-in web UI with EEPROM persistence and zero internet dependency after initial setup.

**Status:** Production-ready on ESP8266 & ESP32 | Built-in web server | UDP mesh networking | EEPROM persistence | Arduino Library

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

Qymera supports **9 sensor types** plus **2 actuator types**. Each sensor has
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

```bash
# Factory reset (clears WiFi, returns to AP mode)
curl -X POST http://<device-ip>/factory

# Toggle an actuator
curl -X POST http://<device-ip>/toggle -d "id=0&state=1"

# Set dimmer value
curl -X POST http://<device-ip>/dimmer -d "id=0&value=75"
```

Full API reference: see `AGENTS.md` for endpoint documentation.

---

## Supported Platforms

| Board | Status |
|-------|--------|
| ESP8266 (NodeMCU, Wemos D1 Mini, etc.) | Fully Tested |
| ESP32 (DevKit, WROOM, etc.) | Fully Tested |
| ESP32-S2, ESP32-S3, ESP32-C3 | Untested but likely compatible |

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

Qymera currently has **no authentication**. Recommended for home local networks
only:

1. Use only on a trusted local WiFi network
2. Change the AP SSID from `QymeraSetup` to a random name via settings
3. Block external access through your router's firewall
4. For remote access, use a VPN or custom HTTP authentication

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
#define PULSE_DURATION_MS  10  // default relay pulse duration
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

MQTT · Zigbee/Z-Wave · Matter · graphing dashboard · email/SMS notifications · mobile app. Driven by community needs.

---

License: MIT — see `LICENSE` file.

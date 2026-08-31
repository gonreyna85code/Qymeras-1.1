# Qymera Link

**Qymera Link** — Qymera ported to native **ESP-IDF** (ESP32-only) acting as an
**Matter Bridge**: it reads sensors, controls actuators, and executes automation
rules from a built-in web UI, and can optionally expose its local Link-owned
actuators (relays/dimmers) to the **Matter ecosystem**.

```
Qymera devices ──UDP──▶ Qymera Link (native ESP-IDF, ESP32) ──Matter──▶ Matter ecosystem
                                          │
                                          └─ web UI (HTTP) + automation engine (local)
```

**Branch status (qymera-IDF):** Matter-disabled build **green** (RAM 19.2% /
Flash 17.3%), host sanity suite **45/45**. Matter bridge is written and
isolated but **not yet compiled** — it requires ESP-IDF 5.2.1 + ESP-Matter 1.3.0
(see [Matter](#matter-opt-in)).

---

## Why & how this differs from Qymeras 1.1

| | Qymeras 1.1 (`main`) | Qymera Link (`qymera-IDF`) |
|--|----------------------|----------------------------|
| Framework | Arduino Library | **Native ESP-IDF** (no Arduino at runtime) |
| Targets | ESP8266 + ESP32 | **ESP32 only** |
| Transport | UDP mesh + ESP-NOW | **UDP only** (ESP-NOW removed) |
| Persistence | EEPROM / Preferences | **NVS-backed** single-factory partition |
| Matter | roadmap | **Optional, isolated** Matter bridge |

The historical Arduino build lives on `main`. This branch `#error`s if built
with the Arduino framework (`config.h`).

---

## Quick Start (ESP-IDF)

### 1. What you need

| Component | Required? |
|-----------|-----------|
| ESP32 (DevKit, WROOM, etc.) | Yes |
| USB cable (data capable) | Yes |
| ESP-IDF ≥ 5.0 toolchain (or PlatformIO `framework-espidf`) | Yes |
| Optional sensors/actuators (DHT22, relays, LED/PWM dimmers, ...) | Optional |

### 2. Build and flash

Authoritative build is **`idf.py build`** (ESP-IDF ≥ 5.0). PlatformIO
(`pio run -e esp32_idf`, framework-espidf 3.50102.240122 / IDF 5.1.2) is a
convenience environment.

```sh
idf.py set-target esp32
idf.py build
idf.py -p COM3 flash monitor   # device is on COM3
```

### 3. First-time setup

1. **Upload** the firmware to your device.
2. Connect to the WiFi network **`QymeraSetup`** (no password).
3. Open **`http://192.168.4.1`** in your browser.
4. Go to the **NETWORK** tab, enter your home WiFi SSID and password.
5. The device reboots and connects to your network.

### 4. Configure sensors and rules

- **SETTINGS** tab — calibrate sensors, set offsets, timezone, fade/pulse/persist
- **AUTOMATIONS** tab (Rules) — create automation rules
- **DEVICES** tab — control relays and dimmers in real time

---

## Public API

The application no longer defines magic global functions
(`initSatellite`/`report`/`onCommandHook`). It registers callbacks:

```cpp
#include "Qymera.h"

Qymera::setInit([] { /* GPIO/LEDC/I2C/SPI init */ });
Qymera::setReport([] {
  Qymera::temperature("TEMP", 35.2f);
  Qymera::relay("RELAY0", 5, true);
  Qymera::dimmer("DIMM0", 2, false);
});
Qymera::setCommand([](uint32_t uid, uint8_t type, int value, bool state) {
  /* (optional) observe received commands */
});

Qymera::begin();   // init NVS + networking + services
// in the app loop task:
Qymera::loop();    // single deterministic tick
```

---

## Supported entities

12 entity types (10 sensors + 2 actuators). Each entity has individual
calibration (offset, min/max, availability, persistence, pulse/fade).

| Type | API | Typical Hardware |
|------|-----|-------------------|
| Temperature | `Qymera::temperature()` | DHT22, DS18B20, NTC |
| Humidity | `Qymera::humidity()` | DHT22, soil moisture |
| Light | `Qymera::luminosity()` | Photoresistor, BH1750 |
| Pressure | `Qymera::pressure()` | BMP280, BME280 |
| Level | `Qymera::level()` | Ultrasonic, float switch |
| Air Quality | `Qymera::airQ()` | MQ135, SDS011 |
| Rain | `Qymera::rain()` | Rain drop sensor |
| Contact | `Qymera::contact()` | Reed switch, door sensor |
| Generic | `Qymera::custom()` | Any analog/digital value |
| Time | `Qymera::rtc()` / NTP | RTC module or NTP (UTC stored; timezone is a per-node offset) |
| **Relay** (actuator) | `Qymera::relay()` | Digital relay, latching |
| **Dimmer** (actuator) | `Qymera::dimmer()` | LED strip, fan, PWM |

Sensor type enum (`/calib` JSON `type` field): 1=LUMI, 2=HUMI, 3=TEMP, 4=PRESS,
5=LEVEL, 6=AIRQ, 7=RAIN, **8=DIMMER, 9=RELAY**, 10=TIME, 11=GENERIC, 12=CONTACT.

---

## Automation: up to 20 rules

Rules combine up to **5 sensors** and **5 actuators** per rule.

- **Edge** — triggers on state change (RISING / FALLING) for boolean entities
  (contact, rain)
- **Threshold** — triggers when a calibrated value crosses a threshold;
  combine multiple conditions with AND / OR
- **Time** — triggers once per day at a local time (NTP or local RTC)
- **Interval** — triggers every N ms while the date window holds

All rules support execution delay and per-rule cooldown. Note: THRESHOLD has no
hysteresis — always set `cooldown_ms`; TIME `time_s` is truncated to minutes;
TIME/INTERVAL have no catch-up after a reboot.

---

## Actuator control

- **Relays:** ON / OFF / TOGGLE / PULSE (activate for X ms then release); state
  persists across reboots (UID-matched restore before first report — no boot
  glitch).
- **Dimmers:** smooth fade transitions 0–100 % for LEDs, fans, PWM loads.

---

## Communication

- **UDP only.** ESP-NOW is removed from the runtime. `mesh::setTransport()`
  forces UDP; the `TRANSPORT_ESPNOW` enum constant stays only for source
  compatibility.
- **STA with AP fallback**: connects to the saved SSID, or brings up
  `QymeraSetup` when none is stored / connection times out.
- **Mesh**: UDP broadcast discovery/announcement (batched datagrams, protocol
  v4/v5, up to 29 packets/datagram). Remote entities are
  visible/controllable/calibratable by POSTing to the owning node's IP; remote
  config is verified over HTTP and never falls back silently to the local node.
- **Time**: SNTP (poll mode) + manual RTC set via mesh time sync.

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
- Commands address entity **`id` (uid)**; rules address **slot indexes**.
- Remote entities must be addressed on their **owner's IP** (the `ip` field in
  `/calib`).
- Rate limit: 6 requests / 2 s burst on state-changing endpoints (7th → `429`).
- Endpoints: `/`, `/save`, `/calib`, `/calib/set`, `/genset/save`, `/rules`,
  `/rules/set`, `/rules/delete`, `/factory`, `/toggle`, `/dimmer`, `/logs`.

---

## Persistence

A 4 KiB EEPROM-layout image is stored in NVS (`qymera`/`eedata` blob) and edited
byte-wise in RAM. Partition table is single-`factory` (no OTA slots) — see
`partitions_qymera.csv`.

- WiFi credentials · general settings (ports/interval) · sensor calibration
  (UID slot: magic+version+uid+pers_state+min/max+correction+avail+persist+
  pulse+ms+fade) · automation rules · OTA device identity flag.

### Factory reset

`POST /factory` clears Qymera credentials, calibration, and rules, then reboots
into `QymeraSetup` AP mode. **It does NOT touch the Matter fabric.** Matter
commissioning is stored in the chip/Matter NVS namespace, which is independent;
removing a Matter fabric uses the standard Matter operation (e.g. open the
commissioning window after the last fabric is removed), not the Qymera factory
reset.

---

## Matter (opt-in)

The **default build is Matter-free** and pulls only the ESP-IDF core. The Matter
bridge is compiled and run only behind `#if CONFIG_QYMERA_MATTER_ENABLE`.

### Pairing (pinned versions)

| Component | Version |
|-----------|---------|
| `espressif/esp_matter` | `~1.3.0` (first registry-published line; release/v1.3) |
| ESP-IDF | **5.2.1** (both ESP-Matter 1.2 and 1.3 recommend IDF v5.2.1) |
| target | `esp32` |

> **Important:** there is **no `1.2.x` in the Espressif Component Registry**
> (published versions go `0.0.1, 0.0.2, 1.3.0, 1.3.1, 1.4.x, 1.5.x, 1.6.0`).
> Pinning `^1.2` cannot resolve — use `~1.3.0`.

### Enable

1. Uncomment `espressif/esp_matter: "~1.3.0"` in `main/idf_component.yml`
   (the component manager fetches the SDK).
2. Set `CONFIG_QYMERA_MATTER_ENABLE=y` (via `idf.py menuconfig` or
   `sdkconfig.defaults`).

### Device model

| Qymera entity | Matter device type |
|---------------|--------------------|
| Relay | OnOffLight (OnOff cluster) |
| Dimmer | DimmableLight (OnOff + LevelControl clusters) |

Only **local Link-owned** actuators are bridged. Remote Qymera entities in the
mesh are never exposed as Matter endpoints.

### Commissioning

After the Matter-enabled build boots with an IP connection, commission the
device with any Matter controller (Google Home, Apple Home, Alexa, CHIP-Tool,
Matter controller in the ESP app). The bridge starts automatically once the
stack is online; device label is **"Qymera"**.

### State synchronization

- **Matter → Qymera:** on/off and level writes are routed through the same
  validated actuation primitives the web API uses (`sensors::setRelay` /
  `sensors::handleDimmer`) — no direct GPIO.
- **Qymera → Matter:** local state changes are pushed to the Matter attribute DB
  via `attribute::update`, echo-guarded (only pushes on change; the write
  callback only acts on external requests) so there is **no feedback loop**.

### Maturity / verification status

The bridge is **written against the official ESP-Matter `release/v1.3` API**
(`managed_component_light` example + `esp_matter_core.h`), but **has not been
compiled in this workspace** — Matter needs IDF 5.2.1, and this environment only
has IDF 5.1.2 via PlatformIO. Treat Matter as **source-ready scaffold pending a
real IDF 5.2.1 build and hardware commissioning**, not as production-verified.

---

## Architecture

```
main/                    example application (app_main + loop task)
  main.cpp               registers callbacks via Qymera API + boot sequence
  matter_bridge.*        Matter bridge - compiled only if CONFIG_QYMERA_MATTER_ENABLE=y
components/qymera/       reusable ESP-IDF component (the library)
  src/Qymera.h/.cpp      public API (Qymera::begin/loop/setInit/setReport/setCommand)
  src/core.*             deterministic application loop + WiFi orchestration
  src/sensors.*          sensor/actuator model, calibration, command routing
  src/automations.*      automation rules engine
  src/mesh.*             UDP mesh protocol v5
  src/web.* / html.*     embedded web UI + REST endpoints
  src/log.*              runtime log ring buffer
  src/storage.*          NVS-backed persistence (4 KiB EEPROM-image blob)
  src/hal/               platform HAL (qhal/qhttp/qstr)
```

### Deterministic core ↔ HAL boundary

Only `src/hal/*` touches ESP-IDF drivers directly. The deterministic core
(`sensors`/`automations`/`rules`) is frozen and portable, reaching hardware only
through the thin `qhal_*` / `qstr` interface:

| HAL symbol | Backing ESP-IDF API |
|------------|---------------------|
| `qhal_wifi_sta_connect` / `qhal_wifi_start_ap` | `esp_wifi` |
| `qhal_wifi_ap_active` | `esp_wifi` mode query |
| `qhal_local_ip` | `esp_netif` |
| `qhal_pin_output/write` | `driver/gpio` |
| `qhal_pwm_setup/write` | `driver/ledc` (8-bit) |
| `qhal_sntp_init` | `esp_sntp` (poll mode) |
| `qhal_lock/unlock` | FreeRTOS recursive mutex |
| `qhal_delay` / `millis` | `esp_timer` |
| storage (NVS) | `nvs_flash` |
| `qhttp` | `esp_http_server` |

### Runtime / threading model

FreeRTOS:
- **`qymera_loop` task** (priority 5, 8 KiB stack) runs the single deterministic
  `core::loop()` tick at ~1 ms period.
- **httpd worker tasks** serve HTTP concurrently. A recursive mutex
  (`qhal_lock`) serializes them against the loop task so deterministic state
  (calibrations/reports/rules) is only mutated safely.

---

## Security

- HTTP Basic Auth gate present but **off by default** (placeholder creds
  `admin:qymera123` in firmware, not in client JS). Trusted local networks only.
- OTA device identity check: a chip-unique token is stored/verified on
  boot/toggle — a provisioning check, not a firmware authenticity hash.
- Rate limiting: burst-tolerant 6 req / 2 s on state-changing endpoints.
- Strict input validation on every write path (rejects empty/trailing-junk/
  overflow/NaN/Inf; enforces type ranges).
- Known limitations: HTTP only (no HTTPS); no CSRF tokens; no full firmware
  signing. Use on a trusted local WiFi network; block external access via
  router firewall.

---

## Tests

- **`tests/host_sanity.py`**: host-state checks (timezone/float parsing, FIFO).
  **45/45 pass.**
- The authoritative gate is a successful firmware build plus the host sanity
  suite.
- Matter-enabled bridge / commissioning require hardware (`COM3` flash + real
  commissioning) and are tracked as hardware-verified steps, not claimed from
  the build alone.

---

## Troubleshooting

**Device appears but won't join WiFi?** Ensure the USB cable supports data
(not power-only); wait 10 s after power-on before scanning; verify the router
allows unknown MACs on 2.4 GHz.

**Sensors not reporting?** Register the sensor in SETTINGS, add calibration,
verify pin assignments. Remote entities only appear while their owner announces
(< ~30 s, `MESH_TIMEOUT`).

---

## Roadmap

Matter commissioning validation on hardware · MQTT · Zigbee/Z-Wave · graphing
dashboard · email/SMS notifications · mobile app. An optional external AI
assistant subsystem is authorized and under development on
`feature/ai-experiments` (kept out of the production tree per `AGENTS.md`).

---

License: MIT — see `LICENSE`.

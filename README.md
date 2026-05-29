# 🛰️ QYMERAS - Smart Automation Firmware

Qymeras is a complete automation and sensor acquisition system for IoT devices. It transforms an ESP8266 or ESP32 microcontroller into a powerful, standalone node that captures sensor data, executes complex automation rules, and communicates with central systems.

**Status:** ✅ Fully functional on ESP8266 & ESP32 | 🧪 Auto-detects platform | 🚀 Web-based configuration

---

## 🎯 What Does Qymeras Do?

Transform a simple ESP8266/ESP32 into a complete IoT automation hub:

- **📊 Reads sensors**: Temperature, humidity, light, pressure, water level, air quality, rain detection
- **🎛️ Controls actuators**: Relays, dimmers with smooth transitions
- **⚙️ Automates tasks**: Create up to 20 complex rules (edge detection, thresholds, scheduled, periodic)
- **🌐 Web interface**: Fully configure without serial cable or tools
- **📡 Broadcasts data**: Send sensor readings to central server via UDP + HTTP
- **🔌 Offline-first**: Works without Internet, persists configuration in EEPROM
- **🔄 OTA updates**: Update firmware wirelessly
- **⏰ Smart scheduling**: NTP time sync or local RTC for time-based automation

---

## 🚀 Quick Start

### Hardware Requirements

**Minimum:**
- ESP8266 (01, NodeMCU, D1 Mini) or ESP32 (D1 R32, DevKit)
- USB cable for first upload
- Power supply (5V micro-USB or 3.3V GPIO)

**Optional:**
- Sensors (DHT22, BMP280, photoresistors, soil moisture, etc.)
- Relays/SSRs for high-power control
- RTC module (DS3231) for offline time
- USB-TTL adapter for debugging

### First Time Setup

1. **Install PlatformIO** (VS Code extension)
   ```
   https://platformio.org/install/ide?install=vscode
   ```

2. **Clone & Open Project**
   ```bash
   git clone <repo_url>
   cd AntiMatter_Satellite_base_stack_PCF_FINAL_DIRTY_BETA
   ```

3. **Edit `platformio.ini` and select your board:**
   ```ini
   [env:esp8266_d1_mini]      ; For NodeMCU/D1 Mini
   ; [env:esp32_devkit]       ; For ESP32 D1 R32
   ```

4. **Upload to device:**
   ```bash
   pio upload -e esp8266_d1_mini
   ```

5. **Connect to WiFi:**
   - Look for WiFi network: `PeriferalSetup`
   - Open browser: `http://192.168.4.1`
   - Go to **NETWORK** tab
   - Enter your WiFi SSID & password
   - Device reboots and connects

6. **Configure sensors & automation:**
   - Now at `http://<device_ip>` (check your router)
   - Go to **SETTINGS** tab to calibrate sensors
   - Go to **AUTOMATIONS** tab to create rules

---

## 📋 Features Overview

### 1. Sensor Support (10 Types)

| Sensor | Range | Unit | Use Case |
|--------|-------|------|----------|
| **Temperature** | -50 to +150 | °C | DHT22, DS18B20, thermistors |
| **Humidity** | 0-100 | % | Soil moisture, air humidity |
| **Light** | 0-65535 | lux | Photoresistors, lux sensors |
| **Pressure** | 300-1100 | hPa | BMP280, barometric |
| **Level** | 0-100 | % | Tank level, pool level |
| **Air Quality** | GOOD/WARN/BAD | enum | MQ135, SDS011 |
| **Rain** | ON/OFF | bool | Rain detector |
| **Relay** | ON/OFF | bool | Switches, contactors |
| **Dimmer** | 0-100 | % | PWM, LED, fan speed |

Each sensor is **individually calibrated** with offset correction and min/max range mapping.

### 2. Automation Rules (4 Types)

Create up to **20 rules** with different trigger modes:

#### **Type 1: Edge Detection (State Change)**
Triggers when a sensor changes state
```
Example: When door opens (becomes ON) → trigger alarm
```

#### **Type 2: Threshold (Value Crossing)**
Triggers when value crosses a limit
```
Example: When temperature > 30°C → turn on fan
Multiple sensors: When temp > 25°C AND humidity > 80% → open window
```

#### **Type 3: Scheduled Time**
Triggers at specific time of day
```
Example: Every day at 18:00 → turn on lights
Time range: November 1 to March 31 at 22:00 → turn off heating
```

#### **Type 4: Periodic Interval**
Triggers every X milliseconds
```
Example: Every 5 minutes → send sensor data to server
```

**Advanced options for all rules:**
- Delay before execution
- Cooldown between triggers (prevent spam)
- Minimum ON/OFF duration

**Per rule: Up to 5 sensors + 5 actuators**

### 3. Actuator Control

#### **Relays (Binary Control)**
- ON/OFF commands
- TOGGLE (invert state)
- PULSE mode: Activate for X milliseconds then auto-off
  - Use for: Pumps, solenoid valves, buzzers
- Optional PERSISTENCE: Remember state after reboot

#### **Dimmers (Analog PWM)**
- Level 0-100%
- Smooth FADE transitions: Change level gradually over time
  - Use for: LED brightness, fan speed, motor speed
- Each dimmer independently controlled

### 4. Web Configuration Interface

No serial cables or special tools needed. Just a browser.

**4 Configuration Tabs:**

1. **DEVICES** - Real-time control of all actuators
2. **AUTOMATIONS** - Visual wizard to create rules
3. **SETTINGS** - Calibrate sensors, configure ports
4. **NETWORK** - Connect to WiFi, manage credentials

---

## 📡 Communication Protocols

### HTTP (REST API)

Web interface for configuration and data:

```
GET  /                 → Main web interface
GET  /calib            → All sensors (JSON)
POST /calib/set        → Calibrate specific sensor
POST /toggle           → Toggle relay
POST /dimmer           → Set dimmer level
GET  /rules            → All automation rules (JSON)
POST /rules/set        → Create/edit rule
POST /save             → Save WiFi credentials
POST /genset/save      → Save general settings
POST /factory          → Factory reset (erase all)
```

### UDP Broadcasting

Device broadcasts sensor data every 40 seconds to local network:

```
Format: Binary compact format (5 bytes per sensor)
Port: 13345 (configurable)
Use: Central server can collect data from multiple devices
```

### UDP Commands

Receive control commands from central server:

```
Port: 13346 (configurable)
Format: Same binary format as broadcast
Use: Remote control of relays and dimmers
```

### WiFi Modes

- **STA (Station):** Connects to your WiFi router
- **AP (Access Point):** Creates local emergency WiFi if main WiFi unavailable
  - SSID: `PeriferalSetup` (no password)
  - IP: `192.168.4.1`
  - Use for initial setup or troubleshooting

---

## 📊 Real-World Use Cases

### 🌾 Smart Greenhouse Automation
```
Sensors: Temperature, humidity, soil moisture, light, water level
Actuators: Water pump (relay), ventilator (dimmer), heater (relay)

Rules:
  → If temp > 30°C AND humidity > 80% → Open ventilator
  → If soil moisture < 30% → Activate pump for 5 minutes
  → Every day 06:00 → Lights ON
  → Every day 18:00 → Lights OFF
```

### 🏠 Home Automation
```
Sensors: Motion, door contacts, indoor/outdoor temp, light level
Actuators: Smart lights (dimmers), heating (relay), locks (relay)

Rules:
  → Motion detected between 18-22 → Lights ON (70% brightness, fade 1s)
  → No motion after 23:00 → Lights OFF (fade 3s)
  → Temperature < 18°C → Heating ON
  → Every 22:00 → Lock all doors
```

### 💧 Smart Irrigation System
```
Sensors: Soil moisture (4 zones), rain detector, temperature
Actuators: 4 solenoid valves (relays), main pump (relay)

Rules:
  → Zone 1 moisture < 40% AND no rain → Valve 1 ON + Pump ON
  → Rain detected → All valves OFF
  → Temperature < 5°C → Pump OFF (freeze protection)
  → Every 06:00 and 18:00 → Run all zones for 5 minutes
```

### 📡 IoT Data Collection
```
Multiple Qymeras devices across location
Each device broadcasts sensor data every 40 seconds
Central server collects all data into database
Dashboard shows real-time status of all sensors
Central server can send commands back to any device
```

---

## 🛠️ Configuration

### Via Web Interface (Recommended)

1. Connect to WiFi: `PeriferalSetup`
2. Open: `http://192.168.4.1`
3. Go to NETWORK tab → Enter WiFi credentials
4. Device reboots and connects to your WiFi
5. Access now at: `http://<device_ip>`

### Via Code (Advanced)

Edit `config.h`:

```cpp
#define MAX_SENSORS 64           // Max sensors per device
#define MAX_RULES 20             // Max automation rules
#define AP_SSID "PeriferalSetup" // Emergency WiFi name
#define BROADCAST_PORT 13345     // UDP broadcast port
#define COMMAND_PORT 13346       // UDP command port
#define BROADCAST_INTERVAL 40000 // Data send every 40 seconds
```

---

## 🔌 Adding Your Own Sensors

### Example: Add Custom Sensor Reading

Edit your `.ino` file:

```cpp
void report() {
  // Read custom sensor
  int sensorValue = analogRead(A0);
  float temperature = (sensorValue / 1024.0) * 50.0; // Convert to temp
  
  // Report to Qymeras
  sensors::temperature("RoomTemp", temperature);
  sensors::humidity("SoilMoisture", moistureValue);
  sensors::luminosity("OutsideLight", luxValue);
}
```

The sensor automatically:
- Gets registered in the system
- Appears in web UI
- Can be used in automation rules
- Gets persisted in EEPROM

### Supported Sensor Functions

```cpp
sensors::temperature(name, value);      // °C
sensors::humidity(name, value);         // 0-100%
sensors::luminosity(name, value);       // 0-65535 lux
sensors::pressure(name, value);         // hPa
sensors::level(name, value);            // 0-100%
sensors::airQ(name, value);             // 0=GOOD, 1=WARN, 2=BAD
sensors::rain(name, value);             // true/false
```

---

## 🎮 Remote Control

### Control via HTTP (Quick Test)

```bash
# Toggle a relay
curl -X POST "http://<device_ip>/toggle?key=MyRelay"

# Set dimmer to 75%
curl -X POST "http://<device_ip>/dimmer" \
  -d "key=MyLight&value=75"

# Get all sensors
curl "http://<device_ip>/calib"
```

### Control via UDP (Low Latency)

Send binary commands to port 13346:
```
Format: [0xA5] [Version] [Size] [DeviceUID] [Commands...]
```

### Control from Central Server

Example Python script:

```python
import requests
import json

device_ip = "192.168.1.100"

# Get all sensors
sensors = requests.get(f"http://{device_ip}/calib").json()

# Control relay
requests.post(f"http://{device_ip}/toggle", 
              data={"key": "Pump"})

# Create automation rule
rule = {
    "id": -1,
    "sensors": [0, 1],
    "type": 1,  # THRESHOLD
    "cmp": [0, 0],  # > and >
    "threshold": [25, 80],
    "actuators": [5],
    "actions": [0],
    "levels": [0]
}
requests.post(f"http://{device_ip}/rules/set", data=rule)
```

---

## 🔄 Platform Support

### Currently Tested ✅
- **ESP8266**: 01, NodeMCU, D1 Mini
- **ESP32**: D1 R32, DevKit, Generic

### Likely Compatible 🟡
- ESP32-S2, ESP32-S3, ESP32-C3

### Auto-Detection
The code automatically detects your platform in `config.h`:
```cpp
#if defined(ESP8266)
  // ESP8266 specific code
#elif defined(ESP32)
  // ESP32 specific code
#endif
```

To add a new platform, just add an `#elif` block with the platform defines.

---

## 💾 Data Persistence

All configuration is stored in EEPROM (4 KB):

| Data | Size | Survives Reboot? |
|------|------|------------------|
| Sensor values | 512 bytes | Last values only |
| WiFi credentials | 100 bytes | ✅ Yes |
| Automation rules | 1600 bytes | ✅ Yes |
| Calibration data | 512 bytes | ✅ Yes |
| Relay states (if persistent mode) | 10 bytes | ✅ Yes |

**Factory Reset:** Via web UI (Settings tab) or HTTP POST `/factory`
- Erases all configuration
- Restores defaults
- Device reboots to AP mode

---

## ⏰ Time Management

### Automatic NTP Sync
If WiFi is connected:
- Automatically syncs time from Internet
- Updates in background every loop
- Used for scheduled automation rules

### Local RTC Support
For offline time-based rules:
- Connect DS3231 or similar RTC module
- Time persists without WiFi
- Automated rules work offline

### Time Sources Priority
1. NTP (if WiFi + Internet available)
2. RTC (if hardware connected)
3. Fallback (local system time, less accurate)

---

## 🔐 Security Notes

⚠️ **Current Status:** NO authentication, HTTP only (local network only)

### Recommendations for Production

1. **Local Network Only:** Keep device on trusted WiFi, not exposed to Internet
2. **Change AP SSID:** Modify `AP_SSID` in code to something unique
3. **Add Firewall:** Use router to block external access to ports 80, 13345-13346
4. **VPN for Remote:** Use VPN if remote access needed
5. **Custom Authentication:** Fork code and add HTTP Basic Auth if needed

### Planned Security Enhancements
- [ ] HTTP Basic Authentication
- [ ] HTTPS/TLS support
- [ ] EEPROM encryption
- [ ] CSRF protection
- [ ] Rate limiting

---

## 📈 Performance & Limits

### Hardware Constraints

| Metric | Limit | Notes |
|--------|-------|-------|
| Max Sensors | 64 | Configurable, limited by RAM |
| Max Rules | 20 | Configurable, limited by EEPROM |
| Sensors per Rule | 5 | Hardcoded |
| Actuators per Rule | 5 | Hardcoded |
| EEPROM | 4 KB | Typical ESP8266/32 |
| WiFi Latency | 100-500ms | Local network |
| UDP Latency | 10-50ms | Local network |
| HTTP Response | 100-300ms | Depends on request size |

### Real-World Performance

- **Web UI response time:** 100-300ms
- **Relay activation time:** <100ms
- **Sensor reading frequency:** Configurable, typically 1-5 seconds
- **UDP broadcast:** Every 40 seconds (default)
- **Rule evaluation:** Every 10-100ms

---

## 🐛 Troubleshooting

### Device not appearing in WiFi list

1. Check power supply (5V micro-USB or 3.3V GPIO)
2. Check USB cable (some cables power-only, no data)
3. Wait 10 seconds after power-on
4. Try in airplane mode on phone
5. Restart device (replug power)

### Can't access web interface

1. Confirm you're connected to `PeriferalSetup` WiFi
2. Try IP: `192.168.4.1` directly
3. Check firewall/antivirus blocking
4. Try from different device/browser
5. Check device logs via serial (USB terminal)

### WiFi connection fails

1. Check SSID and password are correct (case-sensitive)
2. Make sure WiFi is 2.4 GHz (ESP8266/32 don't support 5 GHz)
3. Move closer to router
4. Restart both device and router
5. Check router allows unknown MAC addresses

### Sensors not showing values

1. Check sensor is properly connected to GPIO
2. Verify pin configuration in code
3. Try reading raw sensor value via serial monitor
4. Calibrate sensor in Settings tab
5. Check EEPROM isn't full

### Rules not triggering

1. Check rule is saved (appears in AUTOMATIONS tab)
2. Verify sensor values change (watch in DEVICES tab)
3. Check COOLDOWN isn't preventing execution
4. Verify actuators are wired correctly
5. Check EEPROM for corruption (Factory Reset)

---

## 📚 API Reference

### Core Functions

```cpp
// WiFi & Time
core::is_connected()           // Returns WiFi status
core::get_uid()                // Device unique ID

// Time
sensors::getTime()             // Current RTCTime
sensors::getUnixTime()         // Seconds since epoch
sensors::getMinutesOfDay()     // 0-1440
sensors::timeValid()           // Is time synchronized?

// Sensors
sensors::temperature(key, value)
sensors::humidity(key, value)
sensors::luminosity(key, value)
sensors::pressure(key, value)
sensors::level(key, value)
sensors::airQ(key, value)
sensors::rain(key, value)

// Actuators
sensors::relay(key, pin)
sensors::dimmer(key, pin)
sensors::setRelay(key, state)
sensors::handleDimmer(key, value)
sensors::handleToggle(key)
sensors::startFade(key, pin, from, to, duration_ms)

// Automations
automations::tick(millis)
automations::rules[i]          // Access rule directly
```

---

## 🚀 Future Roadmap

- [ ] MQTT support
- [ ] Zigbee/Z-Wave
- [ ] Matter protocol integration
- [ ] Dashboard with graphs
- [ ] Event logging to SD card
- [ ] Email/SMS notifications
- [ ] Voice control integration
- [ ] Android/iOS mobile app
- [ ] Redundancy & failover
- [ ] Scene management

---

## 📝 Contributing

Found a bug? Have a feature idea? Feel free to open an issue or submit a pull request.

### Development Setup

```bash
# Install PlatformIO CLI
pip install platformio

# Install dependencies
pio pkg install

# Build for your platform
pio run -e esp8266_d1_mini

# Upload & monitor
pio run -t upload -e esp8266_d1_mini --monitor
```

---

## 📄 License

MIT License - See LICENSE file

---

## 🙏 Thanks & Credits

- **PlatformIO** - Build system and IDE
- **Arduino Core** - For ESP8266/ESP32 support
- **Contributors** - Community feedback and testing

---

## 📞 Support

For issues, questions, or suggestions:

1. Check [Troubleshooting](#-troubleshooting) section
2. Review [API Reference](#-api-reference)
3. Open an issue on GitHub
4. Check project wiki for detailed guides

---

**Made with ❤️ for IoT automation**


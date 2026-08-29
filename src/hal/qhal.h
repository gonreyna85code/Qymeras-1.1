#pragma once
/*
  qhal.h - native ESP-IDF platform abstraction for Qymera (qymera-IDF branch)

  Wraps ESP-IDF drivers (gpio, ledc, esp_wifi, esp_netif, SNTP, esp_timer)
  behind the small API the deterministic core expects. This is Qymera's own
  HAL - it is NOT the Arduino framework and pulls no external libraries.
*/

#include <stdint.h>
#include <stddef.h>
#include <time.h>

#include "esp_system.h"
#include "esp_timer.h"
#include "esp_mac.h"
#include "esp_heap_caps.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"

// ================= TIME =================
// ms since boot (esp_timer), unsigned long for source compatibility.
unsigned long millis();
void qhal_delay(unsigned long ms);

// System clock: SNTP (poll mode) and manual RTC set (mesh time sync).
void qhal_sntp_init();
void qhal_settimeofday(time_t t);

// ================= CHIP / SYSTEM =================
uint32_t qhal_chip_id();
void qhal_restart();
size_t qhal_free_heap();

// ================= CONCURRENCY GUARD =================
// The original Qymera ran single-threaded (Arduino loop). ESP-IDF runs the
// HTTP handler on httpd worker tasks, so each handler is serialized against the
// main qymera loop task through this recursive mutex. Deterministic core state
// (calibrations/reports/rules) must only be mutated while holding it.
void qhal_lock_init();
void qhal_lock();
void qhal_unlock();
class QhalLockGuard {
 public:
  QhalLockGuard() { qhal_lock(); }
  ~QhalLockGuard() { qhal_unlock(); }
};

// ================= GPIO =================
void qhal_pin_output(uint8_t pin);
void qhal_pin_input(uint8_t pin);
void qhal_digital_write(uint8_t pin, bool level);
bool qhal_digital_read(uint8_t pin);

// ================= PWM (LEDC, 8-bit 0..255) =================
void qhal_pwm_setup(uint8_t pin);
void qhal_pwm_write(uint8_t pin, uint8_t value);
uint8_t qhal_pwm_read(uint8_t pin);

// ================= NETWORK =================
// WiFi events are surfaced as a pending-events bitmask polled by core::loop().
#define QHAL_WIFI_EVT_GOT_IP       (1u << 0)
#define QHAL_WIFI_EVT_DISCONNECTED (1u << 1)

void qhal_net_init();                       // esp_netif + esp_wifi + event loop
void qhal_wifi_sta_connect(const char *ssid, const char *password);
void qhal_wifi_start_ap(const char *ssid);
uint32_t qhal_wifi_poll_events();           // returns + clears pending events
bool qhal_wifi_ap_active();
bool qhal_wifi_sta_connected();
int8_t qhal_wifi_rssi();
bool qhal_wifi_init_done();

class QIP {
 public:
  uint8_t b[4] = {0, 0, 0, 0};
  uint8_t operator[](int i) const { return b[i]; }
  bool isValid() const {
    return b[0] != 0 || b[1] != 0 || b[2] != 0 || b[3] != 0;
  }
};

QIP qhal_local_ip();
void qhal_ip_from_str(QIP &out, const char *dotted);

// ================= BOOTSTRAP =================
void qhal_system_init();
/* qhal.cpp - native ESP-IDF platform abstraction for Qymera (qymera-IDF branch) */

#include "qhal.h"

#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_sntp.h"
#include "esp_efuse.h"
#include "lwip/ip4_addr.h"

// ================= TIME =================
unsigned long millis() {
  return (unsigned long)(esp_timer_get_time() / 1000);
}

void qhal_delay(unsigned long ms) {
  vTaskDelay(pdMS_TO_TICKS(ms));
}

void qhal_sntp_init() {
  esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
  esp_sntp_setservername(0, "pool.ntp.org");
  esp_sntp_setservername(1, "time.nist.gov");
  esp_sntp_init();
}

void qhal_settimeofday(time_t t) {
  struct timeval tv = {};
  tv.tv_sec = t;
  settimeofday(&tv, nullptr);
}

// ================= CHIP / SYSTEM =================
uint32_t qhal_chip_id() {
  uint8_t mac[6] = {0};
  if (esp_efuse_mac_get_default(mac) != ESP_OK) return 0;
  // Mirrors Arduino's ESP.getEfuseMac() ordering (mac[0] in the top byte),
  // then the same (uint32_t) truncation Qymera has always used.
  uint64_t v = 0;
  for (int i = 0; i < 6; i++) v |= (uint64_t)mac[i] << (40 - 8 * i);
  return (uint32_t)(v & 0xFFFFFFFFull);
}

void qhal_restart() {
  esp_restart();
}

size_t qhal_free_heap() {
  return (size_t)esp_get_free_heap_size();
}

// ================= CONCURRENCY GUARD =================
static SemaphoreHandle_t s_lock = nullptr;

void qhal_lock_init() {
  if (!s_lock) s_lock = xSemaphoreCreateRecursiveMutex();
}

void qhal_lock() {
  if (!s_lock) qhal_lock_init();
  xSemaphoreTakeRecursive(s_lock, portMAX_DELAY);
}

void qhal_unlock() {
  if (s_lock) xSemaphoreGiveRecursive(s_lock);
}

// ================= GPIO =================
void qhal_pin_output(uint8_t pin) {
  gpio_set_direction((gpio_num_t)pin, GPIO_MODE_OUTPUT);
}

void qhal_pin_input(uint8_t pin) {
  gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT);
}

void qhal_digital_write(uint8_t pin, bool level) {
  gpio_set_level((gpio_num_t)pin, level ? 1 : 0);
}

bool qhal_digital_read(uint8_t pin) {
  return gpio_get_level((gpio_num_t)pin) != 0;
}

// ================= PWM (LEDC, 8-bit 0..255) =================
#define QHAL_PWM_CHANNELS 8
static const int s_pwm_timer = LEDC_TIMER_0;
static const ledc_mode_t s_pwm_mode = LEDC_LOW_SPEED_MODE;
static uint8_t s_pwm_level[QHAL_PWM_CHANNELS] = {0};

void qhal_pwm_setup(uint8_t pin) {
  static bool timer_configured = false;
  if (!timer_configured) {
    ledc_timer_config_t timer = {};
    timer.speed_mode = s_pwm_mode;
    timer.timer_num = (ledc_timer_t)s_pwm_timer;
    timer.duty_resolution = LEDC_TIMER_8_BIT;   // 0..255
    timer.freq_hz = 1000;                        // 1 kHz, matches Arduino core
    timer.clk_cfg = LEDC_AUTO_CLK;
    ledc_timer_config(&timer);
    timer_configured = true;
  }

  int ch = pin % QHAL_PWM_CHANNELS;
  gpio_set_direction((gpio_num_t)pin, GPIO_MODE_OUTPUT);
  ledc_channel_config_t chan = {};
  chan.gpio_num = (int)pin;
  chan.speed_mode = s_pwm_mode;
  chan.channel = (ledc_channel_t)ch;
  chan.intr_type = LEDC_INTR_DISABLE;
  chan.timer_sel = (ledc_timer_t)s_pwm_timer;
  chan.duty = s_pwm_level[ch];
  chan.hpoint = 0;
  ledc_channel_config(&chan);
  ledc_update_duty(s_pwm_mode, (ledc_channel_t)ch);
}

void qhal_pwm_write(uint8_t pin, uint8_t value) {
  int ch = pin % QHAL_PWM_CHANNELS;
  s_pwm_level[ch] = value;
  ledc_set_duty(s_pwm_mode, (ledc_channel_t)ch, value);
  ledc_update_duty(s_pwm_mode, (ledc_channel_t)ch);
}

uint8_t qhal_pwm_read(uint8_t pin) {
  int ch = pin % QHAL_PWM_CHANNELS;
  return s_pwm_level[ch];
}

// ================= NETWORK =================
static esp_netif_t *s_sta_netif = nullptr;
static esp_netif_t *s_ap_netif = nullptr;
static volatile uint32_t s_evt = 0;
static volatile bool s_ip_ok = false;
static volatile bool s_wifi_started = false;
static volatile bool s_connect_pending = false;
static bool s_net_inited = false;

static void qy_wifi_event_handler(void * /*arg*/, esp_event_base_t base,
                                  int32_t id, void * /*data*/) {
  if (base == WIFI_EVENT) {
    if (id == WIFI_EVENT_STA_START) {
      if (s_connect_pending) {
        s_connect_pending = false;
        esp_wifi_connect();
      }
    } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
      s_evt |= QHAL_WIFI_EVT_DISCONNECTED;
      s_ip_ok = false;
    }
  } else if (base == IP_EVENT) {
    if (id == IP_EVENT_STA_GOT_IP) {
      s_evt |= QHAL_WIFI_EVT_GOT_IP;
      s_ip_ok = true;
    }
  }
}

void qhal_net_init() {
  if (s_net_inited) return;
  s_net_inited = true;

  esp_netif_init();
  esp_event_loop_create_default();

  s_sta_netif = esp_netif_create_default_wifi_sta();
  s_ap_netif = esp_netif_create_default_wifi_ap();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
  esp_wifi_set_storage(WIFI_STORAGE_FLASH);

  esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                      qy_wifi_event_handler, nullptr, nullptr);
  esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                      qy_wifi_event_handler, nullptr, nullptr);
}

static void qy_wifi_ensure_started() {
  if (!s_wifi_started) {
    esp_wifi_start();
    s_wifi_started = true;
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
  }
}

void qhal_wifi_sta_connect(const char *ssid, const char *password) {
  wifi_config_t cfg = {};
  strncpy((char *)cfg.sta.ssid, ssid, sizeof(cfg.sta.ssid) - 1);
  if (password) strncpy((char *)cfg.sta.password, password, sizeof(cfg.sta.password) - 1);
  cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_set_config(WIFI_IF_STA, &cfg);
  s_evt &= ~(QHAL_WIFI_EVT_GOT_IP | QHAL_WIFI_EVT_DISCONNECTED);
  s_ip_ok = false;

  if (s_wifi_started) {
    esp_wifi_connect();
  } else {
    s_connect_pending = true;
    qy_wifi_ensure_started();
  }
}

void qhal_wifi_start_ap(const char *ssid) {
  wifi_config_t cfg = {};
  strncpy((char *)cfg.ap.ssid, ssid, sizeof(cfg.ap.ssid) - 1);
  cfg.ap.ssid_len = (uint8_t)strlen(ssid);
  cfg.ap.max_connection = 4;
  cfg.ap.authmode = WIFI_AUTH_OPEN;

  esp_wifi_set_mode(WIFI_MODE_AP);
  esp_wifi_set_config(WIFI_IF_AP, &cfg);
  s_evt &= ~(QHAL_WIFI_EVT_GOT_IP | QHAL_WIFI_EVT_DISCONNECTED);
  qy_wifi_ensure_started();
}

uint32_t qhal_wifi_poll_events() {
  uint32_t e = s_evt;
  s_evt = 0;
  return e;
}

bool qhal_wifi_ap_active() {
  wifi_mode_t m = WIFI_MODE_NULL;
  if (esp_wifi_get_mode(&m) != ESP_OK) return false;
  return m == WIFI_MODE_AP || m == WIFI_MODE_APSTA;
}

bool qhal_wifi_sta_connected() {
  return s_ip_ok;
}

int8_t qhal_wifi_rssi() {
  int8_t rssi = 0;
  wifi_ap_record_t ap = {};
  if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) rssi = ap.rssi;
  return rssi;
}

QIP qhal_local_ip() {
  QIP ip;
  esp_netif_t *n = (s_sta_netif && qhal_wifi_sta_connected()) ? s_sta_netif : s_ap_netif;
  if (!n) return ip;
  esp_netif_ip_info_t info;
  if (esp_netif_get_ip_info(n, &info) == ESP_OK) {
    ip.b[0] = ip4_addr1_16(&info.ip);
    ip.b[1] = ip4_addr2_16(&info.ip);
    ip.b[2] = ip4_addr3_16(&info.ip);
    ip.b[3] = ip4_addr4_16(&info.ip);
  }
  return ip;
}

void qhal_ip_from_str(QIP &out, const char *dotted) {
  unsigned a = 0, b = 0, c = 0, d = 0;
  if (sscanf(dotted, "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
    out.b[0] = (uint8_t)a;
    out.b[1] = (uint8_t)b;
    out.b[2] = (uint8_t)c;
    out.b[3] = (uint8_t)d;
  }
}

// ================= BOOTSTRAP =================
void qhal_system_init() {
  qhal_lock_init();
  qhal_net_init();
}
/**
 * Qymera Dashboard - HAL Implementation (ESP32 / Arduino-ESP32 framework)
 * Uses Arduino-ESP32 framework APIs which wrap ESP-IDF
 */
#include "qymera_hal.h"
#include "qymera_types.h"

#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "Arduino.h"
#include "WiFi.h"
#include "WebServer.h"
#include "ESPmDNS.h"
#include "Update.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_chip_info.h"
#include "esp_idf_version.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_efuse.h"

/* =========================
 * Internal State
 * ========================= */

static bool s_hal_initialized = false;
static bool s_wifi_initialized = false;
static bool s_netif_initialized = false;
static SemaphoreHandle_t s_wifi_mutex = NULL;
static bool s_ota_enabled = false;
static esp_ota_handle_t s_ota_handle = 0;
static bool s_ota_in_progress = false;

#define QYMERA_MAX_PWM_CHANNELS 8
static bool s_pwm_channel_used[QYMERA_MAX_PWM_CHANNELS] = {0};
static int s_pwm_pin_map[QYMERA_MAX_PWM_CHANNELS] = {-1, -1, -1, -1, -1, -1, -1, -1};
static bool s_pwm_fading[QYMERA_MAX_PWM_CHANNELS] = {0};

/* =========================
 * GPIO Implementation
 * ========================= */

qymera_err_t qymera_gpio_init(void) {
    if (s_hal_initialized) return QYMERA_OK;
    
    s_wifi_mutex = xSemaphoreCreateMutex();
    if (!s_wifi_mutex) return QYMERA_ERR_BUSY;
    
    s_hal_initialized = true;
    printf("[HAL] Initialized\n");
    return QYMERA_OK;
}

qymera_err_t qymera_gpio_set_mode(int pin, qymera_gpio_mode_t mode) {
    if (!s_hal_initialized) return QYMERA_ERR_INVALID_STATE;
    if (pin < 0 || pin > 39) return QYMERA_ERR_INVALID_ARG;
    
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << pin),
        .mode = GPIO_MODE_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    
    switch (mode) {
        case QYMERA_GPIO_MODE_INPUT:
            cfg.mode = GPIO_MODE_INPUT;
            break;
        case QYMERA_GPIO_MODE_OUTPUT:
            cfg.mode = GPIO_MODE_OUTPUT;
            break;
        case QYMERA_GPIO_MODE_INPUT_PULLUP:
            cfg.mode = GPIO_MODE_INPUT;
            cfg.pull_up_en = GPIO_PULLUP_ENABLE;
            break;
        case QYMERA_GPIO_MODE_INPUT_PULLDOWN:
            cfg.mode = GPIO_MODE_INPUT;
            cfg.pull_down_en = GPIO_PULLDOWN_ENABLE;
            break;
    }
    
    esp_err_t err = gpio_config(&cfg);
    return (err == ESP_OK) ? QYMERA_OK : QYMERA_ERR_INVALID_ARG;
}

qymera_err_t qymera_gpio_write(int pin, qymera_gpio_level_t level) {
    if (!s_hal_initialized) return QYMERA_ERR_INVALID_STATE;
    if (pin < 0 || pin > 39) return QYMERA_ERR_INVALID_ARG;
    
    esp_err_t err = gpio_set_level((gpio_num_t)pin, (uint32_t)level);
    return (err == ESP_OK) ? QYMERA_OK : QYMERA_ERR_INVALID_ARG;
}

qymera_gpio_level_t qymera_gpio_read(int pin) {
    if (!s_hal_initialized || pin < 0 || pin > 39) return QYMERA_GPIO_LOW;
    return gpio_get_level((gpio_num_t)pin) ? QYMERA_GPIO_HIGH : QYMERA_GPIO_LOW;
}

/* =========================
 * PWM / LEDC Implementation
 * ========================= */

static int qymera_pwm_find_free_channel(void) {
    for (int i = 0; i < QYMERA_MAX_PWM_CHANNELS; i++) {
        if (!s_pwm_channel_used[i]) return i;
    }
    return -1;
}

qymera_err_t qymera_pwm_init(const qymera_pwm_config_t *config) {
    if (!config || config->pin < 0 || config->pin > 39) return QYMERA_ERR_INVALID_ARG;
    if (config->channel < 0 || config->channel >= QYMERA_MAX_PWM_CHANNELS) return QYMERA_ERR_INVALID_ARG;
    
    int channel = config->channel;
    if (s_pwm_channel_used[channel] && s_pwm_pin_map[channel] != config->pin) {
        return QYMERA_ERR_BUSY;
    }
    
    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = (ledc_timer_bit_t)config->resolution_bits,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = (uint32_t)config->frequency_hz,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    esp_err_t err = ledc_timer_config(&timer_cfg);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return QYMERA_ERR_INVALID_ARG;
    }
    
    ledc_channel_config_t ch_cfg = {
        .gpio_num = config->pin,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = (ledc_channel_t)config->channel,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
        .flags = { .output_invert = (uint32_t)(config->inverted ? 1 : 0) }
    };
    esp_err_t ch_err = ledc_channel_config(&ch_cfg);
    if (ch_err != ESP_OK) return QYMERA_ERR_INVALID_ARG;
    
    s_pwm_channel_used[channel] = true;
    s_pwm_pin_map[channel] = config->pin;
    s_pwm_fading[channel] = false;
    
    return QYMERA_OK;
}

qymera_err_t qymera_pwm_set_duty(int channel, uint32_t duty) {
    if (channel < 0 || channel >= QYMERA_MAX_PWM_CHANNELS || !s_pwm_channel_used[channel]) {
        return QYMERA_ERR_INVALID_ARG;
    }
    
    s_pwm_fading[channel] = false;
    esp_err_t err = ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel, duty);
    if (err != ESP_OK) return QYMERA_ERR_INVALID_ARG;
    
    err = ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel);
    return (err == ESP_OK) ? QYMERA_OK : QYMERA_ERR_INVALID_ARG;
}

qymera_err_t qymera_pwm_set_fade(int channel, uint32_t target_duty, uint32_t duration_ms) {
    if (channel < 0 || channel >= QYMERA_MAX_PWM_CHANNELS || !s_pwm_channel_used[channel]) {
        return QYMERA_ERR_INVALID_ARG;
    }
    
    esp_err_t err = ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel, target_duty, (int)duration_ms);
    if (err != ESP_OK) return QYMERA_ERR_INVALID_ARG;
    
    err = ledc_fade_start(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel, LEDC_FADE_NO_WAIT);
    if (err != ESP_OK) return QYMERA_ERR_INVALID_ARG;
    
    s_pwm_fading[channel] = true;
    return QYMERA_OK;
}

uint32_t qymera_pwm_get_duty(int channel) {
    if (channel < 0 || channel >= QYMERA_MAX_PWM_CHANNELS || !s_pwm_channel_used[channel]) {
        return 0;
    }
    return ledc_get_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel);
}

bool qymera_pwm_is_fading(int channel) {
    if (channel < 0 || channel >= QYMERA_MAX_PWM_CHANNELS) return false;
    return s_pwm_fading[channel];
}

/* =========================
 * WiFi / Network Implementation
 * ========================= */

/* Arduino WiFi event callback for AP-side diagnostics (association / DHCP). */
static void onArduinoWiFiEvent(arduino_event_id_t event) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
            printf("[HAL] AP station connected\n");
            break;
        case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
            printf("[HAL] AP station disconnected\n");
            break;
        case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
            printf("[HAL] AP assigned IP to station\n");
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            printf("[HAL] WiFi STA disconnected\n");
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            printf("[HAL] STA got IP\n");
            break;
        default:
            break;
    }
}

qymera_err_t qymera_netif_init(void) {
    if (s_netif_initialized) return QYMERA_OK;
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return QYMERA_ERR_NETWORK;
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return QYMERA_ERR_NETWORK;
    s_netif_initialized = true;
    return QYMERA_OK;
}

qymera_err_t qymera_wifi_init(void) {
    if (s_wifi_initialized) return QYMERA_OK;

    /* In the Arduino-ESP32 framework the WiFi driver, netif instances and DHCP
     * are owned by the Arduino WiFi library: it calls esp_wifi_init() itself
     * (with its own rx/tx buffer config) and creates the default AP/STA netifs.
     * Calling esp_wifi_init() or esp_wifi_stop()/start() directly here would
     * either fail with ESP_ERR_INVALID_ARG (double init) or tear down the AP
     * netif/DHCP configured by Arduino. So we delegate the whole WiFi lifecycle
     * to the Arduino API and only wrap it behind qymera_* functions. */
    qymera_err_t nerr = qymera_netif_init();
    if (nerr != QYMERA_OK) return nerr;

    /* Force the Arduino WiFi subsystem to perform its low-level init (creates
     * the driver + netifs) without starting any mode yet. */
    WiFi.mode(WIFI_OFF);
    WiFi.onEvent(onArduinoWiFiEvent);

    s_wifi_initialized = true;
    return QYMERA_OK;
}

qymera_err_t qymera_wifi_set_mode(qymera_wifi_mode_t mode) {
    if (!s_wifi_initialized) return QYMERA_ERR_INVALID_STATE;
    
    wifi_mode_t wifi_mode;
    switch (mode) {
        case QYMERA_WIFI_MODE_STA: wifi_mode = WIFI_MODE_STA; break;
        case QYMERA_WIFI_MODE_AP: wifi_mode = WIFI_MODE_AP; break;
        case QYMERA_WIFI_MODE_APSTA: wifi_mode = WIFI_MODE_APSTA; break;
        default: return QYMERA_ERR_INVALID_ARG;
    }
    
    esp_err_t err = esp_wifi_set_mode(wifi_mode);
    return (err == ESP_OK) ? QYMERA_OK : QYMERA_ERR_NETWORK;
}

qymera_err_t qymera_wifi_sta_connect(const qymera_wifi_sta_config_t *config) {
    if (!s_wifi_initialized) return QYMERA_ERR_INVALID_STATE;
    if (!config) return QYMERA_ERR_INVALID_ARG;

    /* Use the Arduino WiFi library as the single owner of the driver/netif
     * lifecycle (same rule as AP mode). Calling esp_wifi_* directly here would
     * desync the driver state that Arduino owns, which is why STA never came up
     * after saving credentials. Switch to STA and begin the connection. */
    if (config->hostname[0]) WiFi.setHostname(config->hostname);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(config->ssid, config->password);

    /* The ARDUINO_EVENT_WIFI_STA_* events (handled by onArduinoWiFiEvent) and
     * qymera_wifi_is_connected() / qymera_wifi_get_mode() reflect the result. */
    return QYMERA_OK;
}

qymera_err_t qymera_wifi_ap_start(const qymera_wifi_ap_config_t *config) {
    if (!s_wifi_initialized) return QYMERA_ERR_INVALID_STATE;
    if (!config) return QYMERA_ERR_INVALID_ARG;

    /* Use the Arduino WiFi library as the single owner of the driver/netif
     * lifecycle. softAP() configures AP mode, the AP netif and DHCP server. */
    uint8_t channel = config->channel ? config->channel : 1;
    int max_conn = config->max_connections ? config->max_connections : 4;
    bool ok = WiFi.softAP(config->ssid,
                          config->password[0] ? config->password : NULL,
                          channel,
                          max_conn);
    if (!ok) {
        printf("[WIFI] softAP failed\n");
        return QYMERA_ERR_NETWORK;
    }
    WiFi.setSleep(false);
    return QYMERA_OK;
}

bool qymera_wifi_is_connected(void) {
    if (!s_wifi_initialized) return false;
    return WiFi.status() == WL_CONNECTED;
}

qymera_err_t qymera_wifi_get_ip(char *ip_str, size_t len) {
    if (!s_wifi_initialized || !ip_str || len < 16) return QYMERA_ERR_INVALID_ARG;
    IPAddress ip = WiFi.localIP();
    if (ip.toString() == "0.0.0.0") return QYMERA_ERR_NETWORK;
    snprintf(ip_str, len, "%s", ip.toString().c_str());
    return QYMERA_OK;
}

int8_t qymera_wifi_get_rssi(void) {
    if (!s_wifi_initialized) return -128;
    int32_t rssi = WiFi.RSSI();
    return (rssi == 0) ? -128 : (int8_t)rssi;
}

qymera_wifi_mode_t qymera_wifi_get_mode(void) {
    if (!s_wifi_initialized) return QYMERA_WIFI_MODE_STA;
    switch (WiFi.getMode()) {
        case WIFI_MODE_AP: return QYMERA_WIFI_MODE_AP;
        case WIFI_MODE_APSTA: return QYMERA_WIFI_MODE_APSTA;
        case WIFI_MODE_STA: return QYMERA_WIFI_MODE_STA;
        default: return QYMERA_WIFI_MODE_STA;
    }
}

qymera_err_t qymera_wifi_get_ap_ssid(char *ssid, size_t len) {
    if (!s_wifi_initialized || !ssid || len == 0) return QYMERA_ERR_INVALID_ARG;
    String s = WiFi.softAPSSID();
    if (s.length() >= len) return QYMERA_ERR_NETWORK;
    strncpy(ssid, s.c_str(), len - 1);
    ssid[len - 1] = '\0';
    return QYMERA_OK;
}

qymera_err_t qymera_wifi_get_ap_ip(char *ip_str, size_t len) {
    if (!s_wifi_initialized || !ip_str || len < 16) return QYMERA_ERR_INVALID_ARG;
    IPAddress ip = WiFi.softAPIP();
    if (ip.toString() == "0.0.0.0") return QYMERA_ERR_NETWORK;
    snprintf(ip_str, len, "%s", ip.toString().c_str());
    return QYMERA_OK;
}

int8_t qymera_wifi_ap_client_count(void) {
    if (!s_wifi_initialized) return -1;
    return (int8_t)WiFi.softAPgetStationNum();
}

void qymera_wifi_set_auto_reconnect(bool enable) {
    if (s_wifi_initialized) {
        WiFi.setAutoReconnect(enable);
    }
}

qymera_err_t qymera_wifi_scan(char *out, size_t len) {
    if (!out || len == 0) return QYMERA_ERR_INVALID_ARG;
    out[0] = '[';
    size_t used = 1;
    int n = WiFi.scanNetworks();
    if (n < 0) {
        if (len >= 3) { out[0] = '['; out[1] = ']'; out[2] = '\0'; }
        return QYMERA_OK;
    }
    for (int i = 0; i < n; i++) {
        String ssid = WiFi.SSID(i);
        int rssi = WiFi.RSSI(i);
        /* Bound: { "ssid" : "..escaped.." , "rssi" : .. , } worst case ~ 8 + 2*slen. */
        size_t need = 8 + ssid.length() * 2 + 12;
        if (used + need + 2 > len) break;
        if (i > 0) out[used++] = ',';
        out[used++] = '{';
        memcpy(out + used, "\"ssid\":\"", 8); used += 8;
        for (size_t k = 0; k < ssid.length(); k++) {
            char c = ssid.charAt(k);
            if (c == '"' || c == '\\') out[used++] = '\\';
            out[used++] = c;
        }
        used += (size_t)snprintf(out + used, len - used, "\",\"rssi\":%d}", rssi);
    }
    if (used + 1 < len) { out[used++] = ']'; out[used] = '\0'; }
    WiFi.scanDelete();
    return QYMERA_OK;
}

/* =========================
 * UDP Sockets Implementation
 * ========================= */

struct qymera_udp_socket {
    int sockfd;
    bool broadcast;
};

qymera_err_t qymera_udp_socket_create(const qymera_udp_socket_config_t *config, qymera_udp_socket_t *sock) {
    if (!config || !sock) return QYMERA_ERR_INVALID_ARG;
    
    qymera_err_t nerr = qymera_netif_init();
    if (nerr != QYMERA_OK) return nerr;
    
    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sockfd < 0) return QYMERA_ERR_NETWORK;
    
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    if (config->broadcast) {
        setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));
    }
    
    struct timeval tv = { .tv_sec = 0, .tv_usec = 100000 };
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config->port);
    if (config->bind_ip[0] == '\0') {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        inet_pton(AF_INET, config->bind_ip, &addr.sin_addr);
    }
    
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sockfd);
        return QYMERA_ERR_NETWORK;
    }
    
    struct qymera_udp_socket *s = (struct qymera_udp_socket *)malloc(sizeof(struct qymera_udp_socket));
    if (!s) {
        close(sockfd);
        return QYMERA_ERR_NO_SPACE;
    }
    
    s->sockfd = sockfd;
    s->broadcast = config->broadcast;
    *sock = s;
    return QYMERA_OK;
}

qymera_err_t qymera_udp_socket_close(qymera_udp_socket_t sock) {
    if (!sock) return QYMERA_ERR_INVALID_ARG;
    struct qymera_udp_socket *s = (struct qymera_udp_socket *)sock;
    close(s->sockfd);
    free(s);
    return QYMERA_OK;
}

qymera_err_t qymera_udp_socket_send(qymera_udp_socket_t sock, const char *dest_ip, uint16_t port,
                                    const void *data, size_t len) {
    if (!sock || !dest_ip || !data) return QYMERA_ERR_INVALID_ARG;
    struct qymera_udp_socket *s = (struct qymera_udp_socket *)sock;
    
    struct sockaddr_in dest = {0};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(port);
    inet_pton(AF_INET, dest_ip, &dest.sin_addr);
    
    ssize_t sent = sendto(s->sockfd, data, len, 0, (struct sockaddr *)&dest, sizeof(dest));
    return (sent == (ssize_t)len) ? QYMERA_OK : QYMERA_ERR_NETWORK;
}

qymera_err_t qymera_udp_socket_recv(qymera_udp_socket_t sock, char *src_ip, uint16_t *src_port,
                                    void *buffer, size_t buffer_len, size_t *received_len) {
    if (!sock || !buffer || !received_len) return QYMERA_ERR_INVALID_ARG;
    struct qymera_udp_socket *s = (struct qymera_udp_socket *)sock;
    
    struct sockaddr_in src = {0};
    socklen_t src_len = sizeof(src);
    ssize_t recv_len = recvfrom(s->sockfd, buffer, buffer_len, 0,
                                (struct sockaddr *)&src, &src_len);
    
    if (recv_len < 0) {
        *received_len = 0;
        return QYMERA_ERR_TIMEOUT;
    }
    
    if (src_ip) inet_ntop(AF_INET, &src.sin_addr, src_ip, 16);
    if (src_port) *src_port = ntohs(src.sin_port);
    *received_len = (size_t)recv_len;
    return QYMERA_OK;
}

qymera_err_t qymera_udp_socket_set_rx_timeout(qymera_udp_socket_t sock, uint32_t timeout_ms) {
    if (!sock) return QYMERA_ERR_INVALID_ARG;
    struct qymera_udp_socket *s = (struct qymera_udp_socket *)sock;
    struct timeval tv = { .tv_sec = (time_t)(timeout_ms / 1000), .tv_usec = (suseconds_t)((timeout_ms % 1000) * 1000) };
    return (setsockopt(s->sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == 0) ? QYMERA_OK : QYMERA_ERR_NETWORK;
}

/* =========================
 * Time / NTP Implementation
 * ========================= */

static int32_t s_timezone_offset_min = 0;
static bool s_time_synced = false;

qymera_err_t qymera_time_init(void) {
    return QYMERA_OK;
}

qymera_err_t qymera_time_set_timezone(int32_t offset_minutes) {
    if (offset_minutes < -720 || offset_minutes > 840) return QYMERA_ERR_INVALID_ARG;
    s_timezone_offset_min = offset_minutes;
    return QYMERA_OK;
}

qymera_err_t qymera_time_sync_ntp(const char *ntp_server1, const char *ntp_server2) {
    if (!s_wifi_initialized || !qymera_wifi_is_connected()) return QYMERA_ERR_NETWORK;
    
    configTime(s_timezone_offset_min * 60, 0, ntp_server1 ? ntp_server1 : "pool.ntp.org", ntp_server2 ? ntp_server2 : "time.nist.gov");
    
    for (int i = 0; i < 100; i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
        time_t now = 0;
        time(&now);
        if (now > 1704067200) {
            s_time_synced = true;
            break;
        }
    }
    
    return s_time_synced ? QYMERA_OK : QYMERA_ERR_TIMEOUT;
}

bool qymera_time_is_valid(void) {
    time_t now = 0;
    time(&now);
    return now > 1704067200;
}

qymera_rtc_time_t qymera_time_get_local(void) {
    qymera_rtc_time_t rt = {0};
    time_t now = 0;
    time(&now);
    if (now < 1704067200) return rt;
    
    time_t local = now + (time_t)s_timezone_offset_min * 60;
    struct tm *timeinfo = gmtime(&local);
    if (!timeinfo) return rt;
    
    rt.year = timeinfo->tm_year + 1900;
    rt.month = timeinfo->tm_mon + 1;
    rt.day = timeinfo->tm_mday;
    rt.hour = timeinfo->tm_hour;
    rt.minute = timeinfo->tm_min;
    rt.second = timeinfo->tm_sec;
    rt.timezone_offset_min = s_timezone_offset_min;
    return rt;
}

uint32_t qymera_time_get_unix(void) {
    time_t now = 0;
    time(&now);
    return (uint32_t)now;
}

uint16_t qymera_time_get_minutes_of_day(void) {
    qymera_rtc_time_t t = qymera_time_get_local();
    return t.hour * 60 + t.minute;
}

/* =========================
 * NVS Implementation
 * ========================= */

qymera_err_t qymera_nvs_init(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    return (err == ESP_OK) ? QYMERA_OK : QYMERA_ERR_STORAGE;
}

qymera_err_t qymera_nvs_set_blob(const char *namespace_, const char *key, const void *data, size_t len) {
    if (!namespace_ || !key || !data) return QYMERA_ERR_INVALID_ARG;
    
    nvs_handle_t handle;
    esp_err_t err = nvs_open(namespace_, NVS_READWRITE, &handle);
    if (err != ESP_OK) return QYMERA_ERR_STORAGE;
    
    err = nvs_set_blob(handle, key, data, len);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return (err == ESP_OK) ? QYMERA_OK : QYMERA_ERR_STORAGE;
}

qymera_err_t qymera_nvs_get_blob(const char *namespace_, const char *key, void *data, size_t *len) {
    if (!namespace_ || !key || !data || !len) return QYMERA_ERR_INVALID_ARG;
    
    nvs_handle_t handle;
    esp_err_t err = nvs_open(namespace_, NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) return QYMERA_ERR_NOT_FOUND;
    if (err != ESP_OK) return QYMERA_ERR_STORAGE;
    
    err = nvs_get_blob(handle, key, data, len);
    nvs_close(handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) return QYMERA_ERR_NOT_FOUND;
    return (err == ESP_OK) ? QYMERA_OK : QYMERA_ERR_STORAGE;
}

qymera_err_t qymera_nvs_erase_key(const char *namespace_, const char *key) {
    if (!namespace_ || !key) return QYMERA_ERR_INVALID_ARG;
    
    nvs_handle_t handle;
    esp_err_t err = nvs_open(namespace_, NVS_READWRITE, &handle);
    if (err != ESP_OK) return QYMERA_ERR_STORAGE;
    
    err = nvs_erase_key(handle, key);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return (err == ESP_OK) ? QYMERA_OK : QYMERA_ERR_STORAGE;
}

qymera_err_t qymera_nvs_commit(const char *namespace_) {
    if (!namespace_) return QYMERA_ERR_INVALID_ARG;
    nvs_handle_t handle;
    esp_err_t err = nvs_open(namespace_, NVS_READWRITE, &handle);
    if (err != ESP_OK) return QYMERA_ERR_STORAGE;
    err = nvs_commit(handle);
    nvs_close(handle);
    return (err == ESP_OK) ? QYMERA_OK : QYMERA_ERR_STORAGE;
}

/* =========================
 * System Implementation
 * ========================= */

qymera_err_t qymera_system_init(void) {
    return qymera_gpio_init();
}

void qymera_system_restart(void) {
    esp_restart();
}

uint32_t qymera_system_get_free_heap(void) {
    return esp_get_free_heap_size();
}

uint32_t qymera_system_get_chip_id(void) {
    uint64_t mac = 0;
    esp_efuse_mac_get_default((uint8_t *)&mac);
    return (uint32_t)mac;
}

const char *qymera_system_get_reset_reason(void) {
    esp_reset_reason_t reason = esp_reset_reason();
    switch (reason) {
        case ESP_RST_POWERON: return "power_on";
        case ESP_RST_EXT: return "external";
        case ESP_RST_SW: return "software";
        case ESP_RST_PANIC: return "panic";
        case ESP_RST_INT_WDT: return "int_wdt";
        case ESP_RST_TASK_WDT: return "task_wdt";
        case ESP_RST_WDT: return "wdt";
        case ESP_RST_DEEPSLEEP: return "deepsleep";
        case ESP_RST_BROWNOUT: return "brownout";
        case ESP_RST_SDIO: return "sdio";
        default: return "unknown";
    }
}

uint32_t qymera_system_get_uptime_ms(void) {
    return (uint32_t)(esp_timer_get_time() / 1000);
}

/* =========================
 * OTA Implementation
 * ========================= */

qymera_err_t qymera_ota_init(const char *hostname) {
    (void)hostname;
    return QYMERA_OK;
}

qymera_err_t qymera_ota_handle(void) {
    return QYMERA_OK;
}

qymera_ota_state_t qymera_ota_get_state(void) {
    return s_ota_in_progress ? QYMERA_OTA_IN_PROGRESS : QYMERA_OTA_IDLE;
}

void qymera_ota_set_enabled(bool enabled) {
    s_ota_enabled = enabled;
}

bool qymera_ota_is_enabled(void) {
    return s_ota_enabled;
}

/* =========================
 * HAL Initialization
 * ========================= */

qymera_err_t qymera_hal_init(void) {
    return qymera_system_init();
}

/* =========================
 * Early Logging
 * ========================= */

void qymera_log_early(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}
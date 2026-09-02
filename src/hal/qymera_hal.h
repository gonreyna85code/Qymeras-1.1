/**
 * Qymera Dashboard - HAL Interface
 * Hardware abstraction layer replacing Arduino APIs
 */
#pragma once

#include "qymera_types.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================
 * GPIO
 * ========================= */

typedef enum {
    QYMERA_GPIO_MODE_INPUT = 0,
    QYMERA_GPIO_MODE_OUTPUT,
    QYMERA_GPIO_MODE_INPUT_PULLUP,
    QYMERA_GPIO_MODE_INPUT_PULLDOWN,
} qymera_gpio_mode_t;

typedef enum {
    QYMERA_GPIO_LOW = 0,
    QYMERA_GPIO_HIGH = 1,
} qymera_gpio_level_t;

qymera_err_t qymera_gpio_init(void);
qymera_err_t qymera_gpio_set_mode(int pin, qymera_gpio_mode_t mode);
qymera_err_t qymera_gpio_write(int pin, qymera_gpio_level_t level);
qymera_gpio_level_t qymera_gpio_read(int pin);

/* =========================
 * PWM / LEDC (for dimmers)
 * ========================= */

typedef struct {
    int pin;
    int channel;
    int frequency_hz;
    int resolution_bits;
    bool inverted;
} qymera_pwm_config_t;

qymera_err_t qymera_pwm_init(const qymera_pwm_config_t *config);
qymera_err_t qymera_pwm_set_duty(int channel, uint32_t duty);
qymera_err_t qymera_pwm_set_fade(int channel, uint32_t target_duty, uint32_t duration_ms);
uint32_t qymera_pwm_get_duty(int channel);
bool qymera_pwm_is_fading(int channel);

/* =========================
 * WiFi / Network
 * ========================= */

typedef enum {
    QYMERA_WIFI_MODE_STA = 0,
    QYMERA_WIFI_MODE_AP,
    QYMERA_WIFI_MODE_APSTA,
} qymera_wifi_mode_t;

typedef struct {
    char ssid[33];
    char password[65];
    char hostname[33];
} qymera_wifi_sta_config_t;

typedef struct {
    char ssid[33];
    char password[65];
    uint8_t channel;
    uint8_t max_connections;
} qymera_wifi_ap_config_t;

qymera_err_t qymera_wifi_init(void);
qymera_err_t qymera_netif_init(void);
qymera_err_t qymera_wifi_set_mode(qymera_wifi_mode_t mode);
qymera_err_t qymera_wifi_sta_connect(const qymera_wifi_sta_config_t *config);
qymera_err_t qymera_wifi_ap_start(const qymera_wifi_ap_config_t *config);
bool qymera_wifi_is_connected(void);
qymera_err_t qymera_wifi_get_ip(char *ip_str, size_t len);
int8_t qymera_wifi_get_rssi(void);
void qymera_wifi_set_auto_reconnect(bool enable);

/* Network-mode introspection for the status endpoint / UI. */
qymera_wifi_mode_t qymera_wifi_get_mode(void);
qymera_err_t qymera_wifi_get_ap_ssid(char *ssid, size_t len);
qymera_err_t qymera_wifi_get_ap_ip(char *ip_str, size_t len);

/* =========================
 * UDP Sockets (raw socket API)
 * ========================= */

typedef void *qymera_udp_socket_t;

typedef struct {
    uint16_t port;
    char bind_ip[16];
    bool broadcast;
} qymera_udp_socket_config_t;

qymera_err_t qymera_udp_socket_create(const qymera_udp_socket_config_t *config, qymera_udp_socket_t *sock);
qymera_err_t qymera_udp_socket_close(qymera_udp_socket_t sock);
qymera_err_t qymera_udp_socket_send(qymera_udp_socket_t sock, const char *dest_ip, uint16_t port,
                                    const void *data, size_t len);
qymera_err_t qymera_udp_socket_recv(qymera_udp_socket_t sock, char *src_ip, uint16_t *src_port,
                                    void *buffer, size_t buffer_len, size_t *received_len);
qymera_err_t qymera_udp_socket_set_rx_timeout(qymera_udp_socket_t sock, uint32_t timeout_ms);

/* =========================
 * Time / NTP
 * ========================= */

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    int32_t timezone_offset_min;
} qymera_rtc_time_t;

qymera_err_t qymera_time_init(void);
qymera_err_t qymera_time_set_timezone(int32_t offset_minutes);
qymera_err_t qymera_time_sync_ntp(const char *ntp_server1, const char *ntp_server2);
bool qymera_time_is_valid(void);
qymera_rtc_time_t qymera_time_get_local(void);
uint32_t qymera_time_get_unix(void);
uint16_t qymera_time_get_minutes_of_day(void);

/* =========================
 * NVS / Storage
 * ========================= */

qymera_err_t qymera_nvs_init(void);
qymera_err_t qymera_nvs_set_blob(const char *namespace_, const char *key, const void *data, size_t len);
qymera_err_t qymera_nvs_get_blob(const char *namespace_, const char *key, void *data, size_t *len);
qymera_err_t qymera_nvs_erase_key(const char *namespace_, const char *key);
qymera_err_t qymera_nvs_commit(const char *namespace_);

/* =========================
 * System
 * ========================= */

qymera_err_t qymera_system_init(void);
void qymera_system_restart(void);
uint32_t qymera_system_get_free_heap(void);
uint32_t qymera_system_get_chip_id(void);
const char *qymera_system_get_reset_reason(void);
uint32_t qymera_system_get_uptime_ms(void);

/* =========================
 * OTA (optional)
 * ========================= */

typedef enum {
    QYMERA_OTA_IDLE = 0,
    QYMERA_OTA_IN_PROGRESS,
    QYMERA_OTA_COMPLETE,
    QYMERA_OTA_ERROR,
} qymera_ota_state_t;

qymera_err_t qymera_ota_init(const char *hostname);
qymera_err_t qymera_ota_handle(void);
qymera_ota_state_t qymera_ota_get_state(void);
void qymera_ota_set_enabled(bool enabled);
bool qymera_ota_is_enabled(void);

/* =========================
 * HAL Initialization
 * ========================= */

qymera_err_t qymera_hal_init(void);

#ifdef __cplusplus
}
#endif
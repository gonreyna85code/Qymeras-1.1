/**
 * Qymera Dashboard - Main Entry Point
 * Minimal user sketch demonstrating the Dashboard
 */
#include "qymera_core.h"
#include "qymera_hal.h"
#include "qymera_log.h"
#include "qymera_registry.h"
#include "qymera_rule.h"
#include "qymera_event_bus.h"
#include "qymera_http_api.h"
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* =========================
 * User Configuration
 * ========================= */

static const char *WIFI_SSID = "YOUR_SSID";
static const char *WIFI_PASSWORD = "YOUR_PASSWORD";

/* =========================
 * Demo Rule: temperature > 30 -> fan ON
 * ========================= */

static void setup_demo_rule(qymera_core_t *core) {
    qymera_rule_engine_t *engine = qymera_core_get_rule_engine(core);
    qymera_log_t *log = qymera_core_get_log(core);
    
    qymera_registry_t *registry = qymera_core_get_registry(core);
    
    uint16_t dev_idx;
    qymera_registry_find_device(registry, "dashboard", &dev_idx);
    
    qymera_entity_t temp_entity = {0};
    strncpy(temp_entity.device_id, "dashboard", sizeof(temp_entity.device_id) - 1);
    strncpy(temp_entity.entity_id, "temperature", sizeof(temp_entity.entity_id) - 1);
    strncpy(temp_entity.name, "Temperature", sizeof(temp_entity.name) - 1);
    temp_entity.type = QYMERA_ENTITY_SENSOR_TEMPERATURE;
    temp_entity.capabilities[0] = QYMERA_CAP_SENSOR_NUMERIC;
    temp_entity.capability_count = 1;
    strncpy(temp_entity.unit, "°C", sizeof(temp_entity.unit) - 1);
    temp_entity.native_min = -50;
    temp_entity.native_max = 150;
    
    uint16_t temp_idx;
    qymera_registry_register_entity(registry, dev_idx, &temp_entity, &temp_idx);
    
    qymera_entity_t fan_entity = {0};
    strncpy(fan_entity.device_id, "dashboard", sizeof(fan_entity.device_id) - 1);
    strncpy(fan_entity.entity_id, "fan", sizeof(fan_entity.entity_id) - 1);
    strncpy(fan_entity.name, "Fan Relay", sizeof(fan_entity.name) - 1);
    fan_entity.type = QYMERA_ENTITY_ACTUATOR_RELAY;
    fan_entity.capabilities[0] = QYMERA_CAP_ACTUATOR_RELAY;
    fan_entity.capability_count = 1;
    strncpy(fan_entity.unit, "bool", sizeof(fan_entity.unit) - 1);
    fan_entity.protected_actuator = false;
    
    uint16_t fan_idx;
    qymera_registry_register_entity(registry, dev_idx, &fan_entity, &fan_idx);
    
    qymera_rule_t rule = {0};
    strncpy(rule.rule_id, "rule-temp-fan-001", sizeof(rule.rule_id) - 1);
    strncpy(rule.name, "Temperature Fan Control", sizeof(rule.name) - 1);
    rule.enabled = true;
    rule.priority = 10;
    rule.cooldown_ms = 60000;
    
    rule.trigger.entity = (qymera_entity_ref_t){"dashboard", "temperature"};
    rule.trigger.operator_ = QYMERA_OP_GT;
    rule.trigger.threshold = 30.0f;
    rule.trigger.duration_ms = 0;
    rule.trigger.condition_type = 0;
    
    rule.actions[0].entity = (qymera_entity_ref_t){"dashboard", "fan"};
    rule.actions[0].action = QYMERA_ACTION_SET_BOOL;
    rule.actions[0].value_u32 = 1;
    rule.action_count = 1;
    
    qymera_validation_result_t result;
    qymera_rule_engine_validate(engine, &rule, &result);
    
    if (!result.valid) {
        qymera_log_error(log, "demo", "Rule validation failed:");
        for (int i = 0; i < result.error_count; i++) {
            qymera_log_error(log, "demo", "  %s", result.errors[i]);
        }
        return;
    }
    
    qymera_compiled_rule_t compiled;
    qymera_rule_engine_compile(engine, &rule, &compiled);
    
    uint16_t slot_idx;
    qymera_rule_engine_load(engine, &compiled, &slot_idx);
    
    qymera_log_info(log, "demo", "Demo rule loaded at slot %d", slot_idx);
}

static void simulate_sensor_reading(qymera_core_t *core) {
    static uint32_t last_sim = 0;
    uint32_t now = qymera_system_get_uptime_ms();
    
    if (now - last_sim >= 5000) {
        last_sim = now;
        
        static float temp = 25.0f;
        static float dir = 1.0f;
        temp += dir * 0.5f;
        if (temp >= 35.0f) { temp = 35.0f; dir = -1.0f; }
        if (temp <= 25.0f) { temp = 25.0f; dir = 1.0f; }
        
        qymera_registry_t *registry = qymera_core_get_registry(core);
        qymera_event_bus_t *event_bus = qymera_core_get_event_bus(core);
        
        uint16_t entity_idx;
        if (qymera_registry_find_entity(registry, "dashboard", "temperature", &entity_idx) == QYMERA_OK) {
            qymera_entity_value_t value = {0};
            value.valid = true;
            value.numeric_value = temp;
            value.timestamp = qymera_timestamp_now();
            
            qymera_registry_update_entity_value(registry, entity_idx, &value);
            
            qymera_event_t event;
            qymera_entity_value_t prev = { .valid = true, .numeric_value = temp - 0.5f };
            qymera_event_make_sensor_changed(&event, "dashboard", "temperature", &value, &prev);
            qymera_event_bus_publish(event_bus, &event);
        }
    }
}

int main(void) {
    qymera_hal_init();
    
    qymera_core_config_t config = {0};
    
    config.general.device_uid = qymera_system_get_chip_id();
    snprintf(config.general.device_name, sizeof(config.general.device_name), "Qymera-%08X", config.general.device_uid);
    config.general.timezone_offset_min = 0;
    
    config.network.sta_enabled = (WIFI_SSID[0] != 'Y');
    strncpy(config.network.sta_ssid, WIFI_SSID, sizeof(config.network.sta_ssid) - 1);
    strncpy(config.network.sta_password, WIFI_PASSWORD, sizeof(config.network.sta_password) - 1);
    strncpy(config.network.sta_hostname, config.general.device_name, sizeof(config.network.sta_hostname) - 1);
    config.network.udp_discovery_port = QYMERA_UDP_PORT_DISCOVERY;
    config.network.udp_control_port = QYMERA_UDP_PORT_CONTROL;
    config.network.report_interval_ms = 5000;
    
    config.ai.mode = QYMERA_AI_MODE_NONE;
    
    qymera_core_t *core = NULL;
    qymera_err_t err = qymera_core_init(&core, &config);
    if (err != QYMERA_OK) {
        qymera_log_early("Core init failed: %d", err);
        while (1) { qymera_system_restart(); }
    }
    
    qymera_log_t *log = qymera_core_get_log(core);
    qymera_log_system(log, "main", "Qymera Dashboard started");
    qymera_log_info(log, "main", "Device: %s (UID: %08X)", config.general.device_name, config.general.device_uid);
    
    setup_demo_rule(core);
    
    if (config.network.sta_enabled) {
        qymera_wifi_sta_config_t sta_cfg = {0};
        strncpy(sta_cfg.ssid, config.network.sta_ssid, sizeof(sta_cfg.ssid) - 1);
        strncpy(sta_cfg.password, config.network.sta_password, sizeof(sta_cfg.password) - 1);
        strncpy(sta_cfg.hostname, config.network.sta_hostname, sizeof(sta_cfg.hostname) - 1);
        qymera_wifi_sta_connect(&sta_cfg);
        qymera_log_info(log, "main", "Connecting to WiFi: %s", config.network.sta_ssid);
    } else {
        qymera_log_warn(log, "main", "WiFi not configured, starting AP mode");
        qymera_wifi_ap_config_t ap_cfg = {0};
        snprintf(ap_cfg.ssid, sizeof(ap_cfg.ssid), "Qymera-%08X", config.general.device_uid);
        ap_cfg.channel = 1;
        qymera_wifi_ap_start(&ap_cfg);
    }

    qymera_err_t herr = qymera_http_api_init(core);
    if (herr == QYMERA_OK) {
        qymera_log_info(log, "main", "Dashboard HTTP API started on port 80");
    } else {
        qymera_log_error(log, "main", "Dashboard HTTP API init failed: %d", herr);
    }

    qymera_log_info(log, "main", "Entering main loop");
    
    while (1) {
        qymera_core_tick(core);
        simulate_sensor_reading(core);
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    return 0;
}

/* Arduino framework entry points */
void setup(void) {
    // Initialize HAL first
    qymera_hal_init();
}

void loop(void) {
    // The actual application runs in main()
    // Arduino loop is kept minimal
    vTaskDelay(pdMS_TO_TICKS(1000));
}
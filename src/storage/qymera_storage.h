/**
 * Qymera Dashboard - Storage Abstraction
 * Unified interface for configuration, rules, registry, logs, telemetry
 */
#pragma once

#include "qymera_types.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================
 * Storage Namespaces
 * ========================= */

#define QYMERA_NS_CONFIG      "qymera_cfg"
#define QYMERA_NS_DEVICES     "qymera_dev"
#define QYMERA_NS_RULES       "qymera_rules"
#define QYMERA_NS_LOGS        "qymera_logs"
#define QYMERA_NS_TELEMETRY   "qymera_telm"
#define QYMERA_NS_AI          "qymera_ai"

/* =========================
 * Storage Keys
 * ========================= */

#define QYMERA_KEY_NETWORK    "network"
#define QYMERA_KEY_GENERAL    "general"
#define QYMERA_KEY_IDENTITY   "identity"
#define QYMERA_KEY_RULES      "rules_list"
#define QYMERA_KEY_RULE_PREFIX "rule_"
#define QYMERA_KEY_DEVICES    "devices_list"
#define QYMERA_KEY_DEVICE_PREFIX "dev_"

/* =========================
 * Network Configuration
 * ========================= */

typedef struct {
    char sta_ssid[33];
    char sta_password[65];
    char sta_hostname[33];
    bool sta_enabled;
    char ap_ssid[33];
    char ap_password[65];
    uint8_t ap_channel;
    uint16_t udp_discovery_port;
    uint16_t udp_control_port;
    uint32_t report_interval_ms;
} qymera_network_config_t;

/* =========================
 * General Settings
 * ========================= */

typedef struct {
    uint32_t device_uid;
    char device_name[QYMERA_DEVICE_ID_LEN];
    char location[64];
    int32_t timezone_offset_min;
    bool ota_enabled;
    uint32_t boot_count;
} qymera_general_config_t;

/* =========================
 * Rule Storage (metadata)
 * ========================= */

#define QYMERA_MAX_STORED_RULES 100

typedef struct {
    char rule_id[QYMERA_RULE_ID_LEN];
    uint32_t revision;
    uint32_t created_ts;
    uint32_t updated_ts;
    bool enabled;
    size_t compiled_size;
    uint32_t checksum;
} qymera_rule_meta_t;

typedef struct {
    uint32_t count;
    qymera_rule_meta_t rules[QYMERA_MAX_STORED_RULES];
} qymera_rules_index_t;

/* =========================
 * Storage Handle
 * ========================= */

typedef struct qymera_storage_s qymera_storage_t;

/* =========================
 * Storage API
 * ========================= */

/**
 * Initialize storage subsystem
 * @param storage Output storage handle
 * @return QYMERA_OK on success
 */
qymera_err_t qymera_storage_init(qymera_storage_t **storage);

/**
 * Load network configuration
 * @param storage Storage handle
 * @param config  Output configuration
 * @return QYMERA_OK on success, QYMERA_ERR_NOT_FOUND if not configured
 */
qymera_err_t qymera_storage_load_network(qymera_storage_t *storage, qymera_network_config_t *config);

/**
 * Save network configuration
 * @param storage Storage handle
 * @param config  Configuration to save
 * @return QYMERA_OK on success
 */
qymera_err_t qymera_storage_save_network(qymera_storage_t *storage, const qymera_network_config_t *config);

/**
 * Load general configuration
 * @param storage Storage handle
 * @param config  Output configuration
 * @return QYMERA_OK on success
 */
qymera_err_t qymera_storage_load_general(qymera_storage_t *storage, qymera_general_config_t *config);

/**
 * Save general configuration
 * @param storage Storage handle
 * @param config  Configuration to save
 * @return QYMERA_OK on success
 */
qymera_err_t qymera_storage_save_general(qymera_storage_t *storage, const qymera_general_config_t *config);

/**
 * Load rules index
 * @param storage Storage handle
 * @param index   Output rules index
 * @return QYMERA_OK on success
 */
qymera_err_t qymera_storage_load_rules_index(qymera_storage_t *storage, qymera_rules_index_t *index);

/**
 * Save rules index
 * @param storage Storage handle
 * @param index   Rules index to save
 * @return QYMERA_OK on success
 */
qymera_err_t qymera_storage_save_rules_index(qymera_storage_t *storage, const qymera_rules_index_t *index);

/**
 * Load a compiled rule by ID
 * @param storage   Storage handle
 * @param rule_id   Rule ID
 * @param buffer    Output buffer for compiled rule
 * @param buf_len   Buffer length
 * @param actual_len Output: actual rule size
 * @return QYMERA_OK on success
 */
qymera_err_t qymera_storage_load_rule(qymera_storage_t *storage, const char *rule_id,
                                       void *buffer, size_t buf_len, size_t *actual_len);

/**
 * Save a compiled rule
 * @param storage   Storage handle
 * @param rule_id   Rule ID
 * @param buffer    Compiled rule data
 * @param len       Data length
 * @param meta      Rule metadata
 * @return QYMERA_OK on success
 */
qymera_err_t qymera_storage_save_rule(qymera_storage_t *storage, const char *rule_id,
                                       const void *buffer, size_t len, const qymera_rule_meta_t *meta);

/**
 * Delete a rule
 * @param storage Storage handle
 * @param rule_id Rule ID to delete
 * @return QYMERA_OK on success
 */
qymera_err_t qymera_storage_delete_rule(qymera_storage_t *storage, const char *rule_id);

/**
 * Load device registry (list of device IDs)
 * @param storage     Storage handle
 * @param device_ids  Output array of device IDs
 * @param max_ids     Max device IDs to load
 * @param actual_count Output: actual count loaded
 * @return QYMERA_OK on success
 */
qymera_err_t qymera_storage_load_device_list(qymera_storage_t *storage, char (*device_ids)[QYMERA_DEVICE_ID_LEN],
                                              size_t max_ids, size_t *actual_count);

/**
 * Load a device configuration
 * @param storage   Storage handle
 * @param device_id Device ID
 * @param buffer    Output buffer
 * @param buf_len   Buffer length
 * @param actual_len Output: actual size
 * @return QYMERA_OK on success
 */
qymera_err_t qymera_storage_load_device(qymera_storage_t *storage, const char *device_id,
                                         void *buffer, size_t buf_len, size_t *actual_len);

/**
 * Save a device configuration
 * @param storage   Storage handle
 * @param device_id Device ID
 * @param buffer    Device data
 * @param len       Data length
 * @return QYMERA_OK on success
 */
qymera_err_t qymera_storage_save_device(qymera_storage_t *storage, const char *device_id,
                                         const void *buffer, size_t len);

/**
 * Generic blob storage (for telemetry, logs, etc.)
 * @param storage Storage handle
 * @param namespace_ Namespace
 * @param key     Key
 * @param data    Data to store
 * @param len     Data length
 * @return QYMERA_OK on success
 */
qymera_err_t qymera_storage_put_blob(qymera_storage_t *storage, const char *namespace_,
                                      const char *key, const void *data, size_t len);

/**
 * Generic blob retrieval
 * @param storage  Storage handle
 * @param namespace_ Namespace
 * @param key      Key
 * @param data     Output buffer
 * @param len      Input: buffer size, Output: actual data size
 * @return QYMERA_OK on success, QYMERA_ERR_NOT_FOUND if missing
 */
qymera_err_t qymera_storage_get_blob(qymera_storage_t *storage, const char *namespace_,
                                      const char *key, void *data, size_t *len);

/**
 * Erase a key
 * @param storage Storage handle
 * @param namespace_ Namespace
 * @param key      Key to erase
 * @return QYMERA_OK on success
 */
qymera_err_t qymera_storage_erase(qymera_storage_t *storage, const char *namespace_,
                                   const char *key);

/**
 * Factory reset (erase all user data, keep credentials optional)
 * @param storage Storage handle
 * @param keep_credentials true to preserve WiFi credentials
 * @return QYMERA_OK on success
 */
qymera_err_t qymera_storage_factory_reset(qymera_storage_t *storage, bool keep_credentials);

#ifdef __cplusplus
}
#endif
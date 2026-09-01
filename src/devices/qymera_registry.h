/**
 * Qymera Dashboard - Device Registry
 * Scalable device and entity registry with capabilities
 */
#pragma once

#include "qymera_types.h"
#include "qymera_ring.h"
#include "qymera_control_context.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================
 * Entity Definition
 * ========================= */

typedef struct {
    char device_id[QYMERA_DEVICE_ID_LEN];
    char entity_id[QYMERA_ENTITY_ID_LEN];
    char name[QYMERA_ENTITY_ID_LEN];
    qymera_entity_type_t type;
    qymera_capability_t capabilities[4];  // Multiple capabilities per entity
    uint8_t capability_count;
    char unit[16];
    float native_min;
    float native_max;
    float calibration_min;
    float calibration_max;
    float correction;
    bool persist_state;
    uint32_t pulse_ms;
    uint32_t fade_ms;
    bool protected_actuator;
    int8_t gpio_pin;  // GPIO pin for actuator control (-1 = not mapped)
    qymera_entity_value_t value;
    qymera_timestamp_t last_updated;
} qymera_entity_t;

/* =========================
 * Device Definition
 * ========================= */

typedef struct {
    char device_id[QYMERA_DEVICE_ID_LEN];
    char name[QYMERA_DEVICE_ID_LEN];
    uint32_t chip_uid;
    char model[32];
    char fw_version[32];
    uint8_t role;  // 0=dashboard, 1=remote, 2=provisioning
    qymera_timestamp_t registered_at;
    qymera_timestamp_t last_seen;
    bool online;
    uint8_t state;  // 0=operational, 1=offline, 2=degraded, 3=provisioning
    char location[64];
    uint16_t entity_indices[16];  // Indices into entity table
    uint8_t entity_count;
    char ip_addr[16];  // For remote devices
    uint16_t port;
} qymera_device_t;

/* =========================
 * Registry Configuration
 * ========================= */

typedef struct {
    qymera_device_t *devices;
    size_t max_devices;
    qymera_entity_t *entities;
    size_t max_entities;
    qymera_ring_t event_ring;  // For device lifecycle events
} qymera_registry_config_t;

/* =========================
 * Registry Handle
 * ========================= */

typedef struct qymera_registry_s qymera_registry_t;

/* =========================
 * Registry API
 * ========================= */

/**
 * Initialize the device registry
 * @param registry Pointer to registry handle
 * @param config   Configuration with pre-allocated storage
 * @return QYMERA_OK on success
 */
qymera_err_t qymera_registry_init(qymera_registry_t **registry, const qymera_registry_config_t *config);

/**
 * Register a new device
 * @param registry    Registry handle
 * @param device      Device info to register
 * @param device_idx  Output: assigned device index
 * @return QYMERA_OK on success, QYMERA_ERR_NO_SPACE if full
 */
qymera_err_t qymera_registry_register_device(qymera_registry_t *registry, const qymera_device_t *device, uint16_t *device_idx);

/**
 * Register an entity on a device
 * @param registry    Registry handle
 * @param device_idx  Device index
 * @param entity      Entity info to register
 * @param entity_idx  Output: assigned entity index
 * @return QYMERA_OK on success
 */
qymera_err_t qymera_registry_register_entity(qymera_registry_t *registry, uint16_t device_idx, const qymera_entity_t *entity, uint16_t *entity_idx);

/**
 * Find device by ID
 * @param registry   Registry handle
 * @param device_id  Device ID string
 * @param device_idx Output: device index if found
 * @return QYMERA_OK if found, QYMERA_ERR_NOT_FOUND otherwise
 */
qymera_err_t qymera_registry_find_device(qymera_registry_t *registry, const char *device_id, uint16_t *device_idx);

/**
 * Find entity by device_id + entity_id
 * @param registry    Registry handle
 * @param device_id   Device ID
 * @param entity_id   Entity ID
 * @param entity_idx  Output: entity index if found
 * @return QYMERA_OK if found, QYMERA_ERR_NOT_FOUND otherwise
 */
qymera_err_t qymera_registry_find_entity(qymera_registry_t *registry, const char *device_id, const char *entity_id, uint16_t *entity_idx);

/**
 * Find entity by entity reference
 * @param registry Registry handle
 * @param ref      Entity reference
 * @param entity_idx Output: entity index if found
 * @return QYMERA_OK if found
 */
qymera_err_t qymera_registry_find_entity_by_ref(qymera_registry_t *registry, const qymera_entity_ref_t *ref, uint16_t *entity_idx);

/**
 * Update device last_seen timestamp
 * @param registry  Registry handle
 * @param device_idx Device index
 * @return QYMERA_OK on success
 */
qymera_err_t qymera_registry_update_seen(qymera_registry_t *registry, uint16_t device_idx);

/**
 * Mark device online/offline
 * @param registry   Registry handle
 * @param device_idx Device index
 * @param online     Online state
 * @return QYMERA_OK on success
 */
qymera_err_t qymera_registry_set_online(qymera_registry_t *registry, uint16_t device_idx, bool online);

/**
 * Update entity value
 * @param registry   Registry handle
 * @param entity_idx Entity index
 * @param value      New value
 * @return QYMERA_OK on success
 */
qymera_err_t qymera_registry_update_entity_value(qymera_registry_t *registry, uint16_t entity_idx, const qymera_entity_value_t *value);

/**
 * Get device by index
 * @param registry   Registry handle
 * @param device_idx Device index
 * @param device     Output device info
 * @return QYMERA_OK on success
 */
qymera_err_t qymera_registry_get_device(qymera_registry_t *registry, uint16_t device_idx, qymera_device_t *device);

/**
 * Get entity by index
 * @param registry   Registry handle
 * @param entity_idx Entity index
 * @param entity     Output entity info
 * @return QYMERA_OK on success
 */
qymera_err_t qymera_registry_get_entity(qymera_registry_t *registry, uint16_t entity_idx, qymera_entity_t *entity);

/**
 * Iterate over all devices
 * @param registry Registry handle
 * @param callback Callback(device_idx, device, context) -> true to continue
 * @param context  User context
 * @return Number of devices visited
 */
typedef bool (*qymera_registry_device_cb_t)(uint16_t idx, const qymera_device_t *device, void *context);
size_t qymera_registry_iterate_devices(qymera_registry_t *registry, qymera_registry_device_cb_t callback, void *context);

/**
 * Iterate over entities of a device
 * @param registry    Registry handle
 * @param device_idx  Device index
 * @param callback    Callback(entity_idx, entity, context) -> true to continue
 * @param context     User context
 * @return Number of entities visited
 */
typedef bool (*qymera_registry_entity_cb_t)(uint16_t idx, const qymera_entity_t *entity, void *context);
size_t qymera_registry_iterate_device_entities(qymera_registry_t *registry, uint16_t device_idx, qymera_registry_entity_cb_t callback, void *context);

/**
 * Get count of registered devices
 * @param registry Registry handle
 * @return Device count
 */
size_t qymera_registry_device_count(qymera_registry_t *registry);

/**
 * Get count of registered entities
 * @param registry Registry handle
 * @return Entity count
 */
size_t qymera_registry_entity_count(qymera_registry_t *registry);

/**
 * Remove a device and its entities
 * @param registry   Registry handle
 * @param device_idx Device index
 * @return QYMERA_OK on success
 */
qymera_err_t qymera_registry_remove_device(qymera_registry_t *registry, uint16_t device_idx);

/* =========================
 * Control API
 * ========================= */

/**
 * Set relay actuator state
 * @param registry   Registry handle
 * @param entity_ref Entity reference (device_id + entity_id)
 * @param state      true = ON, false = OFF
 * @param local_only if true, only act on local devices (not remote)
 * @return QYMERA_OK on success, QYMERA_ERR_NOT_FOUND if entity not found,
 *         QYMERA_ERR_INVALID_CAPABILITY if entity doesn't have relay capability
typedef struct qymera_core_s qymera_core_t;
typedef struct qymera_control_s qymera_control_context_t;
 */
qymera_err_t qymera_control_set_relay(qymera_registry_t *registry, qymera_control_context_t *context, const qymera_entity_ref_t *entity_ref, bool state, bool local_only);

/**
 * Set dimmer actuator level
 * @param registry   Registry handle
 * @param entity_ref Entity reference (device_id + entity_id)
 * @param level      duty cycle 0-100
 * @param local_only if true, only act on local devices (not remote)
 * @return QYMERA_OK on success, QYMERA_ERR_NOT_FOUND if entity not found,
 *         QYMERA_ERR_INVALID_CAPABILITY if entity doesn't have dimmer capability
 */
qymera_err_t qymera_control_set_dimmer(qymera_registry_t *registry, qymera_control_context_t *context, const qymera_entity_ref_t *entity_ref, uint8_t level, bool local_only);

/**
 * Check and update stale devices (called periodically)
 * @param registry       Registry handle
 * @param timeout_ms     Offline timeout in milliseconds
 * @param stale_callback Called for each device that went stale
 * @param context        User context
 * @return Number of devices marked offline
 */
typedef void (*qymera_registry_stale_cb_t)(uint16_t device_idx, const qymera_device_t *device, void *context);
size_t qymera_registry_check_stale(qymera_registry_t *registry, uint32_t timeout_ms, qymera_registry_stale_cb_t stale_callback, void *context);

#ifdef __cplusplus
}
#endif
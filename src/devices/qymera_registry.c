/**
 * Qymera Dashboard - Device Registry Implementation
 */
#include "qymera_registry.h"
#include "qymera_hal.h"
#include <string.h>
#include <stdlib.h>

struct qymera_registry_s {
    qymera_device_t *devices;
    size_t max_devices;
    size_t device_count;
    
    qymera_entity_t *entities;
    size_t max_entities;
    size_t entity_count;
    
    qymera_ring_t event_ring;
    
    uint16_t *free_device_indices;
    size_t free_device_count;
    uint16_t *free_entity_indices;
    size_t free_entity_count;
};

qymera_err_t qymera_registry_init(qymera_registry_t **registry, const qymera_registry_config_t *config) {
    if (!registry || !config) return QYMERA_ERR_INVALID_ARG;
    if (!config->devices || config->max_devices == 0) return QYMERA_ERR_INVALID_ARG;
    if (!config->entities || config->max_entities == 0) return QYMERA_ERR_INVALID_ARG;
    
    qymera_registry_t *reg = calloc(1, sizeof(qymera_registry_t));
    if (!reg) return QYMERA_ERR_NO_SPACE;
    
    reg->devices = config->devices;
    reg->max_devices = config->max_devices;
    reg->device_count = 0;
    
    reg->entities = config->entities;
    reg->max_entities = config->max_entities;
    reg->entity_count = 0;
    
    reg->event_ring = config->event_ring;
    
    reg->free_device_indices = calloc(config->max_devices, sizeof(uint16_t));
    reg->free_entity_indices = calloc(config->max_entities, sizeof(uint16_t));
    
    if (!reg->free_device_indices || !reg->free_entity_indices) {
        free(reg->free_device_indices);
        free(reg->free_entity_indices);
        free(reg);
        return QYMERA_ERR_NO_SPACE;
    }
    
    for (size_t i = 0; i < config->max_devices; i++) {
        reg->free_device_indices[i] = (uint16_t)i;
    }
    reg->free_device_count = config->max_devices;
    
    for (size_t i = 0; i < config->max_entities; i++) {
        reg->free_entity_indices[i] = (uint16_t)i;
    }
    reg->free_entity_count = config->max_entities;
    
    *registry = reg;
    return QYMERA_OK;
}

static qymera_err_t registry_alloc_device(qymera_registry_t *reg, uint16_t *idx) {
    if (reg->free_device_count == 0) return QYMERA_ERR_NO_SPACE;
    *idx = reg->free_device_indices[--reg->free_device_count];
    return QYMERA_OK;
}

static void registry_free_device(qymera_registry_t *reg, uint16_t idx) {
    if (reg->free_device_count < reg->max_devices) {
        reg->free_device_indices[reg->free_device_count++] = idx;
    }
}

static qymera_err_t registry_alloc_entity(qymera_registry_t *reg, uint16_t *idx) {
    if (reg->free_entity_count == 0) return QYMERA_ERR_NO_SPACE;
    *idx = reg->free_entity_indices[--reg->free_entity_count];
    return QYMERA_OK;
}

static void registry_free_entity(qymera_registry_t *reg, uint16_t idx) {
    if (reg->free_entity_count < reg->max_entities) {
        reg->free_entity_indices[reg->free_entity_count++] = idx;
    }
}

qymera_err_t qymera_registry_register_device(qymera_registry_t *registry, const qymera_device_t *device, uint16_t *device_idx) {
    if (!registry || !device || !device_idx) return QYMERA_ERR_INVALID_ARG;
    if (device->device_id[0] == '\0') return QYMERA_ERR_INVALID_ARG;
    
    uint16_t existing_idx;
    if (qymera_registry_find_device(registry, device->device_id, &existing_idx) == QYMERA_OK) {
        return QYMERA_ERR_INVALID_ARG;
    }
    
    uint16_t idx;
    qymera_err_t err = registry_alloc_device(registry, &idx);
    if (err != QYMERA_OK) return err;
    
    qymera_device_t *d = &registry->devices[idx];
    memcpy(d, device, sizeof(qymera_device_t));
    d->entity_count = 0;
    d->registered_at = qymera_timestamp_now();
    d->last_seen = d->registered_at;
    
    registry->device_count++;
    *device_idx = idx;
    return QYMERA_OK;
}

qymera_err_t qymera_registry_register_entity(qymera_registry_t *registry, uint16_t device_idx, const qymera_entity_t *entity, uint16_t *entity_idx) {
    if (!registry || !entity || !entity_idx) return QYMERA_ERR_INVALID_ARG;
    if (device_idx >= registry->max_devices) return QYMERA_ERR_INVALID_ARG;
    if (registry->devices[device_idx].device_id[0] == '\0') return QYMERA_ERR_NOT_FOUND;
    if (entity->entity_id[0] == '\0') return QYMERA_ERR_INVALID_ARG;
    
    uint16_t idx;
    qymera_err_t err = registry_alloc_entity(registry, &idx);
    if (err != QYMERA_OK) return err;
    
    qymera_entity_t *e = &registry->entities[idx];
    memcpy(e, entity, sizeof(qymera_entity_t));
    strncpy(e->device_id, registry->devices[device_idx].device_id, QYMERA_DEVICE_ID_LEN - 1);
    
    qymera_device_t *d = &registry->devices[device_idx];
    if (d->entity_count < QYMERA_ARRAY_SIZE(d->entity_indices)) {
        d->entity_indices[d->entity_count++] = idx;
    }
    
    registry->entity_count++;
    *entity_idx = idx;
    return QYMERA_OK;
}

qymera_err_t qymera_registry_find_device(qymera_registry_t *registry, const char *device_id, uint16_t *device_idx) {
    if (!registry || !device_id || !device_idx) return QYMERA_ERR_INVALID_ARG;
    if (device_id[0] == '\0') return QYMERA_ERR_NOT_FOUND;
    
    for (size_t i = 0; i < registry->max_devices; i++) {
        if (registry->devices[i].device_id[0] != '\0' &&
            strcmp(registry->devices[i].device_id, device_id) == 0) {
            *device_idx = (uint16_t)i;
            return QYMERA_OK;
        }
    }
    return QYMERA_ERR_NOT_FOUND;
}

qymera_err_t qymera_registry_find_entity(qymera_registry_t *registry, const char *device_id, const char *entity_id, uint16_t *entity_idx) {
    if (!registry || !device_id || !entity_id || !entity_idx) return QYMERA_ERR_INVALID_ARG;
    
    uint16_t device_idx;
    qymera_err_t err = qymera_registry_find_device(registry, device_id, &device_idx);
    if (err != QYMERA_OK) return err;
    
    const qymera_device_t *d = &registry->devices[device_idx];
    for (uint8_t i = 0; i < d->entity_count; i++) {
        uint16_t e_idx = d->entity_indices[i];
        if (strcmp(registry->entities[e_idx].entity_id, entity_id) == 0) {
            *entity_idx = e_idx;
            return QYMERA_OK;
        }
    }
    return QYMERA_ERR_NOT_FOUND;
}

qymera_err_t qymera_registry_find_entity_by_ref(qymera_registry_t *registry, const qymera_entity_ref_t *ref, uint16_t *entity_idx) {
    if (!registry || !ref || !entity_idx) return QYMERA_ERR_INVALID_ARG;
    return qymera_registry_find_entity(registry, ref->device_id, ref->entity_id, entity_idx);
}

qymera_err_t qymera_registry_update_seen(qymera_registry_t *registry, uint16_t device_idx) {
    if (!registry || device_idx >= registry->max_devices) return QYMERA_ERR_INVALID_ARG;
    if (registry->devices[device_idx].device_id[0] == '\0') return QYMERA_ERR_NOT_FOUND;
    
    registry->devices[device_idx].last_seen = qymera_timestamp_now();
    return QYMERA_OK;
}

qymera_err_t qymera_registry_set_online(qymera_registry_t *registry, uint16_t device_idx, bool online) {
    if (!registry || device_idx >= registry->max_devices) return QYMERA_ERR_INVALID_ARG;
    if (registry->devices[device_idx].device_id[0] == '\0') return QYMERA_ERR_NOT_FOUND;
    
    qymera_device_t *d = &registry->devices[device_idx];
    bool was_online = d->online;
    d->online = online;
    d->state = online ? 0 : 1;
    d->last_seen = qymera_timestamp_now();
    
    if (was_online != online && registry->event_ring.data) {
    }
    
    return QYMERA_OK;
}

qymera_err_t qymera_registry_update_entity_value(qymera_registry_t *registry, uint16_t entity_idx, const qymera_entity_value_t *value) {
    if (!registry || !value) return QYMERA_ERR_INVALID_ARG;
    if (entity_idx >= registry->max_entities) return QYMERA_ERR_INVALID_ARG;
    if (registry->entities[entity_idx].entity_id[0] == '\0') return QYMERA_ERR_NOT_FOUND;
    
    qymera_entity_t *e = &registry->entities[entity_idx];
    e->value = *value;
    e->last_updated = qymera_timestamp_now();
    
    return QYMERA_OK;
}

qymera_err_t qymera_registry_get_device(qymera_registry_t *registry, uint16_t device_idx, qymera_device_t *device) {
    if (!registry || !device || device_idx >= registry->max_devices) return QYMERA_ERR_INVALID_ARG;
    if (registry->devices[device_idx].device_id[0] == '\0') return QYMERA_ERR_NOT_FOUND;
    
    memcpy(device, &registry->devices[device_idx], sizeof(qymera_device_t));
    return QYMERA_OK;
}

qymera_err_t qymera_registry_get_entity(qymera_registry_t *registry, uint16_t entity_idx, qymera_entity_t *entity) {
    if (!registry || !entity || entity_idx >= registry->max_entities) return QYMERA_ERR_INVALID_ARG;
    if (registry->entities[entity_idx].entity_id[0] == '\0') return QYMERA_ERR_NOT_FOUND;
    
    memcpy(entity, &registry->entities[entity_idx], sizeof(qymera_entity_t));
    return QYMERA_OK;
}

size_t qymera_registry_iterate_devices(qymera_registry_t *registry, qymera_registry_device_cb_t callback, void *context) {
    if (!registry || !callback) return 0;
    
    size_t visited = 0;
    for (size_t i = 0; i < registry->max_devices; i++) {
        if (registry->devices[i].device_id[0] != '\0') {
            if (!callback((uint16_t)i, &registry->devices[i], context)) break;
            visited++;
        }
    }
    return visited;
}

size_t qymera_registry_iterate_device_entities(qymera_registry_t *registry, uint16_t device_idx, qymera_registry_entity_cb_t callback, void *context) {
    if (!registry || !callback || device_idx >= registry->max_devices) return 0;
    if (registry->devices[device_idx].device_id[0] == '\0') return 0;
    
    size_t visited = 0;
    const qymera_device_t *d = &registry->devices[device_idx];
    for (uint8_t i = 0; i < d->entity_count; i++) {
        uint16_t e_idx = d->entity_indices[i];
        if (!callback(e_idx, &registry->entities[e_idx], context)) break;
        visited++;
    }
    return visited;
}

size_t qymera_registry_device_count(qymera_registry_t *registry) {
    return registry ? registry->device_count : 0;
}

size_t qymera_registry_entity_count(qymera_registry_t *registry) {
    return registry ? registry->entity_count : 0;
}

qymera_err_t qymera_registry_remove_device(qymera_registry_t *registry, uint16_t device_idx) {
    if (!registry || device_idx >= registry->max_devices) return QYMERA_ERR_INVALID_ARG;
    if (registry->devices[device_idx].device_id[0] == '\0') return QYMERA_ERR_NOT_FOUND;
    
    qymera_device_t *d = &registry->devices[device_idx];
    
    for (uint8_t i = 0; i < d->entity_count; i++) {
        registry_free_entity(registry, d->entity_indices[i]);
        memset(&registry->entities[d->entity_indices[i]], 0, sizeof(qymera_entity_t));
        registry->entity_count--;
    }
    
    memset(d, 0, sizeof(qymera_device_t));
    registry_free_device(registry, device_idx);
    registry->device_count--;
    
    return QYMERA_OK;
}

/* Control API implementations */

static bool _control_entity_has_capability(qymera_registry_t *registry, const qymera_entity_ref_t *entity_ref, qymera_capability_t cap) {
    uint16_t entity_idx;
    if (qymera_registry_find_entity(registry, entity_ref->device_id, entity_ref->entity_id, &entity_idx) != QYMERA_OK) {
        return false;
    }
    qymera_entity_t entity;

    if (qymera_registry_get_entity(registry, entity_idx, &entity) != QYMERA_OK) {

        return false;

    }

    for (uint8_t i = 0; i < entity.capability_count; i++) {

        if (entity.capabilities[i] == cap) {

            return true;

        }

    }

    return false;

}


qymera_err_t qymera_control_set_relay(qymera_registry_t *registry, const qymera_entity_ref_t *entity_ref, bool state, bool local_only) {
    if (!registry || !entity_ref) return QYMERA_ERR_INVALID_ARG;

    // Check entity has relay capability
    if (!_control_entity_has_capability(registry, entity_ref, QYMERA_CAP_ACTUATOR_RELAY)) {
        return QYMERA_ERR_INVALID_CAPABILITY;
    }

    // Find entity in registry
    uint16_t entity_idx;
    qymera_err_t err = qymera_registry_find_entity(registry, entity_ref->device_id, entity_ref->entity_id, &entity_idx);
    if (err != QYMERA_OK) return err;

    qymera_entity_t *e = &registry->entities[entity_idx];

    // Check device role for locality
    uint16_t dev_idx;
    err = qymera_registry_find_device(registry, entity_ref->device_id, &dev_idx);
    if (err != QYMERA_OK) return err;

    qymera_device_t *d = &registry->devices[dev_idx];
    bool is_remote = (d->role == 1);  // role==1 = remote

    // local_only enforcement: reject remote targets when local_only=true
    if (local_only && is_remote) {
        return QYMERA_ERR_INVALID_CAPABILITY;
    }

    // Perform actual GPIO control for local devices
    if (!is_remote) {
        // Idempotency check: avoid unnecessary GPIO write if already in desired state
        if (e->value.bool_value != state) {
            qymera_err_t gpio_err = qymera_gpio_write(e->gpio_pin,
                        state ? QYMERA_GPIO_HIGH : QYMERA_GPIO_LOW);
            if (gpio_err != QYMERA_OK) {
                return gpio_err;  // GPIO write failed - state not updated
            }
        }
        // else: already in desired state - idempotent no-op, GPIO not re-written
    } else {
        // Remote device: send UDP control command
        // TODO: implement UDP remote control using existing protocol
        (void)entity_ref; (void)state; (void)local_only;
        return QYMERA_ERR_NOT_IMPLEMENTED;
    }

    // Update registry state AFTER successful control
    e->value.valid = true;
    e->value.numeric_value = state ? 1.0f : 0.0f;
    e->value.bool_value = state;
    e->value.timestamp = qymera_timestamp_now();
    e->value.reliability = 0;

    return QYMERA_OK;
}
qymera_err_t qymera_control_set_dimmer(qymera_registry_t *registry, const qymera_entity_ref_t *entity_ref, uint8_t level, bool local_only) {
    if (!registry || !entity_ref) return QYMERA_ERR_INVALID_ARG;

    // Check entity has dimmer capability
    if (!_control_entity_has_capability(registry, entity_ref, QYMERA_CAP_ACTUATOR_DIMMER)) {
        return QYMERA_ERR_INVALID_CAPABILITY;
    }

    // Clamp level to valid range (0-100) - return error for invalid instead of clamping
    if (level > 100) {
        return QYMERA_ERR_INVALID_ARG;
    }

    // Find entity in registry
    uint16_t entity_idx;
    qymera_err_t err = qymera_registry_find_entity(registry, entity_ref->device_id, entity_ref->entity_id, &entity_idx);
    if (err != QYMERA_OK) return err;

    qymera_entity_t *e = &registry->entities[entity_idx];

    // Check device role for locality
    uint16_t dev_idx;
    err = qymera_registry_find_device(registry, entity_ref->device_id, &dev_idx);
    if (err != QYMERA_OK) return err;

    qymera_device_t *d = &registry->devices[dev_idx];
    bool is_remote = (d->role == 1);  // role==1 = remote

    // local_only enforcement: reject remote targets when local_only=true
    if (local_only && is_remote) {
        return QYMERA_ERR_INVALID_CAPABILITY;
    }

    // Perform actual PWM control for local devices
    if (!is_remote) {
        // Idempotency check: avoid unnecessary PWM write if already at desired level
        if (e->value.numeric_value != (float)level) {
            // Set PWM duty cycle - use entity's gpio_pin as PWM channel
            // gpio_pin == -1 means not mapped to PWM
            if (e->gpio_pin < 0) {
                return QYMERA_ERR_INVALID_STATE;  // Pin not mapped to PWM
            }
            qymera_err_t pwm_err = qymera_pwm_set_duty(e->gpio_pin, level);
            if (pwm_err != QYMERA_OK) {
                return pwm_err;  // PWM write failed
            }
        }
        // else: already at desired level - idempotent no-op
    } else {
        // Remote device: send UDP control command
        // TODO: implement UDP remote control using existing protocol
        (void)entity_ref; (void)level; (void)local_only;
        return QYMERA_ERR_NOT_IMPLEMENTED;
    }

    // Update registry state AFTER successful control
    e->value.valid = true;
    e->value.numeric_value = (float)level;
    e->value.bool_value = false;
    e->value.timestamp = qymera_timestamp_now();
    e->value.reliability = 0;

    return QYMERA_OK;
}

size_t qymera_registry_check_stale(qymera_registry_t *registry, uint32_t timeout_ms, qymera_registry_stale_cb_t stale_callback, void *context) {
    if (!registry) return 0;
    
    qymera_timestamp_t now = qymera_timestamp_now();
    size_t marked_offline = 0;
    
    for (size_t i = 0; i < registry->max_devices; i++) {
        qymera_device_t *d = &registry->devices[i];
        if (d->device_id[0] == '\0' || !d->online) continue;
        
        uint32_t elapsed = qymera_timestamp_diff_ms(&now, &d->last_seen);
        if (elapsed >= timeout_ms) {
            d->online = false;
            d->state = 1;
            marked_offline++;
            
            if (stale_callback) {
                stale_callback((uint16_t)i, d, context);
            }
        }
    }
    
    return marked_offline;
}
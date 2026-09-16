/**
 * Qymera Dashboard - Core Runtime Implementation
 */
#include "qymera_core.h"
#include "qymera_hal.h"
#include "qymera_log.h"
#include "qymera_event_bus.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

struct qymera_core_s {
    qymera_core_config_t config;

    qymera_registry_t *registry;
    qymera_event_bus_t *event_bus;
    qymera_log_t *log;
    qymera_node_client_t *node_client;
    qymera_storage_t *storage;
    qymera_rule_engine_t *rule_engine;
    qymera_ai_t *ai;

    qymera_control_context_t control;
    qymera_skill_context_t skill;

    qymera_device_t *devices_storage;
    qymera_entity_t *entities_storage;
    qymera_compiled_rule_t *rules_storage;

    qymera_log_entry_t *log_ring_storage;
    qymera_event_t *event_ring_storage;

    uint32_t last_stale_check;
    uint16_t local_device_idx;
    bool initialized;
};

/* Node reconciliation callbacks -> route authoritative state into the control
 * command machine. The node client owns discovery/ingestion of devices and
 * entities; control owns pending-command resolution only. */
static void node_on_entity_state(void *ctx, const char *device_id, const char *entity_id,
                                 bool available, uint8_t type,
                                 float numeric_value, bool bool_value) {
    qymera_control_on_remote_state((qymera_control_context_t *)ctx, device_id, entity_id,
                                   available, type, numeric_value, bool_value);
}

static void node_on_device_online(void *ctx, const char *device_id, bool online) {
    qymera_control_context_t *control = (qymera_control_context_t *)ctx;
    if (control && control->log) {
        qymera_log_event(control->log, "node", "node %s %s", device_id ? device_id : "?",
                         online ? "online" : "offline");
    }
}

static qymera_err_t core_init_subsystems(qymera_core_t *core) {
    qymera_err_t err = qymera_storage_init(&core->storage);
    if (err != QYMERA_OK) { qymera_log_early("storage_init=%d", err); return err; }

    err = qymera_storage_load_network(core->storage, &core->config.network);
    if (err != QYMERA_OK && err != QYMERA_ERR_NOT_FOUND) { qymera_log_early("load_net=%d", err); return err; }
    qymera_log_early("[NET] core config sta_enabled=%d (persisted/NVS)", (int)core->config.network.sta_enabled);

    err = qymera_storage_load_general(core->storage, &core->config.general);
    if (err != QYMERA_OK && err != QYMERA_ERR_NOT_FOUND) { qymera_log_early("load_gen=%d", err); return err; }

    err = qymera_storage_load_ai(core->storage, &core->config.ai);
    if (err != QYMERA_OK && err != QYMERA_ERR_NOT_FOUND) { qymera_log_early("load_ai=%d", err); return err; }
    qymera_log_early("[AI] mode=%d persisted/NVS", (int)core->config.ai.mode);

    qymera_log_config_t log_cfg = {0};
    log_cfg.log_ring.data = core->log_ring_storage;
    log_cfg.log_ring.capacity = QYMERA_MAX_LOG_ENTRIES;
    log_cfg.log_ring.element_size = sizeof(qymera_log_entry_t);
    log_cfg.log_ring.overwrite = true;
    log_cfg.min_layer = QYMERA_LOG_INFO;
    for (int i = 0; i < 9; i++) log_cfg.layer_enabled[i] = true;
    log_cfg.serial_output = true;

    err = qymera_log_init(&core->log, &log_cfg);
    if (err != QYMERA_OK) return err;

    qymera_log_system(core->log, "core", "Qymera Dashboard starting...");

    qymera_registry_config_t reg_cfg = {0};
    reg_cfg.devices = core->devices_storage;
    reg_cfg.max_devices = QYMERA_MAX_DEVICES;
    reg_cfg.entities = core->entities_storage;
    reg_cfg.max_entities = QYMERA_MAX_ENTITIES;
    reg_cfg.event_ring.data = core->event_ring_storage;
    reg_cfg.event_ring.capacity = QYMERA_MAX_EVENT_QUEUE;
    reg_cfg.event_ring.element_size = sizeof(qymera_event_t);
    reg_cfg.event_ring.overwrite = true;

    err = qymera_registry_init(&core->registry, &reg_cfg);
    if (err != QYMERA_OK) return err;

    qymera_event_bus_config_t ev_cfg = {0};
    ev_cfg.event_ring = reg_cfg.event_ring;
    ev_cfg.subscriptions = calloc(QYMERA_MAX_SUBSCRIPTIONS, sizeof(qymera_subscription_t));
    ev_cfg.max_subscriptions = QYMERA_MAX_SUBSCRIPTIONS;

    err = qymera_event_bus_init(&core->event_bus, &ev_cfg);
    if (err != QYMERA_OK) return err;

    qymera_rule_engine_config_t rule_cfg = {0};
    rule_cfg.rules = core->rules_storage;
    rule_cfg.max_rules = QYMERA_MAX_RULES;
    rule_cfg.event_bus = core->event_bus;
    rule_cfg.log = core->log;
    rule_cfg.registry = core->registry;

    err = qymera_rule_engine_init(&core->rule_engine, &rule_cfg);
    if (err != QYMERA_OK) { qymera_log_early("rule_engine=%d", err); return err; }

    err = qymera_ai_init(&core->ai, &core->config.ai);
    if (err != QYMERA_OK) { qymera_log_early("ai_init=%d", err); return err; }

    qymera_rules_index_t rules_index;
    err = qymera_storage_load_rules_index(core->storage, &rules_index);
    if (err == QYMERA_OK) {
        for (uint32_t i = 0; i < rules_index.count; i++) {
            qymera_compiled_rule_t compiled;
            size_t actual_len;
            if (qymera_storage_load_rule(core->storage, rules_index.rules[i].rule_id,
                                          &compiled, sizeof(qymera_compiled_rule_t), &actual_len) != QYMERA_OK) {
                continue;
            }
            const qymera_compiled_rule_t *to_load = &compiled;
            qymera_compiled_rule_t rebuilt;
            if (actual_len == sizeof(qymera_rule_t)) {
                /* Definition-only persisted form: rebuild the runtime form.
                 * Runtime counters are intentionally not persisted. */
                qymera_rule_engine_compile(core->rule_engine, &compiled.rule, &rebuilt);
                to_load = &rebuilt;
            }
            uint16_t slot;
            qymera_rule_engine_load(core->rule_engine, to_load, &slot);
        }
    }

    qymera_device_t dashboard_dev = {0};
    snprintf(dashboard_dev.device_id, sizeof(dashboard_dev.device_id), "dashboard");
    snprintf(dashboard_dev.name, sizeof(dashboard_dev.name), "Qymera Dashboard");
    dashboard_dev.chip_uid = core->config.general.device_uid;
    dashboard_dev.role = 0;
    dashboard_dev.state = 0;
    dashboard_dev.online = true;
    dashboard_dev.registered_at = qymera_timestamp_now();
    dashboard_dev.last_seen = dashboard_dev.registered_at;

    uint16_t dev_idx = QYMERA_MAX_DEVICES;
    if (qymera_registry_register_device(core->registry, &dashboard_dev, &dev_idx) == QYMERA_OK) {
        core->local_device_idx = dev_idx;
    }

    /* Node client: the single v1 HTTP boundary to remote Nodes. */
    qymera_node_client_config_t nc_cfg = {0};
    nc_cfg.registry = core->registry;
    nc_cfg.log = core->log;
    nc_cfg.storage = core->storage;
    nc_cfg.on_entity_state = node_on_entity_state;
    nc_cfg.on_device_online = node_on_device_online;
    nc_cfg.callback_ctx = &core->control;

    err = qymera_node_client_init(&core->node_client, &nc_cfg);
    if (err != QYMERA_OK) { qymera_log_early("node_client=%d", err); return err; }

    /* Control context: typed reference to the services it needs. */
    qymera_control_context_init(&core->control, core->registry, core->node_client,
                                core->event_bus, core->log);

    /* Skill layer: deterministic, bounded surface over the runtime above. */
    qymera_skill_context_init(&core->skill, core->registry, core->rule_engine,
                              &core->control, core->storage, core->log);

    qymera_log_system(core->log, "core", "Core initialized, device UID: %08X, %u skills",
                      core->config.general.device_uid, (unsigned)qymera_skill_registry_count());

    return QYMERA_OK;
}

qymera_err_t qymera_core_init(qymera_core_t **core, const qymera_core_config_t *config) {
    if (!core) return QYMERA_ERR_INVALID_ARG;

    qymera_err_t err = qymera_system_init();
    if (err != QYMERA_OK) return err;

    qymera_core_t *c = calloc(1, sizeof(qymera_core_t));
    if (!c) return QYMERA_ERR_NO_SPACE;

    c->devices_storage = calloc(QYMERA_MAX_DEVICES, sizeof(qymera_device_t));
    c->entities_storage = calloc(QYMERA_MAX_ENTITIES, sizeof(qymera_entity_t));
    c->rules_storage = calloc(QYMERA_MAX_RULES, sizeof(qymera_compiled_rule_t));
    c->log_ring_storage = calloc(QYMERA_MAX_LOG_ENTRIES, sizeof(qymera_log_entry_t));
    c->event_ring_storage = calloc(QYMERA_MAX_EVENT_QUEUE, sizeof(qymera_event_t));

    if (!c->devices_storage || !c->entities_storage || !c->rules_storage ||
        !c->log_ring_storage || !c->event_ring_storage) {
        return QYMERA_ERR_NO_SPACE;
    }

    if (config) {
        c->config = *config;
    } else {
        c->config.general.device_uid = qymera_system_get_chip_id();
        snprintf(c->config.general.device_name, sizeof(c->config.general.device_name), "Qymera-%08X", c->config.general.device_uid);
        c->config.network.report_interval_ms = 5000;
        qymera_ai_config_defaults(&c->config.ai);
    }

    err = core_init_subsystems(c);
    if (err != QYMERA_OK) {
        qymera_log_early("Core subsystem init failed: %d", err);
        qymera_core_shutdown(c);
        return err;
    }

    c->initialized = true;
    *core = c;
    return QYMERA_OK;
}

qymera_err_t qymera_core_tick(qymera_core_t *core) {
    if (!core || !core->initialized) return QYMERA_ERR_INVALID_STATE;

    uint32_t now_ms = qymera_system_get_uptime_ms();

    qymera_node_client_tick(core->node_client);

    qymera_control_tick(&core->control, now_ms);

    qymera_event_bus_process(core->event_bus);

    qymera_rule_engine_tick(core->rule_engine, now_ms);

    if (now_ms - core->last_stale_check >= 10000) {
        core->last_stale_check = now_ms;
        /* The local dashboard device is inherently online while the firmware
         * runs; refresh its presence so the stale sweep never marks it down. */
        if (core->local_device_idx < QYMERA_MAX_DEVICES) {
            qymera_registry_update_seen(core->registry, core->local_device_idx);
        }
        qymera_registry_check_stale(core->registry, 30000, NULL, NULL);
    }

    if (core->config.network.sta_enabled && !qymera_wifi_is_connected()) {
        qymera_wifi_mode_t mode = qymera_wifi_get_mode();
        if (mode == QYMERA_WIFI_MODE_STA || mode == QYMERA_WIFI_MODE_APSTA) {
            qymera_log_warn(core->log, "core", "WiFi disconnected, attempting reconnect");
            qymera_wifi_sta_config_t sta_cfg = {0};
            strncpy(sta_cfg.ssid, core->config.network.sta_ssid, sizeof(sta_cfg.ssid) - 1);
            strncpy(sta_cfg.password, core->config.network.sta_password, sizeof(sta_cfg.password) - 1);
            strncpy(sta_cfg.hostname, core->config.network.sta_hostname, sizeof(sta_cfg.hostname) - 1);
            qymera_wifi_sta_connect(&sta_cfg);
        }
    }

    return QYMERA_OK;
}

qymera_registry_t *qymera_core_get_registry(qymera_core_t *core) {
    return core ? core->registry : NULL;
}

qymera_event_bus_t *qymera_core_get_event_bus(qymera_core_t *core) {
    return core ? core->event_bus : NULL;
}

qymera_log_t *qymera_core_get_log(qymera_core_t *core) {
    return core ? core->log : NULL;
}

qymera_node_client_t *qymera_core_get_node_client(qymera_core_t *core) {
    return core ? core->node_client : NULL;
}

qymera_storage_t *qymera_core_get_storage(qymera_core_t *core) {
    return core ? core->storage : NULL;
}

const qymera_core_config_t *qymera_core_get_config(const qymera_core_t *core) {
    return core ? &core->config : NULL;
}

qymera_rule_engine_t *qymera_core_get_rule_engine(qymera_core_t *core) {
    return core ? core->rule_engine : NULL;
}

qymera_ai_t *qymera_core_get_ai(qymera_core_t *core) {
    return core ? core->ai : NULL;
}

qymera_control_context_t *qymera_core_get_control(qymera_core_t *core) {
    return core ? &core->control : NULL;
}

qymera_skill_context_t *qymera_core_get_skills(qymera_core_t *core) {
    return core ? &core->skill : NULL;
}

void qymera_core_shutdown(qymera_core_t *core) {
    if (!core) return;

    if (core->log) {
        qymera_log_system(core->log, "core", "Shutting down...");
    }

    free(core->devices_storage);
    free(core->entities_storage);
    free(core->rules_storage);
    free(core->log_ring_storage);
    free(core->event_ring_storage);

    if (core->node_client) qymera_node_client_cleanup(core->node_client);

    if (core->event_bus) {
        qymera_subscription_t *subs = qymera_event_bus_get_subscriptions(core->event_bus);
        if (subs) {
            free(subs);
        }
        free(core->event_bus);
    }
    if (core->registry) free(core->registry);
    if (core->log) free(core->log);
    if (core->storage) free(core->storage);
    if (core->rule_engine) free(core->rule_engine);
    if (core->ai) free(core->ai);

    free(core);
}
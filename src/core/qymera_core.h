/**
 * Qymera Dashboard - Core Runtime
 * Main coordination module
 */
#pragma once

#include "qymera_types.h"
#include "qymera_registry.h"
#include "qymera_event_bus.h"
#include "qymera_log.h"
#include "qymera_udp.h"
#include "qymera_storage.h"
#include "qymera_rule.h"
#include "qymera_ai.h"
#include "qymera_control.h"
#include "qymera_skill.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================
 * Core Configuration
 * ========================= */

typedef struct {
    qymera_network_config_t network;
    qymera_general_config_t general;
    qymera_ai_config_t ai;
} qymera_core_config_t;

typedef struct qymera_core_s qymera_core_t;

qymera_err_t qymera_core_init(qymera_core_t **core, const qymera_core_config_t *config);
qymera_err_t qymera_core_tick(qymera_core_t *core);

qymera_registry_t *qymera_core_get_registry(qymera_core_t *core);
qymera_event_bus_t *qymera_core_get_event_bus(qymera_core_t *core);
qymera_log_t *qymera_core_get_log(qymera_core_t *core);
qymera_udp_transport_t *qymera_core_get_udp(qymera_core_t *core);
qymera_storage_t *qymera_core_get_storage(qymera_core_t *core);
qymera_rule_engine_t *qymera_core_get_rule_engine(qymera_core_t *core);
qymera_ai_t *qymera_core_get_ai(qymera_core_t *core);
qymera_control_context_t *qymera_core_get_control(qymera_core_t *core);
qymera_skill_context_t *qymera_core_get_skills(qymera_core_t *core);

void qymera_core_shutdown(qymera_core_t *core);

#ifdef __cplusplus
}
#endif
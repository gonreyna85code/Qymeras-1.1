#pragma once

/* Forward declaration - registry type defined in qymera_registry.h */
typedef struct qymera_registry_s qymera_registry_t;

/* Control context holds registry and UDP transport */
typedef struct {
    qymera_registry_t *registry;  /* Registry for state lookups */
    void *udp_transport;          /* Opaque UDP transport handle */
} qymera_control_context_t;

/* Initialize context from core (called once at startup) */
qymera_err_t qymera_control_context_init(qymera_control_context_t *context,
                                          qymera_registry_t *registry,
                                          void *udp_transport);

/* Cleanup context (called at shutdown) */
void qymera_control_context_cleanup(qymera_control_context_t *context);

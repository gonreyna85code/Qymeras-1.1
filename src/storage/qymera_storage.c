/**
 * Qymera Dashboard - Storage Implementation
 * NVS-based storage with blob support
 */
#include "qymera_storage.h"
#include "qymera_hal.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

struct qymera_storage_s {
    bool initialized;
};

qymera_err_t qymera_storage_init(qymera_storage_t **storage) {
    if (!storage) return QYMERA_ERR_INVALID_ARG;
    
    qymera_storage_t *s = calloc(1, sizeof(qymera_storage_t));
    if (!s) return QYMERA_ERR_NO_SPACE;
    
    qymera_err_t err = qymera_nvs_init();
    if (err != QYMERA_OK) {
        free(s);
        return err;
    }
    
    s->initialized = true;
    *storage = s;
    return QYMERA_OK;
}

static qymera_err_t storage_put(const char *ns, const char *key, const void *data, size_t len) {
    return qymera_nvs_set_blob(ns, key, data, len);
}

static qymera_err_t storage_get(const char *ns, const char *key, void *data, size_t *len) {
    return qymera_nvs_get_blob(ns, key, data, len);
}

static qymera_err_t storage_erase(const char *ns, const char *key) {
    return qymera_nvs_erase_key(ns, key);
}

qymera_err_t qymera_storage_load_network(qymera_storage_t *storage, qymera_network_config_t *config) {
    if (!storage || !storage->initialized || !config) return QYMERA_ERR_INVALID_ARG;
    size_t len = sizeof(qymera_network_config_t);
    qymera_err_t err = storage_get(QYMERA_NS_CONFIG, QYMERA_KEY_NETWORK, config, &len);
    if (err == QYMERA_OK && len != sizeof(qymera_network_config_t)) {
        return QYMERA_ERR_STORAGE;
    }
    return err;
}

qymera_err_t qymera_storage_save_network(qymera_storage_t *storage, const qymera_network_config_t *config) {
    if (!storage || !storage->initialized || !config) return QYMERA_ERR_INVALID_ARG;
    return storage_put(QYMERA_NS_CONFIG, QYMERA_KEY_NETWORK, config, sizeof(qymera_network_config_t));
}

qymera_err_t qymera_storage_load_general(qymera_storage_t *storage, qymera_general_config_t *config) {
    if (!storage || !storage->initialized || !config) return QYMERA_ERR_INVALID_ARG;
    size_t len = sizeof(qymera_general_config_t);
    qymera_err_t err = storage_get(QYMERA_NS_CONFIG, QYMERA_KEY_GENERAL, config, &len);
    if (err == QYMERA_OK && len != sizeof(qymera_general_config_t)) {
        return QYMERA_ERR_STORAGE;
    }
    return err;
}

qymera_err_t qymera_storage_save_general(qymera_storage_t *storage, const qymera_general_config_t *config) {
    if (!storage || !storage->initialized || !config) return QYMERA_ERR_INVALID_ARG;
    return storage_put(QYMERA_NS_CONFIG, QYMERA_KEY_GENERAL, config, sizeof(qymera_general_config_t));
}

/* NVS stores a single blob per key and a page holds at most ~4032 data
 * bytes, while the rules index (QYMERA_MAX_STORED_RULES entries) can exceed
 * one page. The index is therefore persisted as fixed-width chunks under
 * short keys ("idx_0", "idx_1", ...), concatenated on load. */
#define QYMERA_NVS_MAX_BLOB 3900
#define QYMERA_IDX_CHUNKS ((sizeof(qymera_rules_index_t) + QYMERA_NVS_MAX_BLOB - 1) / QYMERA_NVS_MAX_BLOB)

static void idx_chunk_key(unsigned n, char *key, size_t key_sz) {
    snprintf(key, key_sz, "idx_%u", n);
}

static size_t idx_chunk_size(unsigned n) {
    size_t off = (size_t)n * QYMERA_NVS_MAX_BLOB;
    size_t left = sizeof(qymera_rules_index_t) - off;
    return left > QYMERA_NVS_MAX_BLOB ? QYMERA_NVS_MAX_BLOB : left;
}

qymera_err_t qymera_storage_load_rules_index(qymera_storage_t *storage, qymera_rules_index_t *index) {
    if (!storage || !storage->initialized || !index) return QYMERA_ERR_INVALID_ARG;
    
    uint8_t *dst = (uint8_t *)index;
    for (unsigned c = 0; c < QYMERA_IDX_CHUNKS; c++) {
        char key[16];
        idx_chunk_key(c, key, sizeof(key));
        size_t sz = idx_chunk_size(c);
        qymera_err_t err = storage_get(QYMERA_NS_RULES, key, dst + (size_t)c * QYMERA_NVS_MAX_BLOB, &sz);
        if (c == 0 && err == QYMERA_ERR_NOT_FOUND) {
            memset(index, 0, sizeof(qymera_rules_index_t));
            return QYMERA_OK;
        }
        if (err != QYMERA_OK) return err;
    }
    return QYMERA_OK;
}

qymera_err_t qymera_storage_save_rules_index(qymera_storage_t *storage, const qymera_rules_index_t *index) {
    if (!storage || !storage->initialized || !index) return QYMERA_ERR_INVALID_ARG;
    
    const uint8_t *src = (const uint8_t *)index;
    for (unsigned c = 0; c < QYMERA_IDX_CHUNKS; c++) {
        char key[16];
        idx_chunk_key(c, key, sizeof(key));
        qymera_err_t err = storage_put(QYMERA_NS_RULES, key, src + (size_t)c * QYMERA_NVS_MAX_BLOB,
                                       idx_chunk_size(c));
        if (err != QYMERA_OK) return err;
    }
    /* Drop a possibly-stale legacy single-blob index written by older builds. */
    qymera_err_t legacy = storage_erase(QYMERA_NS_RULES, QYMERA_KEY_RULES);
    if (legacy != QYMERA_OK && legacy != QYMERA_ERR_NOT_FOUND) return legacy;
    return QYMERA_OK;
}

/* NVS keys are capped at 15 bytes while rule IDs may be long, so map the
 * human-readable rule_id to a short deterministic hash key. */
static void rule_key_from_id(const char *rule_id, char *key, size_t key_sz) {
    uint32_t h = 2166136261u;
    for (const char *p = rule_id; *p; p++) {
        h ^= (uint8_t)*p;
        h *= 16777619u;
    }
    snprintf(key, key_sz, "%s%08x", QYMERA_KEY_RULE_PREFIX, (unsigned)h);
}

qymera_err_t qymera_storage_load_rule(qymera_storage_t *storage, const char *rule_id,
                                       void *buffer, size_t buf_len, size_t *actual_len) {
    if (!storage || !storage->initialized || !rule_id || !buffer || !actual_len) return QYMERA_ERR_INVALID_ARG;
    
    char key[64];
    rule_key_from_id(rule_id, key, sizeof(key));
    
    return storage_get(QYMERA_NS_RULES, key, buffer, actual_len);
}

qymera_err_t qymera_storage_save_rule(qymera_storage_t *storage, const char *rule_id,
                                       const void *buffer, size_t len, const qymera_rule_meta_t *meta) {
    if (!storage || !storage->initialized || !rule_id || !buffer || !meta) return QYMERA_ERR_INVALID_ARG;
    
    char key[64];
    rule_key_from_id(rule_id, key, sizeof(key));
    
    qymera_err_t err = storage_put(QYMERA_NS_RULES, key, buffer, len);
    if (err != QYMERA_OK) return err;
    
    // Update index
    qymera_rules_index_t index;
    err = qymera_storage_load_rules_index(storage, &index);
    if (err != QYMERA_OK) return err;
    
    // Find or add rule meta
    bool found = false;
    for (uint32_t i = 0; i < index.count && i < QYMERA_MAX_STORED_RULES; i++) {
        if (strcmp(index.rules[i].rule_id, rule_id) == 0) {
            index.rules[i] = *meta;
            found = true;
            break;
        }
    }
    
    if (!found && index.count < QYMERA_MAX_STORED_RULES) {
        index.rules[index.count++] = *meta;
    }
    
    qymera_err_t r = qymera_storage_save_rules_index(storage, &index);
    return r;
}

qymera_err_t qymera_storage_delete_rule(qymera_storage_t *storage, const char *rule_id) {
    if (!storage || !storage->initialized || !rule_id) return QYMERA_ERR_INVALID_ARG;
    
    char key[64];
    rule_key_from_id(rule_id, key, sizeof(key));
    
    qymera_err_t err = storage_erase(QYMERA_NS_RULES, key);
    if (err != QYMERA_OK && err != QYMERA_ERR_NOT_FOUND) return err;
    
    // Update index
    qymera_rules_index_t index;
    err = qymera_storage_load_rules_index(storage, &index);
    if (err != QYMERA_OK) return err;
    
    for (uint32_t i = 0; i < index.count; i++) {
        if (strcmp(index.rules[i].rule_id, rule_id) == 0) {
            // Remove by shifting
            for (uint32_t j = i; j < index.count - 1; j++) {
                index.rules[j] = index.rules[j + 1];
            }
            index.count--;
            break;
        }
    }
    
    return qymera_storage_save_rules_index(storage, &index);
}

qymera_err_t qymera_storage_load_device_list(qymera_storage_t *storage, char (*device_ids)[QYMERA_DEVICE_ID_LEN],
                                              size_t max_ids, size_t *actual_count) {
    if (!storage || !storage->initialized || !device_ids || !actual_count) return QYMERA_ERR_INVALID_ARG;
    
    // For simplicity, we'll store device IDs as a blob list
    // In a real implementation, this would be more efficient
    size_t len = max_ids * QYMERA_DEVICE_ID_LEN;
    qymera_err_t err = storage_get(QYMERA_NS_DEVICES, QYMERA_KEY_DEVICES, device_ids, &len);
    if (err == QYMERA_ERR_NOT_FOUND) {
        *actual_count = 0;
        return QYMERA_OK;
    }
    if (err != QYMERA_OK) return err;
    
    *actual_count = len / QYMERA_DEVICE_ID_LEN;
    return QYMERA_OK;
}

qymera_err_t qymera_storage_load_device(qymera_storage_t *storage, const char *device_id,
                                         void *buffer, size_t buf_len, size_t *actual_len) {
    if (!storage || !storage->initialized || !device_id || !buffer || !actual_len) return QYMERA_ERR_INVALID_ARG;
    
    char key[64];
    snprintf(key, sizeof(key), "%s%s", QYMERA_KEY_DEVICE_PREFIX, device_id);
    
    return storage_get(QYMERA_NS_DEVICES, key, buffer, actual_len);
}

qymera_err_t qymera_storage_save_device(qymera_storage_t *storage, const char *device_id,
                                         const void *buffer, size_t len) {
    if (!storage || !storage->initialized || !device_id || !buffer) return QYMERA_ERR_INVALID_ARG;
    
    char key[64];
    snprintf(key, sizeof(key), "%s%s", QYMERA_KEY_DEVICE_PREFIX, device_id);
    
    qymera_err_t err = storage_put(QYMERA_NS_DEVICES, key, buffer, len);
    if (err != QYMERA_OK) return err;
    
    // Update device list
    char device_ids[QYMERA_MAX_DEVICES][QYMERA_DEVICE_ID_LEN];
    size_t count = 0;
    qymera_storage_load_device_list(storage, device_ids, QYMERA_MAX_DEVICES, &count);
    
    bool found = false;
    for (size_t i = 0; i < count; i++) {
        if (strcmp(device_ids[i], device_id) == 0) {
            found = true;
            break;
        }
    }
    
    if (!found && count < QYMERA_MAX_DEVICES) {
        strncpy(device_ids[count], device_id, QYMERA_DEVICE_ID_LEN - 1);
        count++;
        return storage_put(QYMERA_NS_DEVICES, QYMERA_KEY_DEVICES, device_ids, count * QYMERA_DEVICE_ID_LEN);
    }
    
    return QYMERA_OK;
}

qymera_err_t qymera_storage_put_blob(qymera_storage_t *storage, const char *namespace_,
                                      const char *key, const void *data, size_t len) {
    if (!storage || !storage->initialized || !namespace_ || !key || !data) return QYMERA_ERR_INVALID_ARG;
    return storage_put(namespace_, key, data, len);
}

qymera_err_t qymera_storage_get_blob(qymera_storage_t *storage, const char *namespace_,
                                      const char *key, void *data, size_t *len) {
    if (!storage || !storage->initialized || !namespace_ || !key || !data || !len) return QYMERA_ERR_INVALID_ARG;
    return storage_get(namespace_, key, data, len);
}

qymera_err_t qymera_storage_erase(qymera_storage_t *storage, const char *namespace_,
                                   const char *key) {
    if (!storage || !storage->initialized || !namespace_ || !key) return QYMERA_ERR_INVALID_ARG;
    return storage_erase(namespace_, key);
}

qymera_err_t qymera_storage_factory_reset(qymera_storage_t *storage, bool keep_credentials) {
    if (!storage || !storage->initialized) return QYMERA_ERR_INVALID_ARG;
    
    // Erase all namespaces except config if keeping credentials
    const char *namespaces[] = {
        QYMERA_NS_DEVICES,
        QYMERA_NS_RULES,
        QYMERA_NS_LOGS,
        QYMERA_NS_TELEMETRY,
        QYMERA_NS_AI,
    };
    
    if (!keep_credentials) {
        namespaces[5] = QYMERA_NS_CONFIG;
    }
    
    for (size_t i = 0; i < (keep_credentials ? 5 : 6); i++) {
        // In NVS, we'd need to erase each key individually
        // For now, just erase the namespace by re-initializing
        // This is a simplified implementation
    }
    
    // Full NVS erase
    return qymera_nvs_erase_key("nvs", "factory_reset");  // Placeholder
}
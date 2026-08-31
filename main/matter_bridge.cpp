/*
  Qymera Link - Matter bridge (qymera-IDF branch)

  Maps Qymera Link's local Link-owned actuators to Matter endpoints using the
  SAME validated actuation primitives as the web API (sensors::setRelay /
  sensors::handleDimmer): no direct GPIO writes, same bounds checks.
  Relays    -> OnOffLight   endpoints (OnOff cluster)
  Dimmers   -> DimmableLight endpoints (OnOff + LevelControl clusters)

  MATTER PAIRING (pinned - see main/idf_component.yml):
    esp_matter : ~1.3.0   (release/v1.3 line; first registry-published line)
    ESP-IDF    : 5.2.1    (ESP-Matter 1.2/1.3 both recommend IDF v5.2.1)
    target     : esp32

  NOTE: compiled ONLY when CONFIG_QYMERA_MATTER_ENABLE=y AND the ESP-Matter
  SDK managed dependency is present (see main/CMakeLists.txt). Written against
  the official release/v1.3 managed_component_light example (node::create with
  attribute + identify callbacks) and esp_matter_core.h. The bridge CANNOT be
  compiled in this workspace (only IDF 5.1.2 via PlatformIO; Matter needs
  IDF 5.2.1). Every esp_matter call below follows the v1.3 example; run the
  Matter-enabled build under IDF 5.2.1 to finalize/verify. Qymera core keeps
  running even if the bridge fails to start.
*/

#include "matter_bridge.h"

#if CONFIG_QYMERA_MATTER_ENABLE

#include "Qymera.h"

#include <esp_log.h>
#include <esp_matter.h>
#include <esp_matter_attribute.h>
#include <esp_matter_cluster.h>
#include <esp_matter_endpoint.h>

#include <app-common/zap-generated/ids/Clusters.h>
#include <app-common/zap-generated/ids/Attributes.h>

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

static const char *TAG = "qym_matter";

// Per-endpoint context. Each endpoint's priv_data points at one entry in
// g_matter_eps (kept alive for the node lifetime), so the attribute callback
// can resolve the endpoint back to its Qymera calibration[] entry.
#define MAX_MATTER_EPS 5
struct matter_ep_ctx {
  uint8_t calib_index;
  uint16_t endpoint_id;
  bool last_on;
  uint8_t last_level;
};
static matter_ep_ctx g_matter_eps[MAX_MATTER_EPS];
static uint8_t g_matter_eps_count = 0;
static bool g_started = false;

// ===================== Matter -> Qymera =====================
// Called by ESP-Matter on every attribute update against a Qymera endpoint.
// Only acts on PRE_UPDATE. Delegates to the exact relay/dimmer primitives the
// web API uses (sensors::setRelay / sensors::handleDimmer), keeping the same
// bounds checks and side effects - no direct GPIO writes.

static esp_err_t app_attribute_update_cb(attribute::callback_type_t type, uint16_t /*endpoint_id*/,
                                         uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val,
                                         void *priv_data) {
  if (type != PRE_UPDATE) return ESP_OK;
  auto *ctx = (matter_ep_ctx *)priv_data;
  if (!ctx) return ESP_OK;
  auto &c = sensors::calibrations[ctx->calib_index];

  if (cluster_id == OnOff::Id && attribute_id == OnOff::Attributes::OnOff::Id) {
    if (val->val.u8 != 0) {
      // ON. Relays flip their validated state; dimmers restore last level
      // (or 100% if none was saved yet). This avoids echoing a stale value.
      if (c.type == sensors::TYPE_RELAY) {
        sensors::setRelay(c.name, true);
      } else {
        int want = (c.value > 0) ? (int)c.value : 100;
        if ((int)c.value == 0) sensors::handleDimmer(c.uid, want);
      }
    } else {
      // OFF. shared primitives: setRelay(false) -> relay off, handleDimmer(0)-> dimmer off.
      if (c.type == sensors::TYPE_RELAY) {
        sensors::setRelay(c.name, false);
      } else {
        sensors::handleDimmer(c.uid, 0);
      }
    }
  } else if (cluster_id == LevelControl::Id && attribute_id == LevelControl::Attributes::CurrentLevel::Id) {
    // LevelControl is 0..254; Qymera dimmers are 0..100%.
    int qv = (int)((val->val.u8 * 100) / 254);
    if (qv != (int)c.value) sensors::handleDimmer(c.uid, qv);
  }
  return ESP_OK;
}

static esp_err_t app_identification_cb(identification::callback_type_t /*type*/, uint16_t /*endpoint_id*/,
                                       uint8_t /*effect_id*/, uint8_t /*effect_variant*/, void * /*priv_data*/) {
  return ESP_OK;
}

// ===================== Startup =====================

static void matter_start() {
  node::config_t node_config;
  node_config.root_node.attributes.node_label = (char *)"Qymera";

  node_t *node = node::create(&node_config, app_attribute_update_cb, app_identification_cb);
  if (!node) {
    ESP_LOGE(TAG, "node::create failed");
    return;
  }

  // One endpoint per Qymera LOCAL relay/dimmer (remote Qymera entities are not
  // bridged and never become Matter endpoints).
  for (uint8_t i = 0; i < sensors::MAX_SENSORS && g_matter_eps_count < MAX_MATTER_EPS; i++) {
    auto &c = sensors::calibrations[i];
    if (!c.local || c.uid == 0) continue;
    if (c.type != sensors::TYPE_RELAY && c.type != sensors::TYPE_DIMMER) continue;

    matter_ep_ctx *ctx = &g_matter_eps[g_matter_eps_count];
    endpoint_t *ep = nullptr;
    if (c.type == sensors::TYPE_RELAY) {
      endpoint::on_off_light::config_t cfg;
      ep = endpoint::on_off_light::create(node, &cfg, ENDPOINT_FLAG_NONE, ctx);
    } else {
      endpoint::dimmable_light::config_t cfg;
      ep = endpoint::dimmable_light::create(node, &cfg, ENDPOINT_FLAG_NONE, ctx);
    }
    if (!ep) continue;

    ctx->calib_index = i;
    ctx->endpoint_id = endpoint::get_id(ep);
    ctx->last_on = c.state;
    ctx->last_level = (uint8_t)(((int)c.value * 254) / 100);
    g_matter_eps_count++;

    ESP_LOGI(TAG, "Matter endpoint %u -> qymera '%s'", ctx->endpoint_id, c.name.c_str());
  }

  esp_err_t err = esp_matter::start();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_matter::start failed: %d", err);
    return;
  }
  g_started = true;
  ESP_LOGI(TAG, "Matter started (%u endpoints)", g_matter_eps_count);
}

// ===================== Qymera -> Matter =====================
// Push local Qymera state into the Matter attribute DB. Echo-guarded: only
// pushes on change, and app_attribute_update_cb only acts on external writes,
// so a Qymera-side change cannot re-trigger itself (no feedback loop).

static void matter_push(uint16_t ep_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t val) {
  if (!g_started) return;
  attribute::update(ep_id, cluster_id, attribute_id, &val);  // v1.3; returns esp_err_t
}

static void matter_sync() {
  if (!g_started) return;
  for (uint8_t i = 0; i < g_matter_eps_count; i++) {
    auto &e = g_matter_eps[i];
    auto &c = sensors::calibrations[e.calib_index];

    if (c.type == sensors::TYPE_DIMMER) {
      uint8_t lvl = (uint8_t)(((int)c.value * 254) / 100);
      if (lvl != e.last_level) {
        e.last_level = lvl;
        matter_push(e.endpoint_id, LevelControl::Id, LevelControl::Attributes::CurrentLevel::Id,
                    esp_matter::uint8(lvl));
      }
      bool on = c.state || c.value > 0;
      if (on != e.last_on) {
        e.last_on = on;
        matter_push(e.endpoint_id, OnOff::Id, OnOff::Attributes::OnOff::Id, esp_matter::bool(on));
      }
    } else {
      if (c.state != e.last_on) {
        e.last_on = c.state;
        matter_push(e.endpoint_id, OnOff::Id, OnOff::Attributes::OnOff::Id, esp_matter::bool(c.state));
      }
    }
  }
}

// ===================== Public API =====================

void matter_bridge_init() {
  g_matter_eps_count = 0;
  g_started = false;
}

void matter_bridge_loop() {
  if (g_started) {
    matter_sync();
    return;
  }
  // Start only once the stack has IP connectivity (Qymera owns provisioning;
  // Matter rides the existing network and does not re-provision WiFi). If
  // Matter fails to start, Qymera keeps operating - the bridge just stays off.
  if (core::is_connected()) {
    matter_start();
  }
}

#endif  // CONFIG_QYMERA_MATTER_ENABLE
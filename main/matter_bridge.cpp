/*
  Matter bridge scaffold (qymera-IDF branch)

  Maps Qymera actuators to Matter endpoints using the SAME validated actuation
  primitives as the web API (sensors::setRelay / sensors::handleDimmer): no
  direct GPIO writes, same bounds checks. Relays -> OnOffLight endpoints,
  dimmers -> DimmableLight endpoints (OnOff + LevelControl clusters).

  IMPORTANT: this file is only compiled when CONFIG_QYMERA_MATTER_ENABLE=y and
  the ESP-Matter SDK dependency is present (main/idf_component.yml). API names
  follow ESP-Matter 1.x; adjust to the exact SDK version you pin.
*/

#include "matter_bridge.h"

#if CONFIG_QYMERA_MATTER_ENABLE

#include "Qymera.h"

#include <esp_matter.h>
#include <esp_matter_attribute.h>
#include <esp_matter_cluster.h>
#include <esp_matter_console.h>
#include <esp_matter_endpoint.h>
#include <esp_matter_node.h>

#include <string.h>

using namespace esp_matter;
using namespace esp_matter::cluster;

static const char *TAG = "qym_matter";

// endpoint_id (16-bit) -> calibration[] index. Qymera uids are 32-bit and are
// not Matter endpoints, so we keep an explicit 1:1 map built at startup.
static struct {
  uint16_t endpoint_id;
  uint8_t calib_index;
  bool is_dimmer;
} g_eps[5];
static uint8_t g_eps_count = 0;
static bool g_started = false;

// ===================== Qymera bridge helpers =====================

static bool find_by_endpoint(uint16_t endpoint_id, uint8_t &calib_index, bool &is_dimmer) {
  for (uint8_t i = 0; i < g_eps_count; i++) {
    if (g_eps[i].endpoint_id == endpoint_id) {
      calib_index = g_eps[i].calib_index;
      is_dimmer = g_eps[i].is_dimmer;
      return true;
    }
  }
  return false;
}

// ===================== ESP-Matter attribute callback =====================

static esp_err_t attr_cb(uint16_t ep,
                         uint32_t cluster_id,
                         uint32_t attribute_id,
                         esp_matter_attr_val_t *val,
                         void * /*priv_data*/) {
  uint8_t idx = 0;
  bool is_dimmer = false;
  if (!find_by_endpoint(ep, idx, is_dimmer)) return ESP_OK;
  auto &c = sensors::calibrations[idx];

  if (cluster_id == on_off::Id && attribute_id == on_off::attribute::on_off::id) {
    // Relay: reuse the same validated path as /toggle. Dimmer: on/off toggle.
    if (is_dimmer) {
      sensors::handleDimmer(c.name, val->val.u8 ? (c.state ? 0 : 100) : 0);
    } else {
      if (val->val.u8 != c.state) sensors::setRelay(c.name, val->val.u8 != 0);
    }
  } else if (cluster_id == level_control::Id &&
             attribute_id == level_control::attribute::current_level::id) {
    // LevelControl: 0..254, Qymera dimmers are 0..100%.
    uint8_t level = val->val.u8;
    sensors::handleDimmer(c.name, (level * 100u) / 254u);
  }
  return ESP_OK;
}

// ===================== Startup =====================

static void matter_start() {
  node::config_t node_config;
  node_config.root_node.attributes.node_label = (char *)"Qymera";

  node_t *node = node::create(&node_config, nullptr);
  if (!node) {
    ESP_LOGE(TAG, "node::create failed");
    return;
  }
  attribute::set_callback(attr_cb);

  // One endpoint per Qymera local relay/dimmer.
  for (uint8_t i = 0; i < sensors::MAX_SENSORS && g_eps_count < 5; i++) {
    auto &c = sensors::calibrations[i];
    if (!c.local || c.uid == 0) continue;
    if (c.type != sensors::TYPE_RELAY && c.type != sensors::TYPE_DIMMER) continue;

    endpoint_t ep_id = 0;
    if (c.type == sensors::TYPE_RELAY) {
      endpoint::on_off_light::config_t cfg;
      ep_id = endpoint::create(node, endpoint::on_off_light::get_id(),
                               endpoint::flag::NONE, &cfg);
    } else {
      endpoint::dimmable_light::config_t cfg;
      ep_id = endpoint::create(node, endpoint::dimmable_light::get_id(),
                               endpoint::flag::NONE, &cfg);
    }
    if (ep_id == 0) continue;

    g_eps[g_eps_count].endpoint_id = ep_id;
    g_eps[g_eps_count].calib_index = i;
    g_eps[g_eps_count].is_dimmer = (c.type == sensors::TYPE_DIMMER);
    g_eps_count++;

    ESP_LOGI(TAG, "Matter endpoint %u -> qymera '%s'", ep_id, c.name.c_str());
  }

  esp_err_t err = esp_matter::start();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_matter::start failed: %d", err);
    return;
  }
  g_started = true;
  ESP_LOGI(TAG, "Matter started (%u endpoints)", g_eps_count);
}

// ===================== Public API =====================

void matter_bridge_init() {
  g_eps_count = 0;
  g_started = false;
}

void matter_bridge_loop() {
  if (g_started) return;
  // Start once WiFi is operational (Qymera owns connectivity; Matter rides the
  // existing network instead of re-provisioning WiFi).
  if (core::is_connected()) {
    matter_start();
  }
}

#endif  // CONFIG_QYMERA_MATTER_ENABLE
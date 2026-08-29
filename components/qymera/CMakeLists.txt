# Qymera reusable ESP-IDF component.
#
# The library sources are shared with the historical `src/` tree (this branch
# transforms that tree in place to native ESP-IDF). Only the platform modules
# change; the deterministic core (sensors/automations/rules) stays intact.
#
# Artifacts deliberately excluded from the component:
#   - main.cpp        : moved to the example app in `main/` (provides app_main)
#   - espnow_p2p.*    : ESP-NOW removed from the IDF port (per branch scope)

set(QYMERA_SRCS
    "src/core.cpp"
    "src/storage.cpp"
    "src/log.cpp"
    "src/sensors.cpp"
    "src/mesh.cpp"
    "src/web.cpp"
    "src/html.cpp"
    "src/automations.cpp"
    "src/ota.cpp"
    "src/hal/qhal.cpp"
    "src/hal/qudp.cpp"
    "src/hal/qhttpserver.cpp"
)

idf_component_register(
    SRCS        ${QYMERA_SRCS}
    INCLUDE_DIRS
                "src"
                "src/hal"
    REQUIRES    nvs_flash esp_wifi esp_netif esp_event esp_http_server
                driver esp_efuse esp_system esp_ota
)
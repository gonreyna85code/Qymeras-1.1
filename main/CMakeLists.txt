# Example application: Qymera on native ESP-IDF (+ optional Matter bridge).
#
# Build the plain Qymera runtime with near-zero external dependencies. The
# Matter bridge is opt-in via CONFIG_QYMERA_MATTER_ENABLE=y and requires the
# ESP-Matter SDK (declared in idf_component.yml); see README-IDF.md.

set(PROJ_REQS qymera)
set(MATTER_SRCS "")

if(CONFIG_QYMERA_MATTER_ENABLE)
    list(APPEND PROJ_REQS esp_matter)
    list(APPEND MATTER_SRCS "matter_bridge.cpp")
endif()

idf_component_register(
    SRCS "main.cpp" ${MATTER_SRCS}
    INCLUDE_DIRS "."
    REQUIRES ${PROJ_REQS}
)
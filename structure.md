# Qymera-IDF Branch Structure

## Repository Overview

-  - Reusable ESP-IDF component
-  - Example application with optional Matter bridge
-  - Source files organized by function
-  - Hardware abstraction layer
-  - IDF configuration defaults
-  - Custom partition table
-  - PlatformIO build configuration
-  - CMake project configuration
-  - Host sanity tests
-  - Documentation

# Architecture

-  → Native ESP-IDF
-  - No ESP8266 support
-  - ESP-NOW removed from runtime
-  - Thin compatibility layer only
-  - Forced UDP transport
-  - Behind CONFIG_QYMERA_MATTER_ENABLE boundary

# Build Status

- PlatformIO: Requires component manifest fixes (systemic IDF 5.1.2 issue)
- idf.py: Authoritative build, not tested in this session
- Code validated: OTA removed, partitions fixed, GUI adapted

# Key Files

-  - Component registration
-  - Core initialization
-  - UDP mesh transport
-  - Web server
-  - HTML GUI
-  - Configuration + compatibility aliases
-  - Hardware abstraction

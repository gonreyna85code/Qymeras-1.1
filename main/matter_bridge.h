#pragma once

// Optional Matter bridge (compiled only when CONFIG_QYMERA_MATTER_ENABLE=y).
// Qymera itself has zero Matter/ESP-Matter dependencies: this glue lives in the
// example app, so the reusable component stays dependency-free.

void matter_bridge_init();
void matter_bridge_loop();
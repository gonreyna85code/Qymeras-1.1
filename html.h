#pragma once
#include <Arduino.h>

namespace html {

// Header y estilos
extern const char HTML_START[] PROGMEM;
extern const char HTML_STYLES[] PROGMEM;

// Body structure
extern const char HTML_BODY_START[] PROGMEM;
extern const char HTML_TABS[] PROGMEM;
extern const char HTML_DEVICES_SECTION[] PROGMEM;
extern const char HTML_AUTOMATIONS_SECTION[] PROGMEM;
extern const char HTML_CONFIG_SECTION[] PROGMEM;
extern const char HTML_WIFI_SECTION[] PROGMEM;
extern const char HTML_BODY_END[] PROGMEM;

// JavaScript - Core
extern const char JS_CORE[] PROGMEM;

// JavaScript - Devices & Calibration
extern const char JS_DEVICES[] PROGMEM;
extern const char JS_CARD_RENDERERS[] PROGMEM;

// JavaScript - Automations Wizard
extern const char JS_WIZARD_CORE[] PROGMEM;
extern const char JS_WIZARD_STEPS[] PROGMEM;
extern const char JS_WIZARD_VALIDATION[] PROGMEM;
extern const char JS_WIZARD_HANDLERS[] PROGMEM;

// JavaScript - Init
extern const char JS_INIT[] PROGMEM;

// HTML End
extern const char HTML_END[] PROGMEM;

}  // namespace html
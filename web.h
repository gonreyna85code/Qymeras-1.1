#pragma once
#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

namespace web {
void handleRoot();
void handleSave();
void handleFactoryReset();
void handleCalibSet();
void handleCalib();
void handleDimmerApi();
void handleToggleApi();
void handleRules();
void handleSetRule();
void handleDeleteRule();
void handleGenSetSave();
void saveCalibration();
void loadCalibration();
void loadGeneralSettings();
void loadCredentials();
void saveCredentials(const String &s, const String &p);
extern const char HTML_PAGE_1[];
extern const char HTML_PAGE_2[];
extern const char HTML_PAGE_3[];
extern const char HTML_PAGE_4[];
}

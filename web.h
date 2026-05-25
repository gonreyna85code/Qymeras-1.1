#pragma once
#include <Arduino.h>
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
}

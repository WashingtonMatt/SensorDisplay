#pragma once
#include "config.h"

// Initializes NimBLE and starts scanning. `cfg` is stored by reference
// (not copied) — the scan callback reads from it live on every
// advertisement, so it will automatically pick up changes made via the
// settings portal without needing to be reinitialized, as long as the
// same AppConfig instance is passed in from main.cpp for the life of
// the program.
void bleScanInit(const AppConfig &cfg);

// Pause/resume scanning — call these around the settings portal being
// active, per the project's radio-contention mitigation (ESP32-C3 has
// one shared 2.4GHz radio for WiFi and BLE).
void bleScanPause();
void bleScanResume();

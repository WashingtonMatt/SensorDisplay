#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Device limits
// ---------------------------------------------------------------------------
static constexpr uint8_t MAX_RUUVITAGS = 4;
static constexpr uint8_t LABEL_MAX_LEN = 16;

// ---------------------------------------------------------------------------
// NVS namespace / keys
// ---------------------------------------------------------------------------
static constexpr const char* NVS_NAMESPACE = "sensordisp";

// Victron
static constexpr const char* NVS_KEY_SHUNT_MAC     = "shunt_mac";
static constexpr const char* NVS_KEY_SHUNT_AESKEY  = "shunt_key";
static constexpr const char* NVS_KEY_MPPT_MAC      = "mppt_mac";
static constexpr const char* NVS_KEY_MPPT_AESKEY   = "mppt_key";

// RuuviTags — stored as a single JSON array blob under one key
// (see storage.h) rather than N scalar keys.
static constexpr const char* NVS_KEY_RUUVI_BLOB    = "ruuvi_cfg";

// Display settings
static constexpr const char* NVS_KEY_ROTATION      = "disp_rot";
static constexpr const char* NVS_KEY_DIMMING       = "disp_dim";
static constexpr const char* NVS_KEY_TIMEOUT_S     = "disp_timeout";

// ---------------------------------------------------------------------------
// Config structs
// ---------------------------------------------------------------------------

struct VictronConfig {
    uint8_t shuntMac[6] = {0};
    bool    shuntConfigured = false;
    uint8_t shuntAesKey[16] = {0};

    uint8_t mpptMac[6] = {0};
    bool    mpptConfigured = false;
    uint8_t mpptAesKey[16] = {0};
};

struct RuuviTagConfig {
    char    label[LABEL_MAX_LEN] = {0};   // e.g. "Outdoor", "Indoor", "Refrigerator", "RuuviTag"
    uint8_t mac[6] = {0};
    float   lowTempF = 32.0f;             // gauge arc: blue below this
    float   hiTempF  = 90.0f;             // gauge arc: red above this
    bool    configured = false;
};

struct DisplayConfig {
    uint8_t rotation = 0;      // 0-3, quarter turns
    uint8_t dimmingPct = 100;  // 0-100
    uint16_t timeoutS = 0;     // 0 = never dim/sleep
};

// Runtime app state — single source of truth, loaded from NVS at boot,
// written back on settings-portal save.
struct AppConfig {
    VictronConfig victron;
    RuuviTagConfig ruuviTags[MAX_RUUVITAGS];
    DisplayConfig display;
};

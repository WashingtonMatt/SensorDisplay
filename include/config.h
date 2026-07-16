#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Device limits
// ---------------------------------------------------------------------------
static constexpr uint8_t MAX_RUUVITAGS = 4;
static constexpr uint8_t LABEL_MAX_LEN = 16;

// ---------------------------------------------------------------------------
// Display / touch pins (ported from previous fork, same physical wiring)
// ---------------------------------------------------------------------------
static constexpr int PIN_LCD_SCLK = 6;
static constexpr int PIN_LCD_MOSI = 7;
static constexpr int PIN_LCD_DC   = 2;
static constexpr int PIN_LCD_CS   = 10;
static constexpr int PIN_LCD_BL   = 3;
static constexpr int PIN_TOUCH_SDA = 4;
static constexpr int PIN_TOUCH_SCL = 5;
static constexpr int PIN_TOUCH_INT = 0;
static constexpr int PIN_TOUCH_RST = 1;
static constexpr uint8_t CST816_ADDR = 0x15;

static constexpr uint8_t  BACKLIGHT_PWM_CHANNEL   = 0;
static constexpr uint16_t BACKLIGHT_PWM_FREQUENCY = 5000;
static constexpr uint8_t  BACKLIGHT_PWM_RESOLUTION = 8;

// ---------------------------------------------------------------------------
// Victron BLE constants (ported)
// ---------------------------------------------------------------------------
static constexpr uint16_t VICTRON_COMPANY_ID              = 0x02E1;
static constexpr uint8_t  VICTRON_PRODUCT_ADVERTISEMENT    = 0x10;
static constexpr uint8_t  VICTRON_SOLAR_CHARGER_RECORD     = 0x01;
static constexpr uint8_t  VICTRON_BATTERY_MONITOR_RECORD   = 0x02;

// ---------------------------------------------------------------------------
// RuuviTag BLE constants (ported)
// ---------------------------------------------------------------------------
static constexpr uint16_t RUUVI_COMPANY_ID   = 0x0499;
static constexpr uint8_t  RUUVI_RAWV2_FORMAT = 0x05;

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

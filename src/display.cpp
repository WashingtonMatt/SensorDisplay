#include "display.h"

static Arduino_DataBus *bus = new Arduino_ESP32SPI(
    PIN_LCD_DC,
    PIN_LCD_CS,
    PIN_LCD_SCLK,
    PIN_LCD_MOSI,
    GFX_NOT_DEFINED);

static Arduino_GFX *output_display = new Arduino_GC9A01(
    bus,
    GFX_NOT_DEFINED,
    2,
    true,
    240,
    240);

// Double buffer — this is the flicker fix. Draw to `gfx`, flush() to push
// to output_display.
Arduino_GFX *gfx = new Arduino_Canvas(240, 240, output_display);

static bool screenAwake = true;
static uint8_t lastDutyPct = 100;

bool displayInit() {
    bool panelOk = output_display->begin(80000000); // 80MHz SPI, matches old fork
    if (!panelOk) {
        Serial.println("[display] panel init failed");
    }

    bool canvasOk = gfx->begin();
    if (!canvasOk) {
        Serial.println("[display] canvas init failed");
    }

    Serial.printf("[display] free heap after canvas init: %u bytes, maxAlloc: %u bytes\n",
                  ESP.getFreeHeap(), ESP.getMaxAllocHeap());

    // Canvas stays at rotation 0 always — see header comment.
    gfx->setRotation(0);

    return panelOk && canvasOk;
}

void displaySetRotation(uint8_t rotationQuarterTurns) {
    output_display->setRotation(rotationQuarterTurns % 4);
}

static uint8_t dutyPctToRaw(uint8_t pct) {
    if (pct > 100) pct = 100;
    // Linear 0-100% -> 0-255 PWM duty. The old fork used 3 fixed steps
    // (45/128/255 for Low/Mid/Max); this is the percent-based equivalent
    // now that DisplayConfig stores a 0-100 dimmingPct instead of a level.
    return static_cast<uint8_t>((static_cast<uint16_t>(pct) * 255) / 100);
}

void backlightInit() {
    ledcSetup(BACKLIGHT_PWM_CHANNEL, BACKLIGHT_PWM_FREQUENCY, BACKLIGHT_PWM_RESOLUTION);
    ledcAttachPin(PIN_LCD_BL, BACKLIGHT_PWM_CHANNEL);
    backlightSetDutyPct(lastDutyPct);
}

void backlightSetDutyPct(uint8_t pct) {
    lastDutyPct = (pct > 100) ? 100 : pct;
    if (screenAwake) {
        ledcWrite(BACKLIGHT_PWM_CHANNEL, dutyPctToRaw(lastDutyPct));
    }
}

void backlightSetAwake(bool awake) {
    if (screenAwake == awake) return;
    screenAwake = awake;
    ledcWrite(BACKLIGHT_PWM_CHANNEL, awake ? dutyPctToRaw(lastDutyPct) : 0);
}

#include "render.h"
#include "display.h"
#include "readings.h"
#include <math.h>

static constexpr int16_t SCREEN_CENTER = 120;
static constexpr int16_t GAUGE_RADIUS = 110;
static constexpr int16_t GAUGE_THICKNESS = 14;

// Gauge sweeps 270 degrees starting at 135 (7:30 position), clockwise,
// like a typical speedometer layout on a round display.
static constexpr float GAUGE_START_DEG = 135.0f;
static constexpr float GAUGE_SWEEP_DEG = 270.0f;

static void drawArc(float startDeg, float endDeg, uint16_t color, int16_t thickness) {
    for (float deg = startDeg; deg <= endDeg; deg += 1.5f) {
        float rad = (deg - 90.0f) * PI / 180.0f;
        float cosA = cosf(rad);
        float sinA = sinf(rad);
        for (int16_t t = 0; t < thickness; t++) {
            int16_t r = GAUGE_RADIUS - t;
            int16_t x = SCREEN_CENTER + static_cast<int16_t>(r * cosA);
            int16_t y = SCREEN_CENTER + static_cast<int16_t>(r * sinA);
            gfx->drawPixel(x, y, color);
        }
    }
}

// Blue (cold) -> green (mid) -> red (hot), matching the low/hi range
// coloring called for in the project spec.
static uint16_t tempToColor(float frac) {
    frac = constrain(frac, 0.0f, 1.0f);
    uint8_t r, g, b;
    if (frac < 0.5f) {
        float t = frac / 0.5f;
        r = 0;
        g = static_cast<uint8_t>(255 * t);
        b = static_cast<uint8_t>(255 * (1 - t));
    } else {
        float t = (frac - 0.5f) / 0.5f;
        r = static_cast<uint8_t>(255 * t);
        g = static_cast<uint8_t>(255 * (1 - t));
        b = 0;
    }
    return gfx->color565(r, g, b);
}

static void drawCenteredText(const String &text, int16_t y, uint8_t textSize, uint16_t color) {
    gfx->setTextSize(textSize);
    gfx->setTextColor(color);
    int16_t x1, y1;
    uint16_t w, h;
    gfx->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    gfx->setCursor(SCREEN_CENTER - w / 2, y);
    gfx->print(text);
}

static bool isStale(uint32_t seenMs) {
    return (seenMs == 0) || (millis() - seenMs > READING_STALE_MS);
}

static void drawNoDataState(bool stale) {
    drawCenteredText("--", 100, 3, gfx->color565(120, 120, 120));
    drawCenteredText(stale ? "No signal" : "Waiting...", 145, 2, gfx->color565(120, 120, 120));
}

static void renderRuuviPage(const RuuviTagConfig &tagCfg, const RuuviReading &reading, uint32_t seenMs) {
    gfx->fillScreen(BLACK);
    drawCenteredText(tagCfg.label, 20, 2, WHITE);

    drawArc(GAUGE_START_DEG, GAUGE_START_DEG + GAUGE_SWEEP_DEG, gfx->color565(40, 40, 40), GAUGE_THICKNESS);

    bool stale = isStale(seenMs);
    if (!stale && reading.valid) {
        float range = tagCfg.hiTempF - tagCfg.lowTempF;
        float frac = (range > 0.0f) ? (reading.temperatureF - tagCfg.lowTempF) / range : 0.5f;
        frac = constrain(frac, 0.0f, 1.0f);
        drawArc(GAUGE_START_DEG, GAUGE_START_DEG + GAUGE_SWEEP_DEG * frac, tempToColor(frac), GAUGE_THICKNESS);

        char tempBuf[16];
        snprintf(tempBuf, sizeof(tempBuf), "%.1f%cF", reading.temperatureF, (char)247);
        drawCenteredText(tempBuf, 100, 3, WHITE);

        char humBuf[16];
        snprintf(humBuf, sizeof(humBuf), "%.0f%% RH", reading.humidityPct);
        drawCenteredText(humBuf, 145, 2, gfx->color565(160, 160, 160));
    } else {
        drawNoDataState(stale);
    }

    char rangeBuf[24];
    snprintf(rangeBuf, sizeof(rangeBuf), "%.0f - %.0f%cF", tagCfg.lowTempF, tagCfg.hiTempF, (char)247);
    drawCenteredText(rangeBuf, 200, 1, gfx->color565(100, 100, 100));

    gfx->flush();
}

static void renderShuntPage(const VictronShuntReading &reading, uint32_t seenMs) {
    gfx->fillScreen(BLACK);
    drawCenteredText("Battery Monitor", 20, 2, WHITE);

    bool stale = isStale(seenMs);
    if (!stale && reading.valid) {
        char voltBuf[16];
        snprintf(voltBuf, sizeof(voltBuf), "%.2fV", reading.batteryVoltageV);
        drawCenteredText(voltBuf, 80, 3, WHITE);

        char curBuf[16];
        snprintf(curBuf, sizeof(curBuf), "%.1fA", reading.currentA);
        uint16_t curColor = (reading.currentA >= 0) ? gfx->color565(80, 220, 80) : gfx->color565(220, 120, 80);
        drawCenteredText(curBuf, 125, 2, curColor);

        char socBuf[16];
        snprintf(socBuf, sizeof(socBuf), "%.0f%% SOC", reading.stateOfChargePct);
        drawCenteredText(socBuf, 160, 2, gfx->color565(160, 160, 160));

        if (reading.remainingMins > 0 && reading.remainingMins < 65535) {
            char remBuf[24];
            int hrs = reading.remainingMins / 60;
            int mins = reading.remainingMins % 60;
            snprintf(remBuf, sizeof(remBuf), "%dh %dm left", hrs, mins);
            drawCenteredText(remBuf, 195, 1, gfx->color565(100, 100, 100));
        }
    } else {
        drawNoDataState(stale);
    }

    gfx->flush();
}

static const char *mpptChargeStateText(uint8_t state) {
    // Victron charge-state byte values (Instant Readout solar charger record).
    switch (state) {
        case 0: return "Off";
        case 2: return "Fault";
        case 3: return "Bulk";
        case 4: return "Absorption";
        case 5: return "Float";
        default: return "Unknown";
    }
}

static void renderMpptPage(const VictronMpptReading &reading, uint32_t seenMs) {
    gfx->fillScreen(BLACK);
    drawCenteredText("Solar Charger", 20, 2, WHITE);

    bool stale = isStale(seenMs);
    if (!stale && reading.valid) {
        char powerBuf[16];
        snprintf(powerBuf, sizeof(powerBuf), "%.0fW", reading.panelPowerW);
        drawCenteredText(powerBuf, 80, 3, WHITE);

        char voltBuf[16];
        snprintf(voltBuf, sizeof(voltBuf), "%.2fV batt", reading.batteryVoltageV);
        drawCenteredText(voltBuf, 130, 2, gfx->color565(160, 160, 160));

        drawCenteredText(mpptChargeStateText(reading.chargeState), 165, 2, gfx->color565(80, 220, 80));
    } else {
        drawNoDataState(stale);
    }

    gfx->flush();
}

void renderPage(const PageEntry &page, const AppConfig &cfg) {
    switch (page.type) {
        case PageType::VICTRON_SHUNT:
            renderShuntPage(latestReadings.shunt, latestReadings.shuntSeenMs);
            break;
        case PageType::VICTRON_MPPT:
            renderMpptPage(latestReadings.mppt, latestReadings.mpptSeenMs);
            break;
        case PageType::RUUVI_TAG:
            renderRuuviPage(cfg.ruuviTags[page.slotIndex],
                             latestReadings.ruuvi[page.slotIndex],
                             latestReadings.ruuviSeenMs[page.slotIndex]);
            break;
    }
}

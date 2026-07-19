#include "render.h"
#include "display.h"
#include "gauge_ui.h"
#include "readings.h"
#include "clock.h"
#include <math.h>

static bool isStale(uint32_t seenMs) {
    return (seenMs == 0) || (millis() - seenMs > READING_STALE_MS);
}

static String noSignalText(uint32_t seenMs) {
    return (seenMs != 0 && isStale(seenMs)) ? "no signal" : "waiting for sensor";
}

// --- RuuviTag page, ported from the previous fork's drawOutdoorPage() -----
// One adaptation from the original: the tiny caption at the top of the
// ring now shows the tag's configured label instead of a hardcoded
// "OUTDOOR TEMP", since this fork supports up to 4 independently
// labeled tags. The two side values are the sensor's actual observed
// low/high since boot (readingsRecordRuuvi() in readings.cpp tracks
// this) -- not the same thing as the tag's *configured* gauge-coloring
// range (RuuviTagConfig::lowTempF/hiTempF/scaleLowF/scaleHighF), which
// only drives the ring's gradient and comfort band. A true rolling-24h
// version of this (vs. since-boot) is a follow-up -- it needs
// timestamped samples with eviction.
static void renderRuuviPage(const RuuviTagConfig &tagCfg, const RuuviReading &reading, uint32_t seenMs,
                             float runningLowF, float runningHighF, bool showGrid) {
    bool haveReading = reading.valid && !isStale(seenMs);

    gfx->fillScreen(COLOR_OUTDOOR_TEAL);

    drawGradientRingAt(GAUGE_CENTER_X, haveReading, reading.temperatureF,
                        tagCfg.scaleLowF, tagCfg.scaleHighF,
                        tagCfg.lowTempF, tagCfg.hiTempF,
                        COLOR_OUTDOOR_TEAL);
    gfx->fillCircle(GAUGE_CENTER_X, GAUGE_CENTER_Y, GAUGE_CLEAR_RADIUS, COLOR_OUTDOOR_TEAL);

    String label = tagCfg.label;
    label.toUpperCase();

    if (haveReading) {
        // Vertical line stops at the horizontal bar (38px, vs. the 72px
        // default) -- the bottom row here is a single centered humidity
        // value, not two side-by-side values like the shunt/MPPT pages'
        // bottom rows, so a vertical divider through it doesn't apply.
        if (showGrid) drawValueGridAt(GAUGE_CENTER_X, COLOR_VALUE_GRID, 38);

        int tempDisplay = static_cast<int>(round(reading.temperatureF));
        drawTinyLabel(label, 120, 42, COLOR_DIM_TEXT);
        drawAaCentered(AA_FONT_LARGE, String(tempDisplay) + "F", 120, 47, WHITE, COLOR_OUTDOOR_TEAL);

        String loText = isnan(runningLowF) ? "--F" : (String(static_cast<int>(round(runningLowF))) + "F");
        String hiText = isnan(runningHighF) ? "--F" : (String(static_cast<int>(round(runningHighF))) + "F");
        drawAaCentered(AA_FONT_SMALL, loText, 78, 104, WHITE, COLOR_OUTDOOR_TEAL);
        drawAaCentered(AA_FONT_SMALL, hiText, 164, 104, WHITE, COLOR_OUTDOOR_TEAL);
        drawTinyLabel("LOW", 78, 130, COLOR_DIM_TEXT);
        drawTinyLabel("HIGH", 164, 130, COLOR_DIM_TEXT);

        String humidityText = String(static_cast<int>(round(reading.humidityPct))) + "%";
        drawAaCentered(AA_FONT_SMALL, humidityText, 120, 155, WHITE, COLOR_OUTDOOR_TEAL);
        drawTinyLabel("HUMIDITY", 120, 181, COLOR_DIM_TEXT);
    } else {
        drawAaCentered(AA_FONT_LARGE, "--F", 120, 50, WHITE, COLOR_OUTDOOR_TEAL);
        drawAaCentered(AA_FONT_SMALL, noSignalText(seenMs), 120, 106, COLOR_DIM_TEXT, COLOR_OUTDOOR_TEAL);
    }

    drawSettingsButton(COLOR_OUTDOOR_TEAL, WHITE);
    gfx->flush();
}

// --- Victron SmartShunt page, ported from the previous fork's drawGaugeValues() ---
// Adapted: the old fork's "power"/"aux" cells showed values this data
// model doesn't carry the same way — power is computed here (V x A)
// rather than decoded separately, and "aux" (a second temperature/
// voltage input some Victron shunts have) isn't decoded at all yet, so
// that slot shows remaining time-to-empty instead, which the old fork
// showed elsewhere on this page anyway.
static void renderShuntPage(const VictronShuntReading &reading, uint32_t seenMs, bool showGrid) {
    bool haveReading = reading.valid && !isStale(seenMs);

    gfx->fillScreen(COLOR_PANEL_BLUE);

    float soc = haveReading ? constrain(reading.stateOfChargePct, 0.0f, 100.0f) : 0.0f;
    drawSegmentedGaugeArcAt(GAUGE_CENTER_X, GAUGE_ARC_SWEEP_DEG * soc / 100.0f, COLOR_RING_TRACK, COLOR_RING_GREEN, COLOR_PANEL_BLUE);
    gfx->fillCircle(GAUGE_CENTER_X, GAUGE_CENTER_Y, GAUGE_CLEAR_RADIUS, COLOR_PANEL_BLUE);

    if (haveReading) {
        if (showGrid) drawValueGridAt(GAUGE_CENTER_X);

        int socDisplay = static_cast<int>(round(reading.stateOfChargePct));
        drawTinyLabel("STATE OF CHARGE", 120, 42, COLOR_DIM_TEXT);
        drawAaCentered(AA_FONT_LARGE, String(socDisplay) + "%", 120, 47, WHITE, COLOR_PANEL_BLUE);

        String voltageText = String(reading.batteryVoltageV, 1) + "V";
        String currentText = String(reading.currentA, 1) + "A";
        drawAaCentered(AA_FONT_SMALL, voltageText, 78, 104, WHITE, COLOR_PANEL_BLUE);
        drawAaCentered(AA_FONT_SMALL, currentText, 164, 104, WHITE, COLOR_PANEL_BLUE);
        drawTinyLabel("BATTERY", 78, 130, COLOR_DIM_TEXT);
        drawTinyLabel("CURRENT", 164, 130, COLOR_DIM_TEXT);

        float powerW = reading.batteryVoltageV * reading.currentA;
        String powerText = String(static_cast<int>(round(powerW))) + "W";
        String remainingText = "--";
        if (reading.remainingMins > 0 && reading.remainingMins < 65535) {
            int hrs = reading.remainingMins / 60;
            int mins = reading.remainingMins % 60;
            remainingText = String(hrs) + "h" + String(mins) + "m";
        }
        drawAaCentered(AA_FONT_SMALL, powerText, 78, 147, WHITE, COLOR_PANEL_BLUE);
        drawAaCentered(AA_FONT_SMALL, remainingText, 164, 147, WHITE, COLOR_PANEL_BLUE);
        drawTinyLabel("POWER", 78, 173, COLOR_DIM_TEXT);
        drawTinyLabel("REMAINING", 164, 173, COLOR_DIM_TEXT);
    } else {
        drawAaCentered(AA_FONT_LARGE, "--%", 120, 50, WHITE, COLOR_PANEL_BLUE);
        drawAaCentered(AA_FONT_SMALL, noSignalText(seenMs), 120, 106, COLOR_DIM_TEXT, COLOR_PANEL_BLUE);
    }

    drawSettingsButton(COLOR_PANEL_BLUE, WHITE);
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

// --- Victron MPPT page, ported from the previous fork's drawPageTwo() -----
static void renderMpptPage(const VictronMpptReading &reading, uint32_t seenMs, bool showGrid, float capacityW) {
    bool haveReading = reading.valid && !isStale(seenMs);

    gfx->fillScreen(COLOR_SOLAR_PANEL);

    float ringMaxW = max(capacityW, 1.0f); // guard against a zero/negative config value
    float ringPower = (haveReading && !isnan(reading.panelPowerW))
                           ? constrain(reading.panelPowerW, 0.0f, ringMaxW)
                           : 0.0f;
    drawSegmentedGaugeArcAt(GAUGE_CENTER_X, GAUGE_ARC_SWEEP_DEG * ringPower / ringMaxW,
                             COLOR_SOLAR_TRACK, COLOR_SOLAR_ORANGE, COLOR_SOLAR_PANEL);
    gfx->fillCircle(GAUGE_CENTER_X, GAUGE_CENTER_Y, GAUGE_CLEAR_RADIUS, COLOR_SOLAR_PANEL);

    if (haveReading) {
        // Vertical line stops at the horizontal bar (38px, vs. the 72px
        // default) -- the bottom row here is a single centered yield
        // value, not a side-by-side pair like the middle row, so a
        // vertical divider running through it doesn't apply. See the
        // matching comment/fix on the RuuviTag page above.
        if (showGrid) drawValueGridAt(GAUGE_CENTER_X, COLOR_VALUE_GRID, 38);

        String powerText = isnan(reading.panelPowerW) ? "-- W" : (String(static_cast<int>(round(reading.panelPowerW))) + "W");
        drawTinyLabel("PV POWER", 120, 42, COLOR_SOLAR_DIM_TEXT);
        drawAaCentered(AA_FONT_LARGE, powerText, 120, 43, COLOR_SOLAR_TEXT, COLOR_SOLAR_PANEL);

        String voltText = isnan(reading.batteryVoltageV) ? "--V" : (String(reading.batteryVoltageV, 1) + "V");
        String currentText = isnan(reading.batteryCurrentA) ? "--A" : (String(reading.batteryCurrentA, 1) + "A");
        drawAaCentered(AA_FONT_SMALL, voltText, 78, 112, COLOR_SOLAR_TEXT, COLOR_SOLAR_PANEL);
        drawAaCentered(AA_FONT_SMALL, currentText, 164, 112, COLOR_SOLAR_TEXT, COLOR_SOLAR_PANEL);
        drawTinyLabel("BATTERY", 78, 139, COLOR_SOLAR_DIM_TEXT);
        drawTinyLabel("CHARGE", 164, 139, COLOR_SOLAR_DIM_TEXT);

        String yieldText = isnan(reading.yieldTodayKwh) ? "--kWh" : (String(reading.yieldTodayKwh, 2) + "kWh");
        drawAaCentered(AA_FONT_SMALL, yieldText, 120, 148, COLOR_SOLAR_TEXT, COLOR_SOLAR_PANEL);
        drawTinyLabel("TODAY", 120, 175, COLOR_SOLAR_DIM_TEXT);
        drawAaCentered(AA_FONT_SMALL, mpptChargeStateText(reading.chargeState), 120, 184, COLOR_SOLAR_DIM_TEXT, COLOR_SOLAR_PANEL);
    } else {
        drawAaCentered(AA_FONT_LARGE, "-- W", 120, 50, COLOR_SOLAR_TEXT, COLOR_SOLAR_PANEL);
        drawAaCentered(AA_FONT_SMALL, noSignalText(seenMs), 120, 106, COLOR_SOLAR_DIM_TEXT, COLOR_SOLAR_PANEL);
    }

    drawSettingsButton(COLOR_SOLAR_PANEL, COLOR_SOLAR_TEXT);
    gfx->flush();
}

// AA_FONT_LARGE/AA_FONT_SMALL have no colon glyph (checked the font
// data -- only space, %, +, -, ., digits, and uppercase letters are
// defined), so drawAaText() was silently skipping it, running the
// digits together as "1200" with no separator. This draws hour and
// minute as separate AA strings with a manually-drawn colon (two small
// filled circles) between them.
//
// AM/PM placement: the font's digit glyphs have yOffset=17 and h=37, so
// for a given cursor y, the actual visible ink spans roughly
// [y+17, y+17+37] -- NOT [y, y+~41] as a naive reading of "height 41"
// would suggest. This bottom-aligns AM/PM with that ink span (cursor y
// = ink bottom minus the small font's ~8px height), to the right of the
// clock, like a typical digital clock's AM/PM indicator.
static void drawPlaceholderClock(int16_t centerX, int16_t topY, uint16_t color, uint16_t bgColor,
                                  const char *hourStr, const char *minuteStr, const char *ampm) {
    int16_t hourWidth = aaTextWidth(AA_FONT_LARGE, hourStr);
    int16_t minuteWidth = aaTextWidth(AA_FONT_LARGE, minuteStr);
    constexpr int16_t COLON_GAP = 14;
    int16_t totalWidth = hourWidth + COLON_GAP + minuteWidth;
    int16_t startX = centerX - totalWidth / 2;

    drawAaText(AA_FONT_LARGE, hourStr, startX, topY, color, bgColor);

    int16_t colonX = startX + hourWidth + COLON_GAP / 2;
    gfx->fillCircle(colonX, topY + 28, 3, color); // straddles the digits' vertical ink center (~topY+75.5 in absolute
    gfx->fillCircle(colonX, topY + 43, 3, color); // terms, i.e. topY+17 to topY+17+37)

    drawAaText(AA_FONT_LARGE, minuteStr, startX + hourWidth + COLON_GAP, topY, color, bgColor);

    gfx->setTextSize(1);
    gfx->setTextColor(color);
    gfx->setCursor(startX + totalWidth + 6, topY + 46);
    gfx->print(ampm);
}


// Standalone utility page, not tied to any single device -- see pages.h.
// Its actual sleep-after-timeout behavior lives in main.cpp
// (DisplayConfig::nightModeTimeoutS, independent of the general
// DisplayConfig::timeoutS); this function just draws the page.
//
// Styled to match the other pages (ring gauge, value grid, settings
// button) but in a glow-red night-vision palette on black. The ring has
// no real metric behind it here (nothing single-valued to represent
// across four unrelated readings), so it's drawn fully lit as a
// thematic frame rather than a proportional gauge.
//
// The center clock comes from clock.h -- an in-RAM minutes-since-midnight
// anchor, since this board has no RTC. It's set either by the settings
// portal auto-pushing the phone's local time on page load, or by the
// on-device Hour/Minute/AM-PM rows (3rd settings-menu page). Before
// anything has set it, shows "--:--" rather than a fake time. Split into
// hour/minute (large font) + AM/PM (small, below) rather than one long
// string, since the large AA font was sized for short strings like "72%"
// and a full clock string would run wider than comfortably fits the ring.
static void renderSleepPage(const AppConfig &cfg, bool showGrid) {
    gfx->fillScreen(COLOR_BLACK_SOFT);

    drawSegmentedGaugeArcAt(GAUGE_CENTER_X, GAUGE_ARC_SWEEP_DEG, COLOR_NIGHT_RED_DIM, COLOR_NIGHT_RED, COLOR_BLACK_SOFT);
    gfx->fillCircle(GAUGE_CENTER_X, GAUGE_CENTER_Y, GAUGE_CLEAR_RADIUS, COLOR_BLACK_SOFT);

    if (showGrid) drawValueGridAt(GAUGE_CENTER_X, COLOR_NIGHT_RED_DIM);

    drawTinyLabel("NIGHT MODE", 120, 30, COLOR_NIGHT_RED_DIM);
    char hourStr[3], minuteStr[3], ampmStr[3];
    clockFormat12Hour(hourStr, sizeof(hourStr), minuteStr, sizeof(minuteStr), ampmStr, sizeof(ampmStr));
    drawPlaceholderClock(120, 40, COLOR_NIGHT_RED, COLOR_BLACK_SOFT, hourStr, minuteStr, ampmStr);

    // Row 2: Shunt battery % (left), Solar watts (right)
    bool haveShunt = latestReadings.shunt.valid && !isStale(latestReadings.shuntSeenMs);
    String socText = haveShunt ? (String(static_cast<int>(round(latestReadings.shunt.stateOfChargePct))) + "%") : "--";
    drawAaCentered(AA_FONT_SMALL, socText, 78, 104, COLOR_NIGHT_RED, COLOR_BLACK_SOFT);
    drawTinyLabel("BATTERY", 78, 130, COLOR_NIGHT_RED_DIM);

    bool haveMppt = latestReadings.mppt.valid && !isStale(latestReadings.mpptSeenMs) && !isnan(latestReadings.mppt.panelPowerW);
    String wattsText = haveMppt ? (String(static_cast<int>(round(latestReadings.mppt.panelPowerW))) + "W") : "--";
    drawAaCentered(AA_FONT_SMALL, wattsText, 164, 104, COLOR_NIGHT_RED, COLOR_BLACK_SOFT);
    drawTinyLabel("SOLAR", 164, 130, COLOR_NIGHT_RED_DIM);

    // Row 3: RuuviTag slot 0 temp (left), slot 1 temp (right) -- "RuuviTag
    // 1"/"2" as requested map to config slots 0/1, not any particular
    // label; whichever tags are in those two slots show up here.
    bool haveTag0 = cfg.ruuviTags[0].configured && latestReadings.ruuvi[0].valid && !isStale(latestReadings.ruuviSeenMs[0]);
    String tag0Text = haveTag0 ? (String(static_cast<int>(round(latestReadings.ruuvi[0].temperatureF))) + "F") : "--";
    drawAaCentered(AA_FONT_SMALL, tag0Text, 78, 147, COLOR_NIGHT_RED, COLOR_BLACK_SOFT);
    String tag0Label = cfg.ruuviTags[0].configured ? String(cfg.ruuviTags[0].label) : String("Ruuvi 1");
    tag0Label.toUpperCase();
    drawTinyLabel(tag0Label, 78, 173, COLOR_NIGHT_RED_DIM);

    bool haveTag1 = cfg.ruuviTags[1].configured && latestReadings.ruuvi[1].valid && !isStale(latestReadings.ruuviSeenMs[1]);
    String tag1Text = haveTag1 ? (String(static_cast<int>(round(latestReadings.ruuvi[1].temperatureF))) + "F") : "--";
    drawAaCentered(AA_FONT_SMALL, tag1Text, 164, 147, COLOR_NIGHT_RED, COLOR_BLACK_SOFT);
    String tag1Label = cfg.ruuviTags[1].configured ? String(cfg.ruuviTags[1].label) : String("Ruuvi 2");
    tag1Label.toUpperCase();
    drawTinyLabel(tag1Label, 164, 173, COLOR_NIGHT_RED_DIM);

    drawSettingsButton(COLOR_BLACK_SOFT, COLOR_NIGHT_RED);
    gfx->flush();
}

void renderPage(const PageEntry &page, const AppConfig &cfg) {
    bool showGrid = cfg.display.showValueGrid;
    switch (page.type) {
        case PageType::VICTRON_SHUNT:
            renderShuntPage(latestReadings.shunt, latestReadings.shuntSeenMs, showGrid);
            break;
        case PageType::VICTRON_MPPT:
            renderMpptPage(latestReadings.mppt, latestReadings.mpptSeenMs, showGrid, cfg.victron.solarCapacityW);
            break;
        case PageType::RUUVI_TAG:
            renderRuuviPage(cfg.ruuviTags[page.slotIndex],
                             latestReadings.ruuvi[page.slotIndex],
                             latestReadings.ruuviSeenMs[page.slotIndex],
                             latestReadings.ruuviRunningLowF[page.slotIndex],
                             latestReadings.ruuviRunningHighF[page.slotIndex],
                             showGrid);
            break;
        case PageType::SLEEP_SCREEN:
            renderSleepPage(cfg, showGrid);
            break;
    }
}

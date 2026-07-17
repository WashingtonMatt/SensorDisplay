#include <Arduino.h>
#include <string.h>
#include "config.h"
#include "storage.h"
#include "pages.h"
#include "watchdog.h"
#include "settings_portal.h"
#include "display.h"
#include "gauge_ui.h"
#include "touch.h"
#include "ble_scan.h"
#include "render.h"
#include "readings.h"

static AppConfig appConfig;
static PageEntry activePages[3 + MAX_RUUVITAGS]; // 2 Victron + up to 4 RuuviTag + 1 always-present SLEEP_SCREEN
static uint8_t activePageCount = 0;
static uint8_t currentPageIndex = 0;

// Three-way screen state: browsing sensor pages, the on-device settings
// menu (grid/rotation toggles + a row to launch the web portal), or the
// web portal itself (tracked separately via settingsPortalIsActive()).
enum class UiMode { PAGE_BROWSE, SETTINGS_MENU };
static UiMode uiMode = UiMode::PAGE_BROWSE;

// --- Backlight sleep timeout --------------------------------------------
// DisplayConfig::timeoutS (0 = never) drives this: after that many
// seconds with no touch activity, the backlight turns off (screen keeps
// rendering underneath -- just dark) via backlightSetAwake(false). The
// touch that wakes it back up is swallowed rather than acted on as a
// swipe/tap, so picking the device back up doesn't also flip a page or
// open a menu you didn't mean to touch.
static bool screenSleeping = false;
static uint32_t lastTouchActivityMs = 0;

static void wakeScreen() {
    if (screenSleeping) {
        screenSleeping = false;
        backlightSetAwake(true);
    }
    lastTouchActivityMs = millis();
}

static void checkSleepTimeout() {
    if (screenSleeping) return;

    // The Night Mode page (SLEEP_SCREEN) uses its own timeout
    // (DisplayConfig::nightModeTimeoutS), independent of the general
    // DisplayConfig::timeoutS used on every other page -- that's the
    // whole point of having a separate setting, so Night Mode can be
    // left on indefinitely (0) even while normal pages still time out,
    // or vice versa.
    bool onSleepPage = activePageCount > 0 && currentPageIndex < activePageCount &&
                        activePages[currentPageIndex].type == PageType::SLEEP_SCREEN;
    uint32_t timeoutS = onSleepPage ? appConfig.display.nightModeTimeoutS : appConfig.display.timeoutS;
    if (timeoutS == 0) return;

    uint32_t timeoutMs = timeoutS * 1000UL;
    if (millis() - lastTouchActivityMs > timeoutMs) {
        screenSleeping = true;
        backlightSetAwake(false);
    }
}

// Rebuild page list after any config change (boot, or a save from either
// the on-device menu or the web portal).
static void refreshPages() {
    activePageCount = buildPageList(appConfig, activePages, sizeof(activePages) / sizeof(activePages[0]));
    if (currentPageIndex >= activePageCount) {
        currentPageIndex = 0;
    }
}

static void drawNoDevicesScreen() {
    gfx->fillScreen(BLACK);
    gfx->setTextColor(WHITE);
    gfx->setTextSize(1);
    gfx->setCursor(30, 110);
    gfx->print("No devices configured");
    drawSettingsButton(BLACK, WHITE);
    gfx->flush();
}

// Redraws whatever should currently be on screen for PAGE_BROWSE mode --
// either the current sensor page, or the "nothing configured yet"
// screen. Not used for SETTINGS_MENU or portal screens, which draw
// themselves.
static void redrawCurrentScreen() {
    if (activePageCount > 0) {
        renderPage(activePages[currentPageIndex], appConfig);
    } else {
        drawNoDevicesScreen();
    }
}

// Applies the correct backlight duty for whatever page is currently
// shown: the Night Mode page's own brightness (DisplayConfig::
// nightModeDimmingPct) if it's set to something other than 0, otherwise
// falls back to the whole-device brightness (dimmingPct) -- same "0 =
// use the other setting" pattern as elsewhere. Call this any time
// currentPageIndex changes (a swipe) or display settings are saved, not
// just once at boot.
static void applyBacklightForCurrentPage() {
    bool onSleepPage = activePageCount > 0 && currentPageIndex < activePageCount &&
                        activePages[currentPageIndex].type == PageType::SLEEP_SCREEN;
    uint8_t duty = appConfig.display.dimmingPct;
    if (onSleepPage && appConfig.display.nightModeDimmingPct != 0) {
        duty = appConfig.display.nightModeDimmingPct;
    }
    backlightSetDutyPct(duty);
}

// Registered with settingsPortalSetOnSave() -- called after any
// web-portal save completes, so the page list and live display settings
// stay in sync with what was just saved. The on-device menu calls this
// directly too (see handleSettingsMenuTap()) for the same reason.
static void applyConfigChange() {
    refreshPages();
    displaySetRotation(appConfig.display.rotation);
    touchSetRotation(appConfig.display.rotation);
    applyBacklightForCurrentPage();
}

static bool tapHitSettingsButton(const TouchEvent &event) {
    if (!event.tapped) return false;
    int32_t dx = event.tapX - SETTINGS_BUTTON_X;
    int32_t dy = event.tapY - SETTINGS_BUTTON_Y;
    int32_t r = SETTINGS_BUTTON_R + SETTINGS_BUTTON_TAP_MARGIN;
    return (dx * dx + dy * dy) <= (r * r);
}

// --- On-device settings menu -------------------------------------------
// Ported concept from the old fork's on-device settings page (rows you
// tap to toggle/cycle a value directly on the screen, no phone needed)
// plus one row that launches the full web portal for everything else
// (RuuviTag/Victron pairing). Split across two swipeable pages since a
// single page got too crowded -- swipe left/right while in the menu to
// switch, same gesture used everywhere else in the app. Page 1 groups
// display-appearance settings (4 rows); page 2 groups timing/network (3
// rows, using the first three row positions -- the 4th is simply unused
// there). Grouped this way rather than an arbitrary split so each page
// has a coherent theme.
static constexpr int16_t MENU_ROW_X = 40;
static constexpr int16_t MENU_ROW_W = 160;
static constexpr int16_t MENU_ROW_H = 24;
static constexpr int16_t MENU_ROW_1_Y = 40;
static constexpr int16_t MENU_ROW_2_Y = 74;
static constexpr int16_t MENU_ROW_3_Y = 108;
static constexpr int16_t MENU_ROW_4_Y = 142;
static constexpr uint8_t SETTINGS_MENU_PAGE_COUNT = 2;

static uint8_t settingsMenuPage = 0; // 0: Grid/Rotation/Bright/Night Bright, 1: Sleep/Night/WiFi Setup

static const uint16_t TIMEOUT_OPTIONS[] = {0, 10, 30, 60};
static constexpr uint8_t TIMEOUT_OPTIONS_COUNT = 4;

static const char *timeoutLabel(uint16_t timeoutS) {
    switch (timeoutS) {
        case 10: return "10s";
        case 30: return "30s";
        case 60: return "60s";
        default: return "Off";
    }
}

// Cycles a field to its next value from a fixed option list, wrapping
// around. If the field's current value isn't in the list (e.g. set to
// something else via the portal), this just starts the cycle over from
// the beginning rather than erroring. Shared by the timeout rows
// (uint16_t options) and the brightness rows (uint8_t options).
template <typename T>
static void cycleOption(T &field, const T options[], uint8_t count) {
    uint8_t idx = 0;
    for (uint8_t i = 0; i < count; i++) {
        if (options[i] == field) {
            idx = i;
            break;
        }
    }
    idx = (idx + 1) % count;
    field = options[idx];
}

// Whole-device brightness: no 0 here, since 0% would just mean an
// always-black screen -- that's what the sleep timeout is for.
static const uint8_t BRIGHTNESS_OPTIONS[] = {10, 25, 50, 75, 100};
static constexpr uint8_t BRIGHTNESS_OPTIONS_COUNT = 5;

// Night Mode page brightness: 0 here means "Auto" -- fall back to the
// whole-device brightness instead of overriding it. See
// applyBacklightForCurrentPage().
static const uint8_t NIGHT_BRIGHTNESS_OPTIONS[] = {0, 10, 25, 50, 75, 100};
static constexpr uint8_t NIGHT_BRIGHTNESS_OPTIONS_COUNT = 6;

static String brightnessLabel(uint8_t pct) {
    return String(pct) + "%";
}

static String nightBrightnessLabel(uint8_t pct) {
    return (pct == 0) ? String("Auto") : (String(pct) + "%");
}

static const char *rotationLabel(uint8_t rotation) {
    switch (rotation % 4) {
        case 1: return "90";
        case 2: return "180";
        case 3: return "270";
        default: return "0";
    }
}

static void drawMenuRow(int16_t y, const String &label, const String &value) {
    gfx->drawRect(MENU_ROW_X, y, MENU_ROW_W, MENU_ROW_H, COLOR_VALUE_GRID);
    gfx->setTextSize(1);
    gfx->setTextColor(WHITE);
    gfx->setCursor(MENU_ROW_X + 8, y + 8);
    gfx->print(label);
    int16_t valueWidth = value.length() * 6;
    gfx->setCursor(MENU_ROW_X + MENU_ROW_W - 8 - valueWidth, y + 8);
    gfx->print(value);
}

// Small dot row showing which of the two settings pages is active.
static void drawMenuPageDots() {
    constexpr int16_t DOT_SPACING = 14;
    constexpr int16_t DOT_Y = 176;
    int16_t startX = 120 - (SETTINGS_MENU_PAGE_COUNT - 1) * DOT_SPACING / 2;
    for (uint8_t i = 0; i < SETTINGS_MENU_PAGE_COUNT; i++) {
        int16_t x = startX + i * DOT_SPACING;
        if (i == settingsMenuPage) {
            gfx->fillCircle(x, DOT_Y, 3, WHITE);
        } else {
            gfx->drawCircle(x, DOT_Y, 3, COLOR_DIM_TEXT);
        }
    }
}

static void drawSettingsMenu() {
    gfx->fillScreen(COLOR_PANEL_BLUE);
    gfx->setTextColor(WHITE);
    gfx->setTextSize(2);
    gfx->setCursor(56, 6);
    gfx->print("Settings");

    if (settingsMenuPage == 0) {
        drawMenuRow(MENU_ROW_1_Y, "Grid", appConfig.display.showValueGrid ? "On" : "Off");
        drawMenuRow(MENU_ROW_2_Y, "Rotation", String(rotationLabel(appConfig.display.rotation)) + " deg");
        drawMenuRow(MENU_ROW_3_Y, "Bright", brightnessLabel(appConfig.display.dimmingPct));
        drawMenuRow(MENU_ROW_4_Y, "Night Bright", nightBrightnessLabel(appConfig.display.nightModeDimmingPct));
    } else {
        drawMenuRow(MENU_ROW_1_Y, "Sleep", timeoutLabel(appConfig.display.timeoutS));
        drawMenuRow(MENU_ROW_2_Y, "Night", timeoutLabel(appConfig.display.nightModeTimeoutS));
        drawMenuRow(MENU_ROW_3_Y, "WiFi Setup", "Start >");
    }

    drawMenuPageDots();
    drawSettingsButton(COLOR_PANEL_BLUE, WHITE);
    gfx->flush();
}

static bool tapInRow(const TouchEvent &event, int16_t rowY) {
    if (!event.tapped) return false;
    return event.tapX >= MENU_ROW_X && event.tapX <= MENU_ROW_X + MENU_ROW_W &&
           event.tapY >= rowY && event.tapY <= rowY + MENU_ROW_H;
}

static void handleSettingsMenuTap(const TouchEvent &event) {
    if (tapHitSettingsButton(event)) {
        uiMode = UiMode::PAGE_BROWSE;
        settingsMenuPage = 0; // reset so the menu opens on page 1 next time
        applyBacklightForCurrentPage(); // in case brightness changed while we were in here
        redrawCurrentScreen();
        return;
    }

    if (event.swiped) {
        settingsMenuPage = (settingsMenuPage + 1) % SETTINGS_MENU_PAGE_COUNT;
        drawSettingsMenu();
        return;
    }

    if (settingsMenuPage == 1 && tapInRow(event, MENU_ROW_3_Y)) {
        settingsPortalStart(appConfig); // portal draws its own screen; menu redraw happens when it closes
        return;
    }

    bool changed = false;
    if (settingsMenuPage == 0) {
        if (tapInRow(event, MENU_ROW_1_Y)) {
            appConfig.display.showValueGrid = !appConfig.display.showValueGrid;
            changed = true;
        } else if (tapInRow(event, MENU_ROW_2_Y)) {
            appConfig.display.rotation = (appConfig.display.rotation + 1) % 4;
            changed = true;
        } else if (tapInRow(event, MENU_ROW_3_Y)) {
            cycleOption(appConfig.display.dimmingPct, BRIGHTNESS_OPTIONS, BRIGHTNESS_OPTIONS_COUNT);
            changed = true;
        } else if (tapInRow(event, MENU_ROW_4_Y)) {
            cycleOption(appConfig.display.nightModeDimmingPct, NIGHT_BRIGHTNESS_OPTIONS, NIGHT_BRIGHTNESS_OPTIONS_COUNT);
            changed = true;
        }
    } else {
        if (tapInRow(event, MENU_ROW_1_Y)) {
            cycleOption(appConfig.display.timeoutS, TIMEOUT_OPTIONS, TIMEOUT_OPTIONS_COUNT);
            changed = true;
        } else if (tapInRow(event, MENU_ROW_2_Y)) {
            cycleOption(appConfig.display.nightModeTimeoutS, TIMEOUT_OPTIONS, TIMEOUT_OPTIONS_COUNT);
            changed = true;
        }
    }

    if (changed) {
        storageSaveAll(appConfig);
        applyConfigChange();
        drawSettingsMenu();
    }
}

void setup() {
    Serial.begin(115200);

    watchdogInit();

    storageLoadAll(appConfig);
    refreshPages();

    if (!displayInit()) {
        Serial.println("[boot] display init reported a failure — continuing anyway, screen may be blank");
    }
    displaySetRotation(appConfig.display.rotation);
    backlightInit();
    applyBacklightForCurrentPage();

    touchInit();
    touchSetRotation(appConfig.display.rotation);

    readingsInit();

    // BLE scan reads appConfig by reference — settings-portal saves that
    // update appConfig in place will be picked up automatically without
    // needing to reinit the scan.
    bleScanInit(appConfig);

    settingsPortalSetOnSave(applyConfigChange);

    lastTouchActivityMs = millis();
    redrawCurrentScreen();

    Serial.printf("[boot] %u active page(s)\n", activePageCount);
}

void loop() {
    watchdogFeed();

    TouchEvent event = touchPoll();

    bool wasSleeping = screenSleeping;
    if (event.swiped || event.tapped) {
        wakeScreen();
        if (wasSleeping) {
            // This touch just woke the screen -- don't also act on it as
            // a swipe/tap, or picking the device up would immediately
            // flip a page or open the settings menu.
            event.swiped = false;
            event.tapped = false;
        }
    }
    checkSleepTimeout();

    if (settingsPortalIsActive()) {
        settingsPortalLoop();
        if (tapHitSettingsButton(event)) {
            settingsPortalStop();
            uiMode = UiMode::SETTINGS_MENU; // came from the menu's WiFi row -- go back there, not straight to pages
            drawSettingsMenu();
        }
        return;
    }

    if (uiMode == UiMode::SETTINGS_MENU) {
        handleSettingsMenuTap(event);
        return;
    }

    // uiMode == PAGE_BROWSE
    if (event.swiped && activePageCount > 0) {
        currentPageIndex = (currentPageIndex + activePageCount + event.swipeDirection) % activePageCount;
        applyBacklightForCurrentPage(); // may differ now if we swiped onto/off of the Night Mode page
        redrawCurrentScreen();
    } else if (tapHitSettingsButton(event)) {
        uiMode = UiMode::SETTINGS_MENU;
        drawSettingsMenu();
    }

    // BLE advertisement processing happens on NimBLE's own FreeRTOS task
    // (the ScanCallbacks::onResult callback in ble_scan.cpp) — nothing to
    // drain here. Just periodically redraw the current page so new
    // readings actually show up on screen between swipes.
    static uint32_t lastPeriodicDraw = 0;
    constexpr uint32_t DRAW_INTERVAL_MS = 2000;
    if (millis() - lastPeriodicDraw > DRAW_INTERVAL_MS) {
        lastPeriodicDraw = millis();
        redrawCurrentScreen();
    }

    delay(20);
}

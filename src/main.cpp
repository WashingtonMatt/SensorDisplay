#include <Arduino.h>
#include <string.h>
#include "config.h"
#include "storage.h"
#include "pages.h"
#include "watchdog.h"
#include "settings_portal.h"
#include "display.h"
#include "touch.h"
#include "ble_scan.h"
#include "render.h"
#include "mac_utils.h"

static AppConfig appConfig;
static PageEntry activePages[2 + MAX_RUUVITAGS];
static uint8_t activePageCount = 0;
static uint8_t currentPageIndex = 0;

// --- TEMPORARY: hardcoded test config ---------------------------------
// Stand-in for the settings portal RuuviTag-add route, which doesn't
// exist yet. Fill in TEST_RUUVI_MAC below with your tag's real MAC
// (format "AA:BB:CC:DD:EE:FF" — check the Ruuvi app or nRF Connect) and
// flash. Delete this whole block once the portal can do this for real.
#define TESTING_HARDCODED_CONFIG 1

#if TESTING_HARDCODED_CONFIG
static const char *TEST_RUUVI_MAC = "DE:73:C5:CF:2C:02"; // <-- replace with your tag's real MAC

static void applyHardcodedTestConfig(AppConfig &cfg) {
    uint8_t mac[6];
    if (!macFromString(TEST_RUUVI_MAC, mac)) {
        Serial.println("[test] TEST_RUUVI_MAC is malformed — check the AA:BB:CC:DD:EE:FF format");
        return;
    }
    memcpy(cfg.ruuviTags[0].mac, mac, 6);
    strncpy(cfg.ruuviTags[0].label, "RuuviTag", LABEL_MAX_LEN - 1);
    cfg.ruuviTags[0].lowTempF = 40.0f;
    cfg.ruuviTags[0].hiTempF = 85.0f;
    cfg.ruuviTags[0].configured = true;
    Serial.println("[test] hardcoded RuuviTag config applied");
}
#endif


// Rebuild page list after any config change (boot, or settings-portal save).
static void refreshPages() {
    activePageCount = buildPageList(appConfig, activePages, sizeof(activePages) / sizeof(activePages[0]));
    if (currentPageIndex >= activePageCount) {
        currentPageIndex = 0;
    }
}

// TODO: hook to some physical trigger for entering settings (e.g. hold
// touch on a corner for N seconds, or a boot-button combo) — port
// whatever gesture the old fork used. Left stubbed until the settings
// portal HTML routes exist (there'd be nothing to configure yet).
static bool settingsEntryRequested() {
    return false;
}

void setup() {
    Serial.begin(115200);

    watchdogInit();

    storageLoadAll(appConfig);
#if TESTING_HARDCODED_CONFIG
    applyHardcodedTestConfig(appConfig);
#endif
    refreshPages();

    if (!displayInit()) {
        Serial.println("[boot] display init reported a failure — continuing anyway, screen may be blank");
    }
    displaySetRotation(appConfig.display.rotation);
    backlightInit();
    backlightSetDutyPct(appConfig.display.dimmingPct);

    touchInit();
    touchSetRotation(appConfig.display.rotation);

    // BLE scan reads appConfig by reference — settings-portal saves that
    // update appConfig in place will be picked up automatically without
    // needing to reinit the scan.
    bleScanInit(appConfig);

    if (activePageCount > 0) {
        renderPage(activePages[currentPageIndex], appConfig);
    } else {
        gfx->fillScreen(BLACK);
        gfx->setTextColor(WHITE);
        gfx->setTextSize(1);
        gfx->setCursor(30, 110);
        gfx->print("No devices configured");
        gfx->flush();
    }

    Serial.printf("[boot] %u active page(s)\n", activePageCount);
}

void loop() {
    watchdogFeed();

    if (settingsPortalIsActive()) {
        settingsPortalLoop();
        // NOTE: per project spec, HTTP handling takes priority before
        // touch polling in the single-threaded loop() while the portal
        // is active — keep that ordering, don't interleave touch polling
        // ahead of server.handleClient() here.
        return;
    }

    int8_t swipeDirection = 0;
    if (touchPollForSwipe(&swipeDirection) && activePageCount > 0) {
        currentPageIndex = (currentPageIndex + activePageCount + swipeDirection) % activePageCount;
        renderPage(activePages[currentPageIndex], appConfig);
    }

    // BLE advertisement processing happens on NimBLE's own FreeRTOS task
    // (the ScanCallbacks::onResult callback in ble_scan.cpp) — nothing to
    // drain here. Just periodically redraw the current page so new
    // readings actually show up on screen between swipes.
    static uint32_t lastPeriodicDraw = 0;
    constexpr uint32_t DRAW_INTERVAL_MS = 2000;
    if (activePageCount > 0 && millis() - lastPeriodicDraw > DRAW_INTERVAL_MS) {
        lastPeriodicDraw = millis();
        renderPage(activePages[currentPageIndex], appConfig);
    }

    if (settingsEntryRequested()) {
        settingsPortalStart();
    }

    delay(20);
}

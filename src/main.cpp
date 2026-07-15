#include <Arduino.h>
#include "config.h"
#include "storage.h"
#include "pages.h"
#include "watchdog.h"
#include "ble_decoders.h"
#include "settings_portal.h"
#include "display.h"

static AppConfig appConfig;
static PageEntry activePages[2 + MAX_RUUVITAGS];
static uint8_t activePageCount = 0;
static uint8_t currentPageIndex = 0;

// Rebuild page list after any config change (boot, or settings-portal save).
static void refreshPages() {
    activePageCount = buildPageList(appConfig, activePages, sizeof(activePages) / sizeof(activePages[0]));
    if (currentPageIndex >= activePageCount) {
        currentPageIndex = 0;
    }
}

// TODO: hook to some physical trigger for entering settings (e.g. hold
// touch on a corner for N seconds, or a boot-button combo) — port
// whatever gesture the old fork used.
static bool settingsEntryRequested() {
    return false;
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
    backlightSetDutyPct(appConfig.display.dimmingPct);

    // Smoke test only — confirms the canvas actually reaches the physical
    // panel. Without a flush() here the GC9A01's GRAM just shows whatever
    // random data was there at power-on (uninitialized, not a bug).
    // Replace this with real page rendering in the loop() TODO below.
    gfx->fillScreen(BLACK);
    gfx->setTextColor(WHITE);
    gfx->setTextSize(2);
    gfx->setCursor(20, 110);
    gfx->print("SensorDisplay");
    gfx->flush();

    // TODO: touch controller init
    // TODO: NimBLE init + start scan (Victron + RuuviTag manufacturer IDs).
    // Once wired, feed each advertisement's manufacturer data + source MAC
    // into decodeVictronShunt() / decodeVictronMppt() / decodeRuuviTag()
    // (ble_decoders.h) and route by slotIndex from activePages[] for
    // RuuviTag readings.

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
    } else {
        // TODO: touch polling -> page swipe navigation
        // TODO: BLE advertisement processing (NimBLE callbacks run on
        // their own FreeRTOS task already — this is just draining any
        // queued readings into the display state)
        // TODO: render current page (activePages[currentPageIndex])

        if (settingsEntryRequested()) {
            settingsPortalStart();
        }
    }
}

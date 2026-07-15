#include <Arduino.h>
#include "config.h"
#include "storage.h"
#include "pages.h"
#include "watchdog.h"
#include "ble_decoders.h"
#include "settings_portal.h"

// TODO: display driver include + Arduino_Canvas offscreen double-buffer
// setup — port directly from the previous ChargeScreen fork
// (github.com/WashingtonMatt/ChargeScreen), it's confirmed working and
// fixes the flicker issue. Not reproduced here since it's carried over
// as-is, not new work.

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

    // TODO: display init (GC9A01 over SPI) + Arduino_Canvas double buffer
    // TODO: touch controller init
    // TODO: NimBLE init + start scan (Victron + RuuviTag manufacturer IDs)

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

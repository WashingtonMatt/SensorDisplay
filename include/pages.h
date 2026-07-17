#pragma once
#include "config.h"

// Core UI rule (from project spec): a device's page only appears if that
// device is configured. This applies to Victron pages and RuuviTag pages
// alike — the on-device page list is built at runtime, never fixed.
//
// Deliberate exception: SLEEP_SCREEN ("Night Mode") is a standalone
// utility page, not tied to any configured device, so it always appears
// at the end of the page list regardless of what's configured.

enum class PageType : uint8_t {
    VICTRON_SHUNT,
    VICTRON_MPPT,
    RUUVI_TAG,     // one PageEntry per configured tag, see slotIndex
    SLEEP_SCREEN,  // always present -- see note above
};

struct PageEntry {
    PageType type;
    uint8_t slotIndex;   // meaningful for RUUVI_TAG (0..MAX_RUUVITAGS-1); unused otherwise
};

// Rebuilds the active page list from the current AppConfig. Call this at
// boot after storageLoadAll(), and again any time settings are saved
// (e.g. a RuuviTag added/removed/edited, or a Victron key set/cleared).
//
// `outPages` must be sized for at least (3 + MAX_RUUVITAGS) entries
// (2 Victron + up to 4 RuuviTag + 1 always-present SLEEP_SCREEN).
// Returns the number of pages actually populated.
uint8_t buildPageList(const AppConfig &cfg, PageEntry outPages[], uint8_t maxPages);

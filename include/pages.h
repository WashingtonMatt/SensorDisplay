#pragma once
#include "config.h"

// Core UI rule (from project spec): a device's page only appears if that
// device is configured. This applies to Victron pages and RuuviTag pages
// alike — the on-device page list is built at runtime, never fixed.

enum class PageType : uint8_t {
    VICTRON_SHUNT,
    VICTRON_MPPT,
    RUUVI_TAG,   // one PageEntry per configured tag, see slotIndex
};

struct PageEntry {
    PageType type;
    uint8_t slotIndex;   // meaningful for RUUVI_TAG (0..MAX_RUUVITAGS-1); unused otherwise
};

// Rebuilds the active page list from the current AppConfig. Call this at
// boot after storageLoadAll(), and again any time settings are saved
// (e.g. a RuuviTag added/removed/edited, or a Victron key set/cleared).
//
// `outPages` must be sized for at least (2 + MAX_RUUVITAGS) entries.
// Returns the number of pages actually populated.
uint8_t buildPageList(const AppConfig &cfg, PageEntry outPages[], uint8_t maxPages);

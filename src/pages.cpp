#include "pages.h"

uint8_t buildPageList(const AppConfig &cfg, PageEntry outPages[], uint8_t maxPages) {
    uint8_t count = 0;

    if (cfg.victron.shuntConfigured && count < maxPages) {
        outPages[count++] = {PageType::VICTRON_SHUNT, 0};
    }
    if (cfg.victron.mpptConfigured && count < maxPages) {
        outPages[count++] = {PageType::VICTRON_MPPT, 0};
    }
    for (uint8_t i = 0; i < MAX_RUUVITAGS && count < maxPages; i++) {
        if (cfg.ruuviTags[i].configured) {
            outPages[count++] = {PageType::RUUVI_TAG, i};
        }
    }

    // Always present, regardless of what's configured -- see pages.h.
    if (count < maxPages) {
        outPages[count++] = {PageType::SLEEP_SCREEN, 0};
    }

    return count;
}

#include "ble_scan.h"
#include "ble_decoders.h"
#include "readings.h"
#include "mac_utils.h"
#include "diag_log.h"
#include <NimBLEDevice.h>
#include <string.h>

static const AppConfig *cfgPtr = nullptr;

class ScanCallbacks : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice *device) override {
        if (!cfgPtr || !device->haveManufacturerData()) return;

        std::string mfg = device->getManufacturerData();
        const uint8_t *data = reinterpret_cast<const uint8_t *>(mfg.data());
        size_t len = mfg.size();

        uint8_t sourceMac[6];
        if (!macFromString(String(device->getAddress().toString().c_str()), sourceMac)) {
            return;
        }

        // Diagnostic: log any Victron-company-ID advertisement regardless
        // of whether its MAC matches what's configured. If your shunt/
        // MPPT never shows up here at all, either Instant Readout isn't
        // enabled for it in the Victron app, or you're out of BLE range.
        // If it shows up here but never gets past decodeVictronShunt/Mppt,
        // check the per-check logs those emit -- most likely cause is a
        // MAC typo (compare the address below against what you entered
        // in the portal) or a wrong AES key.
        if (len >= 2) {
            uint16_t companyId = static_cast<uint8_t>(data[0]) | (static_cast<uint8_t>(data[1]) << 8);
            if (companyId == VICTRON_COMPANY_ID) {
                static uint32_t lastVictronLogMs = 0;
                if (millis() - lastVictronLogMs > 3000) {
                    lastVictronLogMs = millis();
                    char buf[96];
                    snprintf(buf, sizeof(buf), "[ble] Victron advertisement from %s (len=%u, rssi=%d)",
                             device->getAddress().toString().c_str(), static_cast<unsigned>(len), device->getRSSI());
                    diagLog(buf);
                }
            }
        }

        VictronShuntReading shunt = decodeVictronShunt(data, len, sourceMac, cfgPtr->victron);
        if (shunt.valid) {
            latestReadings.shunt = shunt;
            latestReadings.shuntSeenMs = millis();
            return; // manufacturer-data records are mutually exclusive per advertisement
        }

        VictronMpptReading mppt = decodeVictronMppt(data, len, sourceMac, cfgPtr->victron);
        if (mppt.valid) {
            latestReadings.mppt = mppt;
            latestReadings.mpptSeenMs = millis();
            return;
        }

        for (uint8_t i = 0; i < MAX_RUUVITAGS; i++) {
            if (!cfgPtr->ruuviTags[i].configured) continue;
            if (memcmp(sourceMac, cfgPtr->ruuviTags[i].mac, 6) != 0) continue;

            RuuviReading r = decodeRuuviTag(data, len, sourceMac, cfgPtr->ruuviTags[i]);

            // Diagnostic: this advertisement's MAC matches a configured
            // RuuviTag slot. Logging it here (regardless of whether the
            // decode above succeeded) is the key signal for "is the radio
            // even hearing this tag" vs. "hearing it but failing to
            // decode it" -- if this line never shows up in the portal's
            // Debug Log while the tag is confirmed nearby and advertising,
            // that's a scan/range/interference problem, not a data-format
            // one. Throttled per-tag same as the Victron block above.
            static uint32_t lastRuuviLogMs[MAX_RUUVITAGS] = {0};
            if (millis() - lastRuuviLogMs[i] > 3000) {
                lastRuuviLogMs[i] = millis();
                char buf[120];
                snprintf(buf, sizeof(buf), "[ble] RuuviTag slot %u (%s) advertisement seen, len=%u, rssi=%d, decoded=%s",
                         static_cast<unsigned>(i), cfgPtr->ruuviTags[i].label, static_cast<unsigned>(len),
                         device->getRSSI(), r.valid ? "yes" : "no");
                diagLog(buf);
            }

            if (r.valid) {
                readingsRecordRuuvi(i, r, millis());
                return;
            }
        }
    }
};

static ScanCallbacks scanCallbacks;

void bleScanInit(const AppConfig &cfg) {
    cfgPtr = &cfg;

    NimBLEDevice::init("SensorDisplay");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    // Scan interval/window ported as-is from the previous fork — tuned
    // there to reliably catch Victron's and Ruuvi's ~1s advertisement
    // interval without hogging the shared radio.
    NimBLEScan *scan = NimBLEDevice::getScan();
    scan->setScanCallbacks(&scanCallbacks, false);
    scan->setDuplicateFilter(0);
    scan->setMaxResults(0);
    scan->setActiveScan(true);
    scan->setInterval(97);
    scan->setWindow(67);
    scan->start(0, false, false);

    diagLog("[ble] scan started");
}

void bleScanDeinit() {
    NimBLEDevice::deinit(true); // true = release controller memory too, not just stop scanning
    diagLog("[ble] scan fully stopped (deinit) for portal WiFi transition");
}

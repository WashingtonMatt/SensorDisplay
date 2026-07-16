#include "ble_scan.h"
#include "ble_decoders.h"
#include "readings.h"
#include "mac_utils.h"
#include <NimBLEDevice.h>

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
            RuuviReading r = decodeRuuviTag(data, len, sourceMac, cfgPtr->ruuviTags[i]);
            if (r.valid) {
                latestReadings.ruuvi[i] = r;
                latestReadings.ruuviSeenMs[i] = millis();
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

    Serial.println("[ble] scan started");
}

void bleScanPause() {
    NimBLEDevice::getScan()->stop();
    Serial.println("[ble] scan paused");
}

void bleScanResume() {
    NimBLEDevice::getScan()->start(0, false, false);
    Serial.println("[ble] scan resumed");
}

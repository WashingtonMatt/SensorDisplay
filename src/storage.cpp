#include "storage.h"
#include "mac_utils.h"
#include <Preferences.h>
#include <ArduinoJson.h>

static Preferences prefs;


bool storageLoadRuuviTags(RuuviTagConfig (&tags)[MAX_RUUVITAGS]) {
    for (uint8_t i = 0; i < MAX_RUUVITAGS; i++) {
        tags[i] = RuuviTagConfig{}; // reset to defaults / unconfigured
    }

    prefs.begin(NVS_NAMESPACE, /*readOnly=*/true);
    String blob = prefs.getString(NVS_KEY_RUUVI_BLOB, "[]");
    prefs.end();

    JsonDocument doc; // ArduinoJson v7 — auto-sized
    DeserializationError err = deserializeJson(doc, blob);
    if (err) {
        Serial.printf("[storage] RuuviTag blob parse failed: %s\n", err.c_str());
        return false;
    }

    JsonArray arr = doc.as<JsonArray>();
    uint8_t slot = 0;
    for (JsonObject obj : arr) {
        if (slot >= MAX_RUUVITAGS) break;

        const char* label = obj["label"] | "RuuviTag";
        const char* macStr = obj["mac"] | "";
        float lo = obj["lo"] | 32.0f;
        float hi = obj["hi"] | 90.0f;

        uint8_t mac[6];
        if (!macFromString(String(macStr), mac)) {
            Serial.printf("[storage] skipping RuuviTag slot with bad MAC: %s\n", macStr);
            continue;
        }

        strncpy(tags[slot].label, label, LABEL_MAX_LEN - 1);
        memcpy(tags[slot].mac, mac, 6);
        tags[slot].lowTempF = lo;
        tags[slot].hiTempF = hi;
        tags[slot].configured = true;
        slot++;
    }

    return true;
}

bool storageSaveRuuviTags(const RuuviTagConfig (&tags)[MAX_RUUVITAGS]) {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    for (uint8_t i = 0; i < MAX_RUUVITAGS; i++) {
        if (!tags[i].configured) continue;
        JsonObject obj = arr.add<JsonObject>();
        obj["label"] = tags[i].label;
        obj["mac"] = macToString(tags[i].mac);
        obj["lo"] = tags[i].lowTempF;
        obj["hi"] = tags[i].hiTempF;
    }

    String out;
    serializeJson(doc, out);

    prefs.begin(NVS_NAMESPACE, /*readOnly=*/false);
    size_t written = prefs.putString(NVS_KEY_RUUVI_BLOB, out);
    prefs.end();

    if (written == 0 && out.length() > 0) {
        Serial.println("[storage] RuuviTag blob write failed");
        return false;
    }
    return true;
}

void storageLoadAll(AppConfig &out) {
    prefs.begin(NVS_NAMESPACE, /*readOnly=*/true);

    size_t shuntLen = prefs.getBytesLength(NVS_KEY_SHUNT_MAC);
    if (shuntLen == 6) {
        prefs.getBytes(NVS_KEY_SHUNT_MAC, out.victron.shuntMac, 6);
        prefs.getBytes(NVS_KEY_SHUNT_AESKEY, out.victron.shuntAesKey, 16);
        out.victron.shuntConfigured = true;
    }

    size_t mpptLen = prefs.getBytesLength(NVS_KEY_MPPT_MAC);
    if (mpptLen == 6) {
        prefs.getBytes(NVS_KEY_MPPT_MAC, out.victron.mpptMac, 6);
        prefs.getBytes(NVS_KEY_MPPT_AESKEY, out.victron.mpptAesKey, 16);
        out.victron.mpptConfigured = true;
    }

    out.display.rotation = prefs.getUChar(NVS_KEY_ROTATION, 0);
    out.display.dimmingPct = prefs.getUChar(NVS_KEY_DIMMING, 100);
    out.display.timeoutS = prefs.getUShort(NVS_KEY_TIMEOUT_S, 0);

    prefs.end();

    storageLoadRuuviTags(out.ruuviTags);
}

void storageSaveAll(const AppConfig &cfg) {
    prefs.begin(NVS_NAMESPACE, /*readOnly=*/false);

    if (cfg.victron.shuntConfigured) {
        prefs.putBytes(NVS_KEY_SHUNT_MAC, cfg.victron.shuntMac, 6);
        prefs.putBytes(NVS_KEY_SHUNT_AESKEY, cfg.victron.shuntAesKey, 16);
    }
    if (cfg.victron.mpptConfigured) {
        prefs.putBytes(NVS_KEY_MPPT_MAC, cfg.victron.mpptMac, 6);
        prefs.putBytes(NVS_KEY_MPPT_AESKEY, cfg.victron.mpptAesKey, 16);
    }

    prefs.putUChar(NVS_KEY_ROTATION, cfg.display.rotation);
    prefs.putUChar(NVS_KEY_DIMMING, cfg.display.dimmingPct);
    prefs.putUShort(NVS_KEY_TIMEOUT_S, cfg.display.timeoutS);

    prefs.end();

    storageSaveRuuviTags(cfg.ruuviTags);
}

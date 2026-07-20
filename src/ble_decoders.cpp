#include "ble_decoders.h"
#include "diag_log.h"
#include <mbedtls/aes.h>
#include <string.h>

// -----------------------------------------------------------------------
// Ported from the previous ChargeScreen fork's Victron/RuuviTag decoders
// (fix/display-flicker and feature/RuuviTag branches respectively) —
// confirmed working on real hardware there. Adapted here only to:
//   - take raw (const uint8_t*, size_t) instead of std::string
//   - take the per-device MAC/key from VictronConfig/RuuviTagConfig
//     instead of a single global stored key
//   - check sourceMac against the configured device (was a no-op stub
//     in the old single-device code, now load-bearing for multi-tag)
// The actual crypto and bit-unpacking logic is untouched.
// -----------------------------------------------------------------------

static bool macMatches(const uint8_t a[6], const uint8_t b[6]) {
    return memcmp(a, b, 6) == 0;
}

static int32_t signExtend(uint32_t value, uint8_t width) {
    uint32_t signBit = 1UL << (width - 1);
    uint32_t mask = (1UL << width) - 1;
    value &= mask;
    return static_cast<int32_t>((value ^ signBit) - signBit);
}

static uint32_t readBits(const uint8_t *data, size_t dataLen, uint16_t startBit, uint8_t bitCount) {
    uint32_t value = 0;
    for (uint8_t i = 0; i < bitCount; i++) {
        uint16_t bit = startBit + i;
        if ((bit / 8) >= dataLen) break;
        if (data[bit / 8] & (1 << (bit % 8))) {
            value |= (1UL << i);
        }
    }
    return value;
}

static bool readLeU16(const uint8_t *data, size_t dataLen, size_t offset, uint16_t &value) {
    if (offset + 1 >= dataLen) return false;
    value = static_cast<uint16_t>(data[offset]) |
            (static_cast<uint16_t>(data[offset + 1]) << 8);
    return true;
}

static bool aesCtrDecrypt(const uint8_t *key, uint16_t nonce,
                           const uint8_t *encrypted, size_t encryptedLen,
                           uint8_t *plain) {
    uint8_t counter[16] = {};
    uint8_t stream[16] = {};
    counter[0] = nonce & 0xFF;
    counter[1] = nonce >> 8;

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    if (mbedtls_aes_setkey_enc(&aes, key, 128) != 0) {
        mbedtls_aes_free(&aes);
        return false;
    }

    for (size_t offset = 0; offset < encryptedLen; offset += sizeof(stream)) {
        if (mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, counter, stream) != 0) {
            mbedtls_aes_free(&aes);
            return false;
        }
        size_t blockLen = min(sizeof(stream), encryptedLen - offset);
        for (size_t i = 0; i < blockLen; i++) {
            plain[offset + i] = encrypted[offset + i] ^ stream[i];
        }
        for (int i = 0; i < 16; i++) {
            if (++counter[i] != 0) break;
        }
    }

    mbedtls_aes_free(&aes);
    return true;
}

// payload here is mfgData + 2 (past the 2-byte company ID), matching the
// old fork's layout: payload[0]=advert type, payload[4]=record type,
// payload[5..6]=nonce, payload[7]=key[0] check byte, payload[8..]=ciphertext.
static bool decryptVictronPayload(const uint8_t *payload, size_t payloadLen,
                                   const uint8_t key[16],
                                   uint8_t *plain, size_t &plainLen) {
    if (payload[7] != key[0]) {
        return false; // key mismatch
    }
    uint16_t nonce = static_cast<uint16_t>(payload[5]) | (static_cast<uint16_t>(payload[6]) << 8);
    const uint8_t *encrypted = payload + 8;
    size_t encryptedLen = payloadLen - 8;
    plainLen = min(encryptedLen, plainLen);
    return aesCtrDecrypt(key, nonce, encrypted, plainLen, plain);
}

static bool isVictronInstantReadout(const uint8_t *mfgData, size_t len) {
    if (len < 4) return false;
    uint16_t companyId = static_cast<uint8_t>(mfgData[0]) | (static_cast<uint8_t>(mfgData[1]) << 8);
    uint8_t advertisementType = mfgData[2];
    return companyId == VICTRON_COMPANY_ID && advertisementType == VICTRON_PRODUCT_ADVERTISEMENT;
}

VictronShuntReading decodeVictronShunt(const uint8_t *mfgData, size_t len,
                                        const uint8_t sourceMac[6],
                                        const VictronConfig &cfg) {
    VictronShuntReading out;
    if (!cfg.shuntConfigured || !macMatches(sourceMac, cfg.shuntMac)) return out;

    // Past this point the MAC matched the configured shunt, so any
    // failure below is worth logging -- it means the device is in range
    // and broadcasting, but something about the decode is off. Each site
    // is throttled independently: these fire on every single matching
    // advertisement otherwise (roughly once a second), which can flood
    // the 24-line diag_log ring buffer and push out everything else --
    // including diagnostics for other devices -- within seconds.
    if (!isVictronInstantReadout(mfgData, len) || len < 11) {
        static uint32_t lastLogMs = 0;
        if (millis() - lastLogMs > 5000) {
            lastLogMs = millis();
            char buf[96];
            snprintf(buf, sizeof(buf), "[victron] shunt MAC matched but advertisement isn't Instant Readout format (len=%u)",
                     static_cast<unsigned>(len));
            diagLog(buf);
        }
        return out;
    }

    const uint8_t *payload = mfgData + 2;
    size_t payloadLen = len - 2;
    if (payloadLen < 9 || payload[4] != VICTRON_BATTERY_MONITOR_RECORD) {
        static uint32_t lastLogMs = 0;
        if (millis() - lastLogMs > 5000) {
            lastLogMs = millis();
            char buf[144];
            snprintf(buf, sizeof(buf),
                     "[victron] shunt MAC matched but record type is 0x%02X, expected 0x%02X (battery monitor) "
                     "-- check you didn't put the MPPT's MAC in the shunt field",
                     payloadLen >= 5 ? payload[4] : 0xFF, VICTRON_BATTERY_MONITOR_RECORD);
            diagLog(buf);
        }
        return out;
    }

    uint8_t plain[24] = {};
    size_t plainLen = sizeof(plain);
    if (!decryptVictronPayload(payload, payloadLen, cfg.shuntAesKey, plain, plainLen)) {
        static uint32_t lastLogMs = 0;
        if (millis() - lastLogMs > 5000) {
            lastLogMs = millis();
            diagLog("[victron] shunt MAC and record type matched, but the AES key check failed "
                    "-- re-check the 32-character hex key from the Victron app");
        }
        return out;
    }
    if (plainLen < 14) return out;

    int32_t currentRaw = signExtend(readBits(plain, plainLen, 66, 22), 22);
    uint32_t consumedRaw = readBits(plain, plainLen, 88, 20);

    int remainingMins = readBits(plain, plainLen, 0, 16);
    float voltage = static_cast<int16_t>(readBits(plain, plainLen, 16, 16)) * 0.01f;
    float current = currentRaw * 0.001f;

    out.batteryVoltageV = voltage;
    out.currentA = current;
    out.remainingMins = remainingMins;
    out.stateOfChargePct = readBits(plain, plainLen, 108, 10) * 0.1f;
    out.valid = true;
    (void)consumedRaw; // consumedAh not carried in VictronShuntReading yet — add if the display page needs it
    return out;
}

VictronMpptReading decodeVictronMppt(const uint8_t *mfgData, size_t len,
                                      const uint8_t sourceMac[6],
                                      const VictronConfig &cfg) {
    VictronMpptReading out;
    if (!cfg.mpptConfigured || !macMatches(sourceMac, cfg.mpptMac)) return out;

    if (!isVictronInstantReadout(mfgData, len) || len < 11) {
        static uint32_t lastLogMs = 0;
        if (millis() - lastLogMs > 5000) {
            lastLogMs = millis();
            char buf[96];
            snprintf(buf, sizeof(buf), "[victron] MPPT MAC matched but advertisement isn't Instant Readout format (len=%u)",
                     static_cast<unsigned>(len));
            diagLog(buf);
        }
        return out;
    }

    const uint8_t *payload = mfgData + 2;
    size_t payloadLen = len - 2;
    if (payloadLen < 9 || payload[4] != VICTRON_SOLAR_CHARGER_RECORD) {
        static uint32_t lastLogMs = 0;
        if (millis() - lastLogMs > 5000) {
            lastLogMs = millis();
            char buf[144];
            snprintf(buf, sizeof(buf),
                     "[victron] MPPT MAC matched but record type is 0x%02X, expected 0x%02X (solar charger) "
                     "-- check you didn't put the shunt's MAC in the MPPT field",
                     payloadLen >= 5 ? payload[4] : 0xFF, VICTRON_SOLAR_CHARGER_RECORD);
            diagLog(buf);
        }
        return out;
    }

    uint8_t plain[16] = {};
    size_t plainLen = sizeof(plain);
    if (!decryptVictronPayload(payload, payloadLen, cfg.mpptAesKey, plain, plainLen)) {
        static uint32_t lastLogMs = 0;
        if (millis() - lastLogMs > 5000) {
            lastLogMs = millis();
            diagLog("[victron] MPPT MAC and record type matched, but the AES key check failed "
                    "-- re-check the 32-character hex key from the Victron app");
        }
        return out;
    }
    if (plainLen < 12) return out;

    uint16_t batteryVoltageRaw = 0, pvPowerRaw = 0, batteryCurrentRaw = 0, yieldRaw = 0;
    if (!readLeU16(plain, plainLen, 2, batteryVoltageRaw) ||
        !readLeU16(plain, plainLen, 4, batteryCurrentRaw) ||
        !readLeU16(plain, plainLen, 6, yieldRaw) ||
        !readLeU16(plain, plainLen, 8, pvPowerRaw)) {
        return out;
    }

    out.chargeState = plain[0];
    out.batteryVoltageV = (batteryVoltageRaw == 0x7FFF) ? NAN : static_cast<int16_t>(batteryVoltageRaw) * 0.01f;
    out.batteryCurrentA = (batteryCurrentRaw == 0x7FFF) ? NAN : static_cast<int16_t>(batteryCurrentRaw) * 0.1f;
    out.yieldTodayKwh = (yieldRaw == 0xFFFF) ? NAN : yieldRaw * 0.01f;
    out.panelPowerW = (pvPowerRaw == 0xFFFF) ? NAN : static_cast<float>(pvPowerRaw);
    out.panelVoltageV = NAN; // old fork doesn't decode a separate panel voltage field — battery-side V only
    out.valid = true;
    return out;
}

static bool isRuuviRawV2(const uint8_t *mfgData, size_t len) {
    // 2 company ID bytes + 1 format byte + 2 temp bytes + 2 humidity bytes minimum.
    if (len < 7) return false;
    uint16_t companyId = static_cast<uint8_t>(mfgData[0]) | (static_cast<uint8_t>(mfgData[1]) << 8);
    uint8_t dataFormat = mfgData[2];
    return companyId == RUUVI_COMPANY_ID && dataFormat == RUUVI_RAWV2_FORMAT;
}

RuuviReading decodeRuuviTag(const uint8_t *mfgData, size_t len,
                             const uint8_t sourceMac[6],
                             const RuuviTagConfig &tagCfg) {
    RuuviReading out;
    if (!tagCfg.configured || !macMatches(sourceMac, tagCfg.mac)) return out;
    if (!isRuuviRawV2(mfgData, len)) return out;

    int16_t rawTemp = static_cast<int16_t>((static_cast<uint16_t>(mfgData[3]) << 8) | mfgData[4]);
    uint16_t rawHumidity = (static_cast<uint16_t>(mfgData[5]) << 8) | mfgData[6];

    // 0x8000 / 0xFFFF are Ruuvi's "sensor not available" markers.
    if (rawTemp == static_cast<int16_t>(0x8000) || rawHumidity == 0xFFFF) {
        return out;
    }

    float tempC = rawTemp * 0.005f;
    out.temperatureF = tempC * 9.0f / 5.0f + 32.0f;
    out.humidityPct = rawHumidity * 0.0025f;
    out.valid = true;
    return out;
}

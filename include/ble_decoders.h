#pragma once
#include "config.h"

// -----------------------------------------------------------------------
// PORTING NOTE: the Victron Instant Readout decoder (AES-CTR decrypt of
// the manufacturer-data payload) and the RuuviTag RAWv2 decoder are both
// "known-good, keep as-is" per the project spec — they worked on real
// hardware in the previous ChargeScreen fork. Port those implementations
// directly rather than rewriting from scratch; only the call sites and
// the RuuviTag single-tag -> multi-tag matching logic need to change.
//
// These structs/signatures are placeholders establishing the interface
// the rest of the skeleton (main.cpp, pages) expects, not new decoder
// logic.
// -----------------------------------------------------------------------

struct VictronShuntReading {
    float batteryVoltageV = 0;
    float currentA = 0;
    float stateOfChargePct = 0;
    int   remainingMins = 0;
    bool  valid = false;
};

struct VictronMpptReading {
    float batteryVoltageV = 0;
    float panelVoltageV = 0;
    float panelPowerW = 0;
    uint8_t chargeState = 0;
    bool  valid = false;
};

struct RuuviReading {
    float temperatureF = 0;
    float humidityPct = 0;
    bool  valid = false;
};

// Called from the NimBLE scan callback with a raw manufacturer-data
// advertisement. Each decode function checks the source MAC against the
// configured device(s) and the manufacturer ID in the payload before
// attempting to decrypt/parse; returns .valid = false on any mismatch.

VictronShuntReading decodeVictronShunt(const uint8_t *mfgData, size_t len,
                                        const VictronConfig &cfg);

VictronMpptReading decodeVictronMppt(const uint8_t *mfgData, size_t len,
                                      const VictronConfig &cfg);

// slotIndex identifies which configured RuuviTag slot this advertisement
// matched (by MAC), so the caller can route the reading to the right
// on-screen gauge / hi-lo tracker.
RuuviReading decodeRuuviTag(const uint8_t *mfgData, size_t len,
                             const RuuviTagConfig &tagCfg);

#pragma once
#include "config.h"
#include "ble_decoders.h"

struct SensorReadings {
    VictronShuntReading shunt;
    uint32_t shuntSeenMs = 0;

    VictronMpptReading mppt;
    uint32_t mpptSeenMs = 0;

    RuuviReading ruuvi[MAX_RUUVITAGS];
    uint32_t ruuviSeenMs[MAX_RUUVITAGS] = {0};

    // Running observed min/max per tag, since boot (NAN = no reading yet).
    // This is NOT the same as the configured gauge-coloring range
    // (RuuviTagConfig::lowTempF/hiTempF) -- it's what the sensor has
    // actually reported. A true rolling 24h window (vs. since-boot)
    // would need timestamped samples with eviction of anything older
    // than 24h; this is the simpler since-boot version for now.
    float ruuviRunningLowF[MAX_RUUVITAGS];
    float ruuviRunningHighF[MAX_RUUVITAGS];
};

extern SensorReadings latestReadings;

// Call once from setup(), before BLE scanning starts, to reset the
// running high/low trackers to "no data yet".
void readingsInit();

// Records a new valid RuuviTag reading for the given slot and updates
// its since-boot running low/high. Always go through this (not a direct
// assignment to latestReadings.ruuvi[slot]) so the running stats stay
// correct.
void readingsRecordRuuvi(uint8_t slot, const RuuviReading &reading, uint32_t nowMs);

// A reading is treated as stale (render as "--") past this age. Victron
// Instant Readout and RuuviTag RAWv2 both advertise roughly once a
// second, so this is generous headroom for a couple of missed BLE
// advertisements without flickering "no signal" during normal jitter.
static constexpr uint32_t READING_STALE_MS = 15000;

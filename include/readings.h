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
};

extern SensorReadings latestReadings;

// A reading is treated as stale (render as "--") past this age. Victron
// Instant Readout and RuuviTag RAWv2 both advertise roughly once a
// second, so this is generous headroom for a couple of missed BLE
// advertisements without flickering "no signal" during normal jitter.
static constexpr uint32_t READING_STALE_MS = 15000;

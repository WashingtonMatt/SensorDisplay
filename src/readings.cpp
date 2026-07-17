#include "readings.h"
#include <math.h>

SensorReadings latestReadings;

void readingsInit() {
    for (uint8_t i = 0; i < MAX_RUUVITAGS; i++) {
        latestReadings.ruuviRunningLowF[i] = NAN;
        latestReadings.ruuviRunningHighF[i] = NAN;
    }
}

void readingsRecordRuuvi(uint8_t slot, const RuuviReading &reading, uint32_t nowMs) {
    if (slot >= MAX_RUUVITAGS || !reading.valid) return;

    latestReadings.ruuvi[slot] = reading;
    latestReadings.ruuviSeenMs[slot] = nowMs;

    float &lo = latestReadings.ruuviRunningLowF[slot];
    float &hi = latestReadings.ruuviRunningHighF[slot];
    if (isnan(lo) || reading.temperatureF < lo) lo = reading.temperatureF;
    if (isnan(hi) || reading.temperatureF > hi) hi = reading.temperatureF;
}

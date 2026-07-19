#include "clock.h"
#include <stdio.h>

static constexpr int32_t MINUTES_PER_DAY = 1440;

static bool isSet = false;
static uint16_t anchorMinutes = 0;
static uint32_t anchorMillis = 0;

static int32_t wrapMinutes(int32_t minutes) {
    int32_t wrapped = minutes % MINUTES_PER_DAY;
    if (wrapped < 0) wrapped += MINUTES_PER_DAY;
    return wrapped;
}

bool clockIsSet() {
    return isSet;
}

void clockSetMinutesSinceMidnight(int32_t minutesSinceMidnight) {
    anchorMinutes = static_cast<uint16_t>(wrapMinutes(minutesSinceMidnight));
    anchorMillis = millis();
    isSet = true;
}

void clockAdjustMinutes(int16_t deltaMinutes) {
    // If nothing has set the clock yet, treat the starting point as
    // midnight rather than projecting off of undefined anchor state.
    uint16_t current = isSet ? clockGetMinutesSinceMidnight() : 0;
    anchorMinutes = static_cast<uint16_t>(wrapMinutes(static_cast<int32_t>(current) + deltaMinutes));
    anchorMillis = millis();
    isSet = true;
}

uint16_t clockGetMinutesSinceMidnight() {
    if (!isSet) return 0;
    // Unsigned subtraction here is intentional -- it's correct even
    // across a millis() rollover (~49.7 days uptime), same trick used
    // elsewhere in this codebase (isStale() etc.) for timestamp deltas.
    uint32_t elapsedMs = millis() - anchorMillis;
    uint32_t elapsedMinutes = elapsedMs / 60000UL;
    return static_cast<uint16_t>((static_cast<uint32_t>(anchorMinutes) + elapsedMinutes) % MINUTES_PER_DAY);
}

void clockFormat12Hour(char *hourOut, size_t hourOutLen, char *minuteOut, size_t minuteOutLen,
                        char *ampmOut, size_t ampmOutLen) {
    if (!isSet) {
        snprintf(hourOut, hourOutLen, "--");
        snprintf(minuteOut, minuteOutLen, "--");
        snprintf(ampmOut, ampmOutLen, "--");
        return;
    }

    uint16_t total = clockGetMinutesSinceMidnight();
    uint8_t hour24 = total / 60;
    uint8_t minute = total % 60;
    bool isPM = hour24 >= 12;
    uint8_t hour12 = hour24 % 12;
    if (hour12 == 0) hour12 = 12;

    snprintf(hourOut, hourOutLen, "%u", hour12);
    snprintf(minuteOut, minuteOutLen, "%02u", minute);
    snprintf(ampmOut, ampmOutLen, isPM ? "PM" : "AM");
}

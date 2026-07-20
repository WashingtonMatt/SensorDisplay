#include "diag_log.h"

static constexpr uint8_t DIAG_LOG_CAPACITY = 40;
static String logLines[DIAG_LOG_CAPACITY];
static uint8_t logCount = 0;
static uint8_t logNext = 0;

void diagLog(const String &line) {
    char prefix[16];
    snprintf(prefix, sizeof(prefix), "[%lus] ", millis() / 1000);
    String prefixed = String(prefix) + line;

    Serial.println(prefixed);

    logLines[logNext] = prefixed;
    logNext = (logNext + 1) % DIAG_LOG_CAPACITY;
    if (logCount < DIAG_LOG_CAPACITY) logCount++;
}

String diagLogGetAll() {
    String out;
    uint8_t start = (logCount < DIAG_LOG_CAPACITY) ? 0 : logNext;
    for (uint8_t i = 0; i < logCount; i++) {
        uint8_t idx = (start + i) % DIAG_LOG_CAPACITY;
        out += logLines[idx];
        out += "\n";
    }
    return out;
}

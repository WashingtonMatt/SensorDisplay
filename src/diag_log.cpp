#include "diag_log.h"

static constexpr uint8_t DIAG_LOG_CAPACITY = 40;
static String logLines[DIAG_LOG_CAPACITY];
static uint8_t logCount = 0;
static uint8_t logNext = 0;

static uint32_t totalLogged = 0;
static uint32_t lastPrintedTotal = 0;

void diagLog(const String &line) {
    char prefix[16];
    snprintf(prefix, sizeof(prefix), "[%lus] ", millis() / 1000);
    String prefixed = String(prefix) + line;

    logLines[logNext] = prefixed;
    logNext = (logNext + 1) % DIAG_LOG_CAPACITY;
    if (logCount < DIAG_LOG_CAPACITY) logCount++;
    totalLogged++;
}

void diagLogFlushToSerial() {
    if (totalLogged == lastPrintedTotal) return;

    uint32_t newCount = totalLogged - lastPrintedTotal;
    if (newCount > DIAG_LOG_CAPACITY) {
        // More lines arrived since the last flush than the ring buffer
        // holds -- the oldest of them are already overwritten. Just
        // resync to whatever's currently in the buffer instead of
        // printing garbage/duplicates.
        newCount = logCount;
    }

    // logNext is one past the most recently written entry. Walk back
    // `newCount` slots from there to find where the unprinted entries
    // start, then print forward from there.
    uint32_t startIdx = (logNext + DIAG_LOG_CAPACITY - newCount) % DIAG_LOG_CAPACITY;
    for (uint32_t i = 0; i < newCount; i++) {
        uint32_t idx = (startIdx + i) % DIAG_LOG_CAPACITY;
        Serial.println(logLines[idx]);
    }

    lastPrintedTotal = totalLogged;
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

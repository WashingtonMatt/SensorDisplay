#pragma once
#include <Arduino.h>

// Small ring buffer of recent diagnostic lines, viewable via the
// settings portal's /debug route (settings_portal.cpp). Exists because
// the ESP32 is often somewhere USB serial can't reach (e.g. mounted in
// an RV near the Victron devices it's trying to pair with), but a phone
// can still join its WiFi AP to check what happened.
//
// Note: BLE scanning is fully deinited while the portal is open (radio
// contention mitigation), so diagnostic lines only accumulate *before*
// opening the portal. Workflow: stay in range with the portal closed
// for a minute or so (let it scan), then open the portal and check
// /debug for what it saw during that window.

// Logs a line: adds it to the ring buffer (for /debug) immediately.
// Does NOT print to Serial directly -- see diagLogFlushToSerial() for
// why. Use this instead of Serial.print* for anything a person might
// need to see without a USB connection.
void diagLog(const String &line);

// Prints any buffered lines added since the last call to Serial, and
// only from wherever this is called -- meant to be called once per
// main loop() iteration. diagLog() itself doesn't print directly
// because most diagLog() calls in this codebase happen from inside
// NimBLE's scan callback, which runs on its own separate FreeRTOS task
// (not the main loop task) -- Serial writes issued from that other task
// were confirmed to reliably vanish on this board's native USB-CDC
// (ring buffer storage still worked fine; only the direct Serial write
// from that task context didn't reach the host). Routing all actual
// Serial output through the main loop task instead sidesteps that.
void diagLogFlushToSerial();

// Fills `out` with all buffered lines, oldest first, newline-separated.
String diagLogGetAll();

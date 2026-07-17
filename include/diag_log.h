#pragma once
#include <Arduino.h>

// Small ring buffer of recent diagnostic lines, viewable via the
// settings portal's /debug route (settings_portal.cpp). Exists because
// the ESP32 is often somewhere USB serial can't reach (e.g. mounted in
// an RV near the Victron devices it's trying to pair with), but a phone
// can still join its WiFi AP to check what happened.
//
// Note: BLE scanning is paused while the portal is open (radio
// contention mitigation), so diagnostic lines only accumulate *before*
// opening the portal. Workflow: stay in range with the portal closed
// for a minute or so (let it scan), then open the portal and check
// /debug for what it saw during that window.

// Logs a line: prints it over Serial (when USB is connected) AND adds
// it to the ring buffer (for /debug). Use this instead of Serial.print*
// for anything a person might need to see without a USB connection.
void diagLog(const String &line);

// Fills `out` with all buffered lines, oldest first, newline-separated.
String diagLogGetAll();

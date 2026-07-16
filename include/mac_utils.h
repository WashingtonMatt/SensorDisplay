#pragma once
#include <Arduino.h>

// "AA:BB:CC:DD:EE:FF" -> mac[6]. Returns false on malformed input.
bool macFromString(const String &s, uint8_t mac[6]);

// mac[6] -> "AA:BB:CC:DD:EE:FF"
String macToString(const uint8_t mac[6]);

#pragma once
#include <Arduino.h>

// "AA:BB:CC:DD:EE:FF" -> mac[6]. Returns false on malformed input.
bool macFromString(const String &s, uint8_t mac[6]);

// mac[6] -> "AA:BB:CC:DD:EE:FF"
String macToString(const uint8_t mac[6]);

// "3AB2C1..." (exactly outLen*2 hex chars, no separators) -> out[outLen].
// Returns false on malformed input (wrong length or non-hex chars).
// Ported from the previous fork's hexToBytes(), used for the Victron
// AES key settings-portal field.
bool hexToBytes(const String &hex, uint8_t *out, size_t outLen);

// out[len] -> uppercase hex string, no separators.
String bytesToHex(const uint8_t *data, size_t len);

#include "mac_utils.h"

bool macFromString(const String &s, uint8_t mac[6]) {
    if (s.length() != 17) return false;
    int vals[6];
    if (sscanf(s.c_str(), "%x:%x:%x:%x:%x:%x",
               &vals[0], &vals[1], &vals[2], &vals[3], &vals[4], &vals[5]) != 6) {
        return false;
    }
    for (int i = 0; i < 6; i++) mac[i] = (uint8_t)vals[i];
    return true;
}

String macToString(const uint8_t mac[6]) {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
}

bool hexToBytes(const String &hex, uint8_t *out, size_t outLen) {
    if (hex.length() != outLen * 2) return false;

    for (size_t i = 0; i < outLen; i++) {
        char pair[3] = {hex[i * 2], hex[i * 2 + 1], '\0'};
        char *end = nullptr;
        long value = strtol(pair, &end, 16);
        if (*end != '\0' || value < 0 || value > 255) return false;
        out[i] = static_cast<uint8_t>(value);
    }
    return true;
}

String bytesToHex(const uint8_t *data, size_t len) {
    static const char *hexChars = "0123456789ABCDEF";
    String out;
    out.reserve(len * 2);
    for (size_t i = 0; i < len; i++) {
        out += hexChars[(data[i] >> 4) & 0x0F];
        out += hexChars[data[i] & 0x0F];
    }
    return out;
}

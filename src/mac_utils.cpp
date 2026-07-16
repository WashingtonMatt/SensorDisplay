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

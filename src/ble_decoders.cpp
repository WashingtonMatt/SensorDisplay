#include "ble_decoders.h"

// TODO: port the real implementations from the previous ChargeScreen fork
// (github.com/WashingtonMatt/ChargeScreen). These are intentionally left
// as no-op stubs rather than reimplemented from scratch, since the AES-CTR
// Instant Readout decrypt and RAWv2 parsing were already confirmed working
// on real hardware there — porting is lower-risk than rewriting.

VictronShuntReading decodeVictronShunt(const uint8_t *mfgData, size_t len,
                                        const VictronConfig &cfg) {
    (void)mfgData; (void)len; (void)cfg;
    return VictronShuntReading{}; // .valid = false
}

VictronMpptReading decodeVictronMppt(const uint8_t *mfgData, size_t len,
                                      const VictronConfig &cfg) {
    (void)mfgData; (void)len; (void)cfg;
    return VictronMpptReading{}; // .valid = false
}

RuuviReading decodeRuuviTag(const uint8_t *mfgData, size_t len,
                             const RuuviTagConfig &tagCfg) {
    (void)mfgData; (void)len; (void)tagCfg;
    return RuuviReading{}; // .valid = false
}

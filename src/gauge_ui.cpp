#include "gauge_ui.h"
#include "display.h"
#include <math.h>

static uint16_t blend565(uint16_t fg, uint16_t bg, uint8_t alpha) {
    if (alpha == 255) return fg;
    if (alpha == 0) return bg;

    uint8_t fgR = ((fg >> 11) & 0x1F) << 3;
    uint8_t fgG = ((fg >> 5) & 0x3F) << 2;
    uint8_t fgB = (fg & 0x1F) << 3;
    uint8_t bgR = ((bg >> 11) & 0x1F) << 3;
    uint8_t bgG = ((bg >> 5) & 0x3F) << 2;
    uint8_t bgB = (bg & 0x1F) << 3;

    uint8_t outR = (fgR * alpha + bgR * (255 - alpha)) / 255;
    uint8_t outG = (fgG * alpha + bgG * (255 - alpha)) / 255;
    uint8_t outB = (fgB * alpha + bgB * (255 - alpha)) / 255;
    return ((outR & 0xF8) << 8) | ((outG & 0xFC) << 3) | (outB >> 3);
}

static bool readAaGlyph(const AaFont &font, char c, AaGlyph &glyph) {
    for (uint8_t i = 0; i < font.glyphCount; i++) {
        memcpy_P(&glyph, &font.glyphs[i], sizeof(AaGlyph));
        if (glyph.c == c) return true;
    }
    return false;
}

int16_t aaTextWidth(const AaFont &font, const String &text) {
    int16_t width = 0;
    AaGlyph glyph;
    for (size_t i = 0; i < text.length(); i++) {
        if (readAaGlyph(font, text[i], glyph)) width += glyph.xAdvance;
    }
    return width;
}

void drawAaText(const AaFont &font, const String &text, int16_t x, int16_t y,
                 uint16_t color, uint16_t backgroundColor) {
    int16_t cursorX = x;
    AaGlyph glyph;
    for (size_t i = 0; i < text.length(); i++) {
        if (!readAaGlyph(font, text[i], glyph)) continue;
        for (uint8_t py = 0; py < glyph.h; py++) {
            for (uint8_t px = 0; px < glyph.w; px++) {
                uint8_t alpha = pgm_read_byte(font.bitmap + glyph.offset + py * glyph.w + px);
                if (alpha == 0) continue;
                gfx->drawPixel(cursorX + glyph.xOffset + px,
                               y + glyph.yOffset + py,
                               blend565(color, backgroundColor, alpha));
            }
        }
        cursorX += glyph.xAdvance;
    }
}

void drawAaCentered(const AaFont &font, const String &text, int16_t centerX, int16_t topY,
                     uint16_t color, uint16_t backgroundColor) {
    int16_t width = aaTextWidth(font, text);
    drawAaText(font, text, centerX - width / 2, topY, color, backgroundColor);
}

void drawTinyLabel(const String &label, int16_t centerX, int16_t y, uint16_t color) {
    gfx->setFont();
    gfx->setTextSize(1);
    gfx->setTextColor(color);
    int16_t textWidth = label.length() * 6;
    gfx->setCursor(centerX - textWidth / 2, y);
    gfx->print(label);
}

void drawGaugeArcAt(int16_t centerX, float startDeg, float sweepDeg, uint16_t color) {
    if (sweepDeg <= 0.0f) return;

    float endDeg = startDeg + sweepDeg;
    while (startDeg >= 360.0f) {
        startDeg -= 360.0f;
        endDeg -= 360.0f;
    }

    if (endDeg <= 360.0f) {
        gfx->fillArc(centerX, GAUGE_CENTER_Y, GAUGE_OUTER_RADIUS, GAUGE_INNER_RADIUS, startDeg, endDeg, color);
    } else {
        gfx->fillArc(centerX, GAUGE_CENTER_Y, GAUGE_OUTER_RADIUS, GAUGE_INNER_RADIUS, startDeg, 360.0f, color);
        gfx->fillArc(centerX, GAUGE_CENTER_Y, GAUGE_OUTER_RADIUS, GAUGE_INNER_RADIUS, 0.0f, endDeg - 360.0f, color);
    }
}

void drawSegmentedGaugeArcAt(int16_t centerX, float fillSweepDeg,
                              uint16_t trackColor, uint16_t fillColor,
                              uint16_t backgroundColor) {
    float segmentSweep = GAUGE_ARC_SWEEP_DEG / GAUGE_SEGMENTS;
    float fillEndDeg = GAUGE_ARC_START_DEG + constrain(fillSweepDeg, 0.0f, GAUGE_ARC_SWEEP_DEG);

    drawGaugeArcAt(centerX, GAUGE_ARC_START_DEG, GAUGE_ARC_SWEEP_DEG, backgroundColor);

    for (uint8_t i = 0; i < GAUGE_SEGMENTS; i++) {
        float segmentStart = GAUGE_ARC_START_DEG + i * segmentSweep + GAUGE_SEGMENT_GAP_DEG * 0.5f;
        float segmentEnd = GAUGE_ARC_START_DEG + (i + 1) * segmentSweep - GAUGE_SEGMENT_GAP_DEG * 0.5f;
        float visibleSweep = max(0.0f, segmentEnd - segmentStart);

        drawGaugeArcAt(centerX, segmentStart, visibleSweep, trackColor);

        float filledEnd = min(segmentEnd, fillEndDeg);
        if (filledEnd > segmentStart) {
            drawGaugeArcAt(centerX, segmentStart, filledEnd - segmentStart, fillColor);
        }
    }
}

// Fixed °F width per gradient band -- also the ring's de facto hash-mark
// spacing. A tight range (e.g. a fridge) naturally gets fewer, closer-
// together bands than a wide one (e.g. outdoor), with no extra config.
static constexpr float RING_TICK_INTERVAL_F = 10.0f;

// How wide (in degrees) the current-reading indicator wedge is. Cosmetic
// only -- doesn't need to be pixel-precise, just visible at a glance.
static constexpr float RING_INDICATOR_WIDTH_DEG = 4.0f;

static uint16_t colorForRuuviGradientF(float tempF, float scaleLowF, float scaleHighF,
                                        float comfortLowF, float comfortHighF) {
    struct RgbStop { uint8_t r, g, b; };
    static const RgbStop ZONE_BLUE  = {40, 90, 230};
    static const RgbStop ZONE_GREEN = {40, 200, 80};
    static const RgbStop ZONE_RED   = {210, 40, 30};

    // Defensive clamp in case comfortLow/High weren't kept inside the
    // scale bounds (e.g. an old saved config edited by hand).
    float cLo = constrain(comfortLowF, scaleLowF, scaleHighF);
    float cHi = constrain(comfortHighF, cLo, scaleHighF);

    RgbStop c;
    if (tempF <= cLo) {
        float span = max(cLo - scaleLowF, 0.01f);
        float frac = constrain((tempF - scaleLowF) / span, 0.0f, 1.0f);
        c.r = static_cast<uint8_t>(ZONE_BLUE.r + (ZONE_GREEN.r - ZONE_BLUE.r) * frac);
        c.g = static_cast<uint8_t>(ZONE_BLUE.g + (ZONE_GREEN.g - ZONE_BLUE.g) * frac);
        c.b = static_cast<uint8_t>(ZONE_BLUE.b + (ZONE_GREEN.b - ZONE_BLUE.b) * frac);
    } else if (tempF >= cHi) {
        float span = max(scaleHighF - cHi, 0.01f);
        float frac = constrain((tempF - cHi) / span, 0.0f, 1.0f);
        c.r = static_cast<uint8_t>(ZONE_GREEN.r + (ZONE_RED.r - ZONE_GREEN.r) * frac);
        c.g = static_cast<uint8_t>(ZONE_GREEN.g + (ZONE_RED.g - ZONE_GREEN.g) * frac);
        c.b = static_cast<uint8_t>(ZONE_GREEN.b + (ZONE_RED.b - ZONE_GREEN.b) * frac);
    } else {
        c = ZONE_GREEN;
    }

    return (static_cast<uint16_t>(c.r >> 3) << 11) |
           (static_cast<uint16_t>(c.g >> 2) << 5) |
           (c.b >> 3);
}

void drawGradientRingAt(int16_t centerX, bool haveReading, float tempF,
                         float scaleLowF, float scaleHighF,
                         float comfortLowF, float comfortHighF,
                         uint16_t backgroundColor) {
    float span = max(scaleHighF - scaleLowF, 1.0f);

    // Band count derives from the scale width, not a fixed constant --
    // this is what makes the ring's hash marks track the user's min/max.
    // Capped at 60 as a sanity ceiling against a runaway config.
    float bandCountF = constrain(roundf(span / RING_TICK_INTERVAL_F), 1.0f, 60.0f);
    uint8_t bandCount = static_cast<uint8_t>(bandCountF);
    float bandSweep = GAUGE_ARC_SWEEP_DEG / bandCount;

    drawGaugeArcAt(centerX, GAUGE_ARC_START_DEG, GAUGE_ARC_SWEEP_DEG, backgroundColor);

    for (uint8_t i = 0; i < bandCount; i++) {
        float bandStartF = scaleLowF + span * (static_cast<float>(i) / bandCount);
        float bandEndF   = scaleLowF + span * (static_cast<float>(i + 1) / bandCount);
        uint16_t bandColor = colorForRuuviGradientF((bandStartF + bandEndF) * 0.5f,
                                                      scaleLowF, scaleHighF,
                                                      comfortLowF, comfortHighF);

        float segStart = GAUGE_ARC_START_DEG + i * bandSweep + GAUGE_SEGMENT_GAP_DEG * 0.5f;
        float segEnd = GAUGE_ARC_START_DEG + (i + 1) * bandSweep - GAUGE_SEGMENT_GAP_DEG * 0.5f;
        float visibleSweep = max(0.0f, segEnd - segStart);
        drawGaugeArcAt(centerX, segStart, visibleSweep, bandColor);
    }

    if (haveReading) {
        float frac = constrain((tempF - scaleLowF) / span, 0.0f, 1.0f); // clamps to ring ends
        float indicatorCenterDeg = GAUGE_ARC_START_DEG + frac * GAUGE_ARC_SWEEP_DEG;
        float indicatorStart = constrain(indicatorCenterDeg - RING_INDICATOR_WIDTH_DEG * 0.5f,
                                          GAUGE_ARC_START_DEG,
                                          GAUGE_ARC_START_DEG + GAUGE_ARC_SWEEP_DEG - RING_INDICATOR_WIDTH_DEG);
        drawGaugeArcAt(centerX, indicatorStart, RING_INDICATOR_WIDTH_DEG, COLOR_BLACK_SOFT);
    }
}

void drawValueGridAt(int16_t centerX, uint16_t color, int16_t verticalLengthPx) {
    gfx->drawFastVLine(centerX, 106, verticalLengthPx, color);
    gfx->drawFastHLine(centerX - 67, 144, 134, color);
}

void drawSettingsButton(uint16_t backgroundColor, uint16_t iconColor) {
    gfx->fillCircle(SETTINGS_BUTTON_X, SETTINGS_BUTTON_Y, SETTINGS_BUTTON_R + 1, backgroundColor);
    gfx->fillCircle(SETTINGS_BUTTON_X, SETTINGS_BUTTON_Y - 7, 1, iconColor);
    gfx->fillCircle(SETTINGS_BUTTON_X, SETTINGS_BUTTON_Y, 1, iconColor);
    gfx->fillCircle(SETTINGS_BUTTON_X, SETTINGS_BUTTON_Y + 7, 1, iconColor);
}

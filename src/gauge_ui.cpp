#include "gauge_ui.h"
#include "display.h"

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

uint16_t colorForRuuviTempF(float tempF, float lowF, float hiF) {
    struct RgbStop { uint8_t r, g, b; };
    static const RgbStop ZONE_GREEN      = {40, 200, 80};
    static const RgbStop ZONE_LIGHT_BLUE = {80, 180, 255};
    static const RgbStop ZONE_DARK_BLUE  = {10, 20, 140};
    static const RgbStop ZONE_LIGHT_RED  = {255, 140, 90};
    static const RgbStop ZONE_DARK_RED   = {180, 20, 20};

    // Degrees past lowF/hiF at which the color reaches full blue/red
    // saturation. A reading exactly at the threshold starts at the
    // light end; readings at or beyond the margin clamp to the dark end.
    constexpr float TEMP_ZONE_MARGIN_F = 15.0f;

    RgbStop c;
    if (tempF < lowF) {
        float depth = constrain((lowF - tempF) / TEMP_ZONE_MARGIN_F, 0.0f, 1.0f);
        c.r = static_cast<uint8_t>(ZONE_LIGHT_BLUE.r + (ZONE_DARK_BLUE.r - ZONE_LIGHT_BLUE.r) * depth);
        c.g = static_cast<uint8_t>(ZONE_LIGHT_BLUE.g + (ZONE_DARK_BLUE.g - ZONE_LIGHT_BLUE.g) * depth);
        c.b = static_cast<uint8_t>(ZONE_LIGHT_BLUE.b + (ZONE_DARK_BLUE.b - ZONE_LIGHT_BLUE.b) * depth);
    } else if (tempF > hiF) {
        float depth = constrain((tempF - hiF) / TEMP_ZONE_MARGIN_F, 0.0f, 1.0f);
        c.r = static_cast<uint8_t>(ZONE_LIGHT_RED.r + (ZONE_DARK_RED.r - ZONE_LIGHT_RED.r) * depth);
        c.g = static_cast<uint8_t>(ZONE_LIGHT_RED.g + (ZONE_DARK_RED.g - ZONE_LIGHT_RED.g) * depth);
        c.b = static_cast<uint8_t>(ZONE_LIGHT_RED.b + (ZONE_DARK_RED.b - ZONE_LIGHT_RED.b) * depth);
    } else {
        c = ZONE_GREEN;
    }

    return (static_cast<uint16_t>(c.r >> 3) << 11) |
           (static_cast<uint16_t>(c.g >> 2) << 5) |
           (c.b >> 3);
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

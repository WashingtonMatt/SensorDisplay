#pragma once
#include "config.h"
#include "aa_font.h"

// Ported from the previous ChargeScreen fork's rendering primitives — the
// anti-aliased font blending, segmented ring gauge, and color palette that
// make up its visual style. AA_FONT_LARGE/AA_FONT_SMALL (aa_font.h) are
// generated bitmap font data, copied verbatim rather than reconstructed.
//
// IMPORTANT: drawAaText()/drawAaCentered() alpha-blend each glyph pixel
// against a fixed backgroundColor parameter, not whatever's actually
// already on screen. Always fillScreen() (or fillCircle, for the gauge
// center) with that same color immediately before drawing AA text over
// it, or the anti-aliased edges show a visible fringe. The old fork
// hardcoded this blend background to COLOR_PANEL_BLUE everywhere,
// including its black-background RuuviTag page, which left a faint
// fringe there — this version takes backgroundColor as a parameter
// instead so each page can pass its actual background and avoid that.

// --- Colors (RGB565), ported from the previous fork -----------------------
static constexpr uint16_t COLOR_PANEL_BLUE     = 0x0129;
static constexpr uint16_t COLOR_BLACK_SOFT     = 0x0000;
static constexpr uint16_t COLOR_OUTDOOR_TEAL   = 0x0985; // RuuviTag page background
static constexpr uint16_t COLOR_RING_TRACK     = 0x1268;
static constexpr uint16_t COLOR_RING_GREEN     = 0x55E8;
static constexpr uint16_t COLOR_DIM_TEXT       = 0xBDF7;
static constexpr uint16_t COLOR_SOLAR_PANEL    = 0x2A20;
static constexpr uint16_t COLOR_SOLAR_TRACK    = 0x5B23;
static constexpr uint16_t COLOR_SOLAR_ORANGE   = 0xEC83;
static constexpr uint16_t COLOR_SOLAR_TEXT     = 0xFFBD;
static constexpr uint16_t COLOR_SOLAR_DIM_TEXT = 0xD6AB;
static constexpr uint16_t COLOR_NIGHT_RED      = 0xB8C1; // Night Mode page: glow red (night-vision style)
static constexpr uint16_t COLOR_NIGHT_RED_DIM  = 0x3820; // Night Mode page: dim track/label red

// --- Gauge ring geometry, ported from the previous fork --------------------
static constexpr int16_t GAUGE_CENTER_X = 120;
static constexpr int16_t GAUGE_CENTER_Y = 120;
static constexpr int16_t GAUGE_OUTER_RADIUS = 120;
static constexpr int16_t GAUGE_INNER_RADIUS = 104;
static constexpr int16_t GAUGE_CLEAR_RADIUS = 101;
static constexpr float GAUGE_ARC_START_DEG = 128.0f;
static constexpr float GAUGE_ARC_SWEEP_DEG = 284.0f;
static constexpr uint8_t GAUGE_SEGMENTS = 8;
static constexpr float GAUGE_SEGMENT_GAP_DEG = 2.2f;

// --- Anti-aliased text ------------------------------------------------
int16_t aaTextWidth(const AaFont &font, const String &text);
void drawAaText(const AaFont &font, const String &text, int16_t x, int16_t y,
                 uint16_t color, uint16_t backgroundColor = COLOR_PANEL_BLUE);
void drawAaCentered(const AaFont &font, const String &text, int16_t centerX, int16_t topY,
                     uint16_t color, uint16_t backgroundColor = COLOR_PANEL_BLUE);

// Plain (non-AA) small all-caps caption, used under each value (e.g.
// "BATTERY", "CURRENT").
void drawTinyLabel(const String &label, int16_t centerX, int16_t y, uint16_t color);

// --- Segmented ring gauge -------------------------------------------------
void drawGaugeArcAt(int16_t centerX, float startDeg, float sweepDeg, uint16_t color);
void drawSegmentedGaugeArcAt(int16_t centerX, float fillSweepDeg,
                              uint16_t trackColor, uint16_t fillColor,
                              uint16_t backgroundColor = COLOR_PANEL_BLUE);

// --- RuuviTag gradient ring ---------------------------------------------
// Continuous blue -> green -> red gradient spanning [scaleLowF, scaleHighF],
// rendered as many thin no-gap slices (see RING_GRADIENT_SLICE_DEG in
// gauge_ui.cpp) -- fine enough to read as smooth rather than stepped.
// The green "comfort" plateau sits between [comfortLowF, comfortHighF]
// (independently configurable per RuuviTag -- wide for outdoor, tight for
// a fridge). Always fully lit, regardless of whether there's a current
// reading. A short white wedge marks the current reading's position;
// pass haveReading=false to hide it (e.g. no signal yet). Readings
// outside [scaleLowF, scaleHighF] clamp the wedge to the nearest ring
// end rather than disappearing.
void drawGradientRingAt(int16_t centerX, bool haveReading, float tempF,
                         float scaleLowF, float scaleHighF,
                         float comfortLowF, float comfortHighF,
                         uint16_t backgroundColor = COLOR_PANEL_BLUE);

// --- Value grid --------------------------------------------------------
static constexpr uint16_t COLOR_VALUE_GRID = 0x6B4D;

// Divides the value cells with a cross. Ported from the old fork's
// drawBatteryValueGridAt() (there, shunt-page only, gated by a "Show
// value grid" setting that defaulted on). Generalized here to take a
// centerX parameter so any of the three gauge pages can use it, plus a
// verticalLengthPx so pages whose bottom row is a single centered value
// (not two side-by-side values, like RuuviTag's humidity) can stop the
// vertical line at the horizontal crossbar instead of running it
// through that value. Default (72px) matches the old fork's shunt-page
// behavior, where the bottom row IS two side-by-side values.
// Gate calls on DisplayConfig::showValueGrid (config.h). Draw this right
// after the gauge's center clear-circle and before the value text, same
// order as the old fork, so the lines sit under the text rather than
// over it.
void drawValueGridAt(int16_t centerX, uint16_t color = COLOR_VALUE_GRID, int16_t verticalLengthPx = 72);

// --- Settings-portal entry button --------------------------------------
// Small "..." dots-in-a-circle tap target at the bottom of every page,
// ported from the old fork's drawSettingsButton(). Tapping within
// SETTINGS_BUTTON_R + 8 px opens/closes the settings portal — main.cpp's
// loop() does the hit-testing against these same constants after a
// TouchEvent::tapped from touch.h.
static constexpr int16_t SETTINGS_BUTTON_X = 120;
static constexpr int16_t SETTINGS_BUTTON_Y = 224;
static constexpr int16_t SETTINGS_BUTTON_R = 14;
static constexpr int16_t SETTINGS_BUTTON_TAP_MARGIN = 8;

void drawSettingsButton(uint16_t backgroundColor, uint16_t iconColor);

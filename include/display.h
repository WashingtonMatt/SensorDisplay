#pragma once
#include <Arduino_GFX_Library.h>
#include "config.h"

// Ported directly from the previous ChargeScreen fork (fix/display-flicker
// branch) — confirmed working on hardware, not reimplemented. Only the
// backlight dimming model changed: the old fork used 3 discrete levels
// (Low/Mid/Max), this uses a 0-100% duty since that's what DisplayConfig
// (config.h) stores. See dimmingDutyForPct() in display.cpp if you'd
// rather revert to discrete levels.

// The global canvas handle the rest of the app draws to. Always draw to
// `gfx`, then call gfx->flush() to blit to the physical panel — never
// draw to output_display directly, or you lose the double-buffer fix.
extern Arduino_GFX *gfx;

// Initializes SPI bus, panel, and the offscreen canvas. Call once from
// setup(), before any drawing. Returns false if either the panel or the
// canvas failed to initialize (check Serial for which one).
bool displayInit();

// Only the physical panel rotates — the canvas itself must stay at
// rotation 0, since Arduino_Canvas::flush() blits its framebuffer to
// output_display unrotated. Rotating the canvas too would double-apply
// the rotation. This handles that correctly; don't call
// gfx->setRotation() elsewhere.
void displaySetRotation(uint8_t rotationQuarterTurns);

// Backlight PWM. Call backlightInit() once from setup() (after
// displayInit()), then backlightSetDutyPct() whenever DisplayConfig
// changes, or backlightSetAwake(false) to go fully dark without losing
// the configured brightness level.
void backlightInit();
void backlightSetDutyPct(uint8_t pct); // 0-100
void backlightSetAwake(bool awake);

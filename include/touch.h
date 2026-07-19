#pragma once
#include <Arduino.h>

struct TouchPoint {
    bool touched = false;
    int16_t x = 0;
    int16_t y = 0;
};

struct TouchEvent {
    bool swiped = false;
    int8_t swipeDirection = 0; // -1 = left ("next page"), +1 = right ("previous page")
    bool tapped = false;       // short touch with minimal movement
    int16_t tapX = 0;
    int16_t tapY = 0;

    // Reported on every poll *while* a touch is in progress (unlike
    // swiped/tapped, which only fire once on release), once it's been
    // held roughly stationary for longer than a normal tap. Intended for
    // press-and-hold fast-step controls (e.g. the Hour/Minute clock-set
    // rows) -- a hold that gets released never also fires `tapped`, so
    // callers don't need to guard against a double-step.
    bool held = false;
    uint32_t heldDurationMs = 0;
    int16_t heldX = 0;
    int16_t heldY = 0;
};

// Initializes the I2C bus (Wire) and the CST816 touch controller.
// Call once from setup(), after displayInit()/displaySetRotation().
void touchInit();

// Coordinate rotation must track the panel rotation, or touch points
// land in the wrong place relative to what's drawn. Call whenever
// displaySetRotation() is called with the same value.
void touchSetRotation(uint8_t rotationQuarterTurns);

// Polls the touch controller once and reports what happened since the
// last call: a completed horizontal swipe, OR a completed short tap
// (small movement, short duration) with its coordinates — never both.
// Call once per loop() iteration. Use event.tapped + tapX/tapY for
// hit-testing tap targets like the settings button (gauge_ui.h).
//
// Ported from the previous fork's updateTouchSwipe(), with one
// deliberate simplification: the screen-wake debounce logic (requiring
// N stable touch samples before waking from a dimmed/sleeping screen)
// was dropped, since display sleep/dimming isn't wired up in this fork
// yet (DisplayConfig.timeoutS exists in config.h but nothing acts on it
// yet). If a sleep timeout gets implemented later, that debounce should
// come back — otherwise the touch that wakes the screen can double as a
// swipe or tap.
TouchEvent touchPoll();

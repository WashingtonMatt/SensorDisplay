#pragma once
#include <Arduino.h>

struct TouchPoint {
    bool touched = false;
    int16_t x = 0;
    int16_t y = 0;
};

// Initializes the I2C bus (Wire) and the CST816 touch controller.
// Call once from setup(), after displayInit()/displaySetRotation().
void touchInit();

// Coordinate rotation must track the panel rotation, or touch points
// land in the wrong place relative to what's drawn. Call whenever
// displaySetRotation() is called with the same value.
void touchSetRotation(uint8_t rotationQuarterTurns);

// Polls the touch controller once. Returns true and sets *direction to
// -1 (swipe left, e.g. "next page") or +1 (swipe right, "previous page")
// if a horizontal swipe gesture completed since the last call. Returns
// false otherwise (no swipe, in-progress touch, or a tap). Call every
// loop() iteration.
//
// Ported from the previous fork's updateTouchSwipe(), with one
// deliberate simplification: the screen-wake debounce logic (requiring
// N stable touch samples before waking from a dimmed/sleeping screen)
// was dropped, since display sleep/dimming isn't wired up in this fork
// yet (DisplayConfig.timeoutS exists in config.h but nothing acts on it
// yet). If a sleep timeout gets implemented later, that debounce should
// come back — otherwise the touch that wakes the screen can double as a
// page-swipe.
bool touchPollForSwipe(int8_t *direction);

#pragma once
#include <Arduino.h>

// Simple wall-clock tracker for the Night Mode page. This board has no
// RTC, so "now" is just an anchor (minutes-since-midnight + the millis()
// value it was captured at) that gets projected forward on every read.
// The anchor can be set two ways -- the settings portal auto-pushing the
// phone's local time on every page load, or manual Hour/Minute/AM-PM
// adjustment in the on-device settings menu -- and whichever set it most
// recently simply wins. There's no separate "priority" concept, just
// last-write-wins on the same anchor, so opening the portal will always
// resync to your phone's exact time even after a manual nudge.
//
// Deliberately clock-only, no date/calendar: Night Mode only ever
// displays H:MM AM/PM, so there's nothing else worth the complexity
// (and timezone/epoch pitfalls) of full date handling.

// True once anything has set the clock (portal push or manual
// adjustment). Before that, the Night Mode page shows "--:--" rather
// than a placeholder that looks like a real (wrong) time.
bool clockIsSet();

// Sets the anchor directly. minutesSinceMidnight is wrapped to 0-1439.
// Used by the portal auto-sync route, converted from the phone's local
// time.
void clockSetMinutesSinceMidnight(int32_t minutesSinceMidnight);

// Nudges the anchor by +/- deltaMinutes, wrapping through midnight in
// both directions. Used by the on-device Hour/Minute/AM-PM rows: Hour
// passes +/-60, Minute passes +/-1, AM/PM passes +/-720. If the clock
// has never been set, starts from midnight rather than adjusting
// undefined state.
void clockAdjustMinutes(int16_t deltaMinutes);

// Projects the anchor forward by elapsed millis() and returns the
// current minutes-since-midnight (0-1439). Returns 0 if clockIsSet() is
// false -- callers displaying to the user should check clockIsSet()
// first rather than trusting this alone.
uint16_t clockGetMinutesSinceMidnight();

// Convenience formatter matching drawPlaceholderClock()'s three-part
// split (hour string / zero-padded minute string / "AM" or "PM"). Safe
// to call even when clockIsSet() is false -- fills all three with "--".
void clockFormat12Hour(char *hourOut, size_t hourOutLen, char *minuteOut, size_t minuteOutLen,
                        char *ampmOut, size_t ampmOutLen);

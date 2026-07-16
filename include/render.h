#pragma once
#include "pages.h"
#include "config.h"

// Draws the given page using the latest data in latestReadings (readings.h).
// Functional first pass: plain text + a simple color-graded arc gauge for
// RuuviTag pages. This is not the polished round-gauge/AA-font UI from the
// previous fork (drawAaText/drawGaugeArcAt and friends) — that rendering
// system is tightly coupled to that codebase's globals and custom font
// data, and porting it faithfully is its own follow-up task. This gets
// real sensor data on screen now; visual polish can layer on top later
// without changing the page-list/BLE-decode/storage plumbing underneath.
void renderPage(const PageEntry &page, const AppConfig &cfg);

#pragma once
#include "pages.h"
#include "config.h"

// Draws the given page using the latest data in latestReadings (readings.h).
// Uses the AA-font/segmented-ring-gauge visuals ported from the previous
// fork (gauge_ui.h) — see render.cpp's per-page comments for the specific
// adaptations made where this fork's data model doesn't carry the same
// fields as the old one (e.g. no running observed hi/lo for RuuviTag).
void renderPage(const PageEntry &page, const AppConfig &cfg);

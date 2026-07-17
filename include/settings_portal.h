#pragma once
#include "config.h"

// Simplified settings portal.
//
// Dropped vs. the old ChargeScreen portal:
//   - No captive DNS / auto-popup (DNSServer, wildcard redirect). User
//     joins the AP and manually enters a fixed IP in a browser.
//   - No WiFi firmware upload (Update library). Reflash over USB instead.
//
// Kept:
//   - ~1KB chunked page streaming via sendContent() (avoids single-String
//     heap allocation under WiFi AP + BLE + canvas double-buffer pressure)
//   - Heap safety floor: close portal gracefully if maxAlloc drops below
//     ~7000 bytes while active
//   - ~8s restart cooldown after stopping, before allowing AP restart
//   - BLE scan paused while portal AP is active, resumed on close
//   - Background JS status polling counts as portal activity for the
//     idle timeout (don't let it self-close mid-session)
//   - Task watchdog stays fed during long chunked sends
//
// Routes: RuuviTag slot add/edit/remove, Victron shunt/MPPT key save/
// clear, and display settings (rotation/dimming/timeout/value grid).

// Starts the AP + web server, operating on (and mutating) `cfg` for the
// life of the portal session. No-ops (returns false) if within the
// post-stop cooldown window. Pauses BLE scanning on success and draws a
// simple "connect to..." info screen.
bool settingsPortalStart(AppConfig &cfg);

// Stops the AP + web server, resumes BLE scanning, and starts the
// restart cooldown window. Does NOT redraw the normal page — the
// caller (main.cpp) is responsible for that after this returns.
void settingsPortalStop();

// Call every loop() iteration while the portal is active — services
// the web server, checks heap floor / idle timeout, feeds watchdog.
void settingsPortalLoop();

bool settingsPortalIsActive();

// Registers a callback invoked after any settings save completes
// (RuuviTag, Victron, or display settings), so main.cpp can refresh its
// page list and reapply live display settings (rotation/backlight/touch
// rotation) without settings_portal.cpp needing direct access to those
// main.cpp statics. Call once from setup(), before settingsPortalStart()
// is ever called.
void settingsPortalSetOnSave(void (*callback)());

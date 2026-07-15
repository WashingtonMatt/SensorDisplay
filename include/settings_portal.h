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

// Starts the AP + web server. No-ops (returns false) if within the
// post-stop cooldown window. Pauses BLE scanning on success.
bool settingsPortalStart();

// Stops the AP + web server, resumes BLE scanning, and starts the
// restart cooldown window.
void settingsPortalStop();

// Call every loop() iteration while the portal is active — services
// the web server, checks heap floor / idle timeout, feeds watchdog.
void settingsPortalLoop();

bool settingsPortalIsActive();

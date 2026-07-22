#pragma once
#include "config.h"

// Initializes NimBLE and starts scanning. `cfg` is stored by reference
// (not copied) — the scan callback reads from it live on every
// advertisement, so it will automatically pick up changes made via the
// settings portal without needing to be reinitialized, as long as the
// same AppConfig instance is passed in from main.cpp for the life of
// the program.
//
// Also doubles as the "reinit" half of the settings portal's full
// deinit/reinit cycle (see bleScanDeinit()) -- safe to call again after
// a deinit, since it rebuilds the scan object and its callbacks from
// scratch rather than assuming anything survived.
void bleScanInit(const AppConfig &cfg);

// Fully releases the NimBLE/BLE-controller resources (not just a scan
// stop) -- call before the settings portal brings WiFi AP mode up, and
// call bleScanInit() again after the portal closes to bring scanning
// back. This replaced an earlier pause()/resume() pair (stop-the-scan
// only) that wasn't enough to prevent a
// "wifi:timeout when WiFi un-init, type=4" coexistence error -- that
// error showed up on both the WiFi-up and WiFi-down transitions even
// with settle delays, meaning the two radio stacks contending for the
// same 2.4GHz radio during a transition was the real problem, not
// timing. Fully deiniting BLE first means there's no second radio user
// for WiFi to negotiate against during its own mode change.
void bleScanDeinit();

#pragma once
#include <Arduino.h>

// Hardware task watchdog. Kept even in the simplified (non-captive-DNS)
// portal because of a real gap in the vendored arduino-esp32 library:
// NetworkServer::accept() does not set SO_RCVTIMEO/SO_SNDTIMEO on
// accepted client sockets, so a stalled client can hang
// WebServer::handleClient() indefinitely at the kernel level.
// setTimeout() at the Arduino layer does not help. The watchdog turns
// that permanent freeze into a brief auto-recovering reboot instead.
//
// NOTE: the pinned core version (3.20017.241212) uses the *older*
// esp_task_wdt_init(uint32_t timeout_s, bool panic) signature, not the
// esp_task_wdt_config_t-based API from current ESP-IDF master. Don't
// copy watchdog init code from newer examples without checking this.

static constexpr uint32_t WATCHDOG_TIMEOUT_S = 10;

// Initializes the watchdog and subscribes the current (loop) task.
// Call once from setup().
void watchdogInit();

// Feeds the watchdog. Call once per loop() iteration, and anywhere else
// long-running but known-safe work happens (e.g. inside the chunked
// portal render loop between chunks).
void watchdogFeed();

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

static constexpr uint32_t WATCHDOG_TIMEOUT_S = 25;
// Was 10. The ESP32 WebServer library's own handleClient() has a
// documented worst case of HTTP_MAX_DATA_WAIT (5000ms) waiting on a
// slow/stalled client, on top of whatever parsing and page generation
// takes -- all on the same loop task the watchdog is tracking. A 10s
// budget left very little margin for that, especially with something
// like a phone's captive-portal-detection probes potentially producing
// odd/stalled requests (see the "content length is zero" WebServer
// warnings observed during a crash capture). 25s gives real headroom
// above that documented worst case while still recovering from a
// genuine infinite hang within a reasonable time on an unattended device.

// Initializes the watchdog and subscribes the current (loop) task.
// Call once from setup().
void watchdogInit();

// Feeds the watchdog. Call once per loop() iteration, and anywhere else
// long-running but known-safe work happens (e.g. inside the chunked
// portal render loop between chunks).
void watchdogFeed();

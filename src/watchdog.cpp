#include "watchdog.h"
#include <esp_task_wdt.h>
#include <Arduino.h>

void watchdogInit() {
    // Older signature: esp_task_wdt_init(uint32_t timeout_s, bool panic)
    esp_task_wdt_init(WATCHDOG_TIMEOUT_S, /*panic=*/true);
    esp_task_wdt_add(NULL); // subscribe the current (loop) task
}

void watchdogFeed() {
    esp_task_wdt_reset();
}

#include "settings_portal.h"
#include "watchdog.h"
#include <WiFi.h>
#include <WebServer.h>
#include <esp_heap_caps.h>

static constexpr uint32_t HEAP_FLOOR_BYTES = 7000;
static constexpr uint32_t RESTART_COOLDOWN_MS = 8000;
static constexpr uint32_t IDLE_TIMEOUT_MS = 120000; // adjust to taste

static WebServer server(80);
static bool portalActive = false;
static uint32_t lastStopMs = 0;
static uint32_t lastActivityMs = 0;

// Forward decls — actual route handlers are TODO, this file only wires
// up the safety-critical scaffolding (heap floor, cooldown, BLE pause,
// watchdog feeding) so the dedicated portal-building session can focus
// on RuuviTag add/edit/remove + Victron key + display-setting routes
// without re-deriving the landmine mitigations.
static void registerRoutes();
static void handleStatusPing(); // background JS polling endpoint — must count as activity

bool settingsPortalStart() {
    uint32_t now = millis();
    if (!portalActive && (now - lastStopMs) < RESTART_COOLDOWN_MS) {
        Serial.println("[portal] start rejected: within restart cooldown");
        return false;
    }

    // TODO: pause BLE scan here (radio contention mitigation — one shared
    // 2.4GHz radio on ESP32-C3). e.g. bleStopScan();

    WiFi.mode(WIFI_AP);
    WiFi.softAP("SensorDisplay-Setup" /* TODO: consider a password */);

    registerRoutes();
    server.on("/status", handleStatusPing);
    server.begin();

    portalActive = true;
    lastActivityMs = now;
    Serial.print("[portal] started, AP IP: ");
    Serial.println(WiFi.softAPIP());
    return true;
}

void settingsPortalStop() {
    server.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);

    // TODO: resume BLE scan here. e.g. bleStartScan();

    portalActive = false;
    lastStopMs = millis();
    Serial.println("[portal] stopped");
}

void settingsPortalLoop() {
    if (!portalActive) return;

    server.handleClient();
    watchdogFeed();

    uint32_t freeBlock = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    if (freeBlock < HEAP_FLOOR_BYTES) {
        Serial.printf("[portal] heap floor breached (%u bytes free), closing portal\n", freeBlock);
        settingsPortalStop();
        return;
    }

    if ((millis() - lastActivityMs) > IDLE_TIMEOUT_MS) {
        Serial.println("[portal] idle timeout, closing portal");
        settingsPortalStop();
    }
}

bool settingsPortalIsActive() {
    return portalActive;
}

static void handleStatusPing() {
    lastActivityMs = millis(); // background polling counts as activity
    server.send(200, "application/json", "{\"ok\":true}");
}

static void registerRoutes() {
    // TODO (dedicated portal session):
    //   GET  /                    -> chunked-stream landing page (sendContent, ~1KB chunks)
    //   GET  /ruuvi                -> list current RuuviTag slots
    //   POST /ruuvi/add            -> add tag: label + MAC text input + lo/hi range
    //   POST /ruuvi/remove         -> remove by slot index
    //   POST /ruuvi/edit           -> edit label/MAC/range for existing slot
    //   GET  /victron               -> show/set shunt + MPPT MAC/AES key
    //   POST /victron               -> save Victron keys
    //   GET  /display                -> rotation/dimming/timeout form
    //   POST /display                -> save display settings
    //
    // Every handler that renders HTML must use sendContent() in ~1KB
    // chunks rather than building one large String, and must call
    // lastActivityMs = millis() on entry.
}

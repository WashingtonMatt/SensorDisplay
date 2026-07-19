#include "settings_portal.h"
#include "storage.h"
#include "watchdog.h"
#include "ble_scan.h"
#include "display.h"
#include "gauge_ui.h"
#include "mac_utils.h"
#include "diag_log.h"
#include "clock.h"
#include <WiFi.h>
#include <WebServer.h>
#include <esp_heap_caps.h>
#include <string.h>

static constexpr uint32_t HEAP_FLOOR_BYTES = 7000;
static constexpr uint32_t RESTART_COOLDOWN_MS = 8000;
static constexpr uint32_t IDLE_TIMEOUT_MS = 120000;

static WebServer server(80);
static bool portalActive = false;
static uint32_t lastStopMs = 0;
static uint32_t lastActivityMs = 0;
static AppConfig *cfgPtr = nullptr;
static void (*onSaveCallback)() = nullptr;

static void noteActivity() {
    lastActivityMs = millis();
}

// Accumulates HTML into ~900-byte chunks and streams them via
// sendContent(), avoiding the single-large-String heap allocation that
// caused fragmentation problems under WiFi AP + BLE + canvas
// double-buffer memory pressure in the old fork. Also feeds the
// watchdog on every flush, since a page built across many small add()
// calls can take a while on a slow client connection.
class HtmlChunker {
public:
    explicit HtmlChunker(WebServer &srv) : server_(srv) {}

    void begin() {
        server_.setContentLength(CONTENT_LENGTH_UNKNOWN);
        server_.send(200, "text/html", "");
    }

    void add(const String &s) {
        buffer_ += s;
        if (buffer_.length() >= 900) flush();
    }

    void flush() {
        if (buffer_.length() > 0) {
            server_.sendContent(buffer_);
            buffer_ = "";
        }
        watchdogFeed();
    }

private:
    WebServer &server_;
    String buffer_;
};

static void sendHtmlHeader(HtmlChunker &out, const String &title) {
    out.add("<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
            "<title>");
    out.add(title);
    out.add("</title><style>"
            "body{font-family:sans-serif;background:#111;color:#eee;margin:0;padding:16px}"
            "h1{font-size:20px}h3{margin:0 0 8px}"
            "a{color:#8cf}"
            "nav{margin-bottom:14px}nav a{margin-right:14px}"
            "label{display:block;margin-top:10px;font-size:13px;color:#aaa}"
            "input,select{width:100%;box-sizing:border-box;padding:6px;margin-top:4px;"
            "background:#222;color:#eee;border:1px solid #444;border-radius:4px}"
            "button{margin-top:14px;padding:8px 14px;background:#357;color:#fff;border:none;border-radius:4px}"
            ".card{background:#1a1a1a;border-radius:8px;padding:12px;margin-bottom:12px}"
            ".danger{background:#733;margin-left:8px}"
            ".hint{color:#888;font-size:12px}"
            "</style></head><body>");
    out.add("<nav><a href='/'>Home</a> <a href='/ruuvi'>RuuviTags</a> "
            "<a href='/victron'>Victron</a> <a href='/display'>Display</a> "
            "<a href='/debug'>Debug Log</a></nav>");
    out.add("<h1>");
    out.add(title);
    out.add("</h1>");
}

static void sendHtmlFooter(HtmlChunker &out) {
    // Fires on every portal page load (not just the first), so a
    // multi-page browsing session keeps resyncing rather than only
    // catching the clock once. Fire-and-forget: doesn't block the page,
    // and a failed request just means the clock stays whatever it was.
    // Sends the phone's LOCAL time -- no timezone/UTC math needed here
    // or on the device, since the phone is already showing local time
    // for wherever the RV is parked.
    out.add("<script>"
            "(function(){var d=new Date();var m=d.getHours()*60+d.getMinutes();"
            "fetch('/time/sync',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},"
            "body:'minutes='+m}).catch(function(){});})();"
            "</script>");
    out.add("</body></html>");
    out.flush();
}

static void saveAndNotify() {
    storageSaveAll(*cfgPtr);
    if (onSaveCallback) onSaveCallback();
}

// --- Root ------------------------------------------------------------------
static void handleRoot() {
    noteActivity();
    HtmlChunker out(server);
    out.begin();
    sendHtmlHeader(out, "SensorDisplay Setup");
    out.add("<p>Connected as AP <b>SensorDisplay-Setup</b>, browsing at <b>");
    out.add(WiFi.softAPIP().toString());
    out.add("</b></p>");
    out.add("<div class='card'><a href='/ruuvi'>RuuviTags</a> &mdash; add, edit, or remove up to 4 tags</div>");
    out.add("<div class='card'><a href='/victron'>Victron Devices</a> &mdash; SmartShunt / MPPT pairing</div>");
    out.add("<div class='card'><a href='/display'>Display Settings</a> &mdash; rotation, brightness, value grid</div>");
    out.add("<div class='card'><a href='/debug'>Debug Log</a> &mdash; diagnostic lines from BLE scanning</div>");
    sendHtmlFooter(out);
}

// --- RuuviTags ---------------------------------------------------------
static void handleRuuviGet() {
    noteActivity();
    HtmlChunker out(server);
    out.begin();
    sendHtmlHeader(out, "RuuviTags");

    for (uint8_t i = 0; i < MAX_RUUVITAGS; i++) {
        RuuviTagConfig &tag = cfgPtr->ruuviTags[i];
        out.add("<div class='card'><h3>Slot ");
        out.add(String(i + 1));
        out.add(tag.configured ? " (configured)" : " (empty)");
        out.add("</h3><form method='POST' action='/ruuvi/save'>");
        out.add("<input type='hidden' name='slot' value='");
        out.add(String(i));
        out.add("'><label>Label</label><input name='label' maxlength='15' value='");
        out.add(tag.configured ? String(tag.label) : "RuuviTag");
        out.add("'><label>MAC Address</label><input name='mac' value='");
        out.add(tag.configured ? macToString(tag.mac) : "");
        out.add("' placeholder='AA:BB:CC:DD:EE:FF'>");
        out.add("<label>Low Temp (F)</label><input name='lo' type='number' step='0.1' value='");
        out.add(String(tag.configured ? tag.lowTempF : 32.0f, 1));
        out.add("'><label>High Temp (F)</label><input name='hi' type='number' step='0.1' value='");
        out.add(String(tag.configured ? tag.hiTempF : 90.0f, 1));
        out.add("'><p class='hint'>Low/High Temp define the green \"comfort\" band on the ring "
                "-- keep it wide for outdoor, tight for a fridge.</p>");
        out.add("<label>Ring Scale Low (F)</label><input name='scale_lo' type='number' step='0.1' value='");
        out.add(String(tag.configured ? tag.scaleLowF : -10.0f, 1));
        out.add("'><label>Ring Scale High (F)</label><input name='scale_hi' type='number' step='0.1' value='");
        out.add(String(tag.configured ? tag.scaleHighF : 110.0f, 1));
        out.add("'><p class='hint'>Ring Scale Low/High are the endpoints of the ring's blue-to-red "
                "gradient -- the full range this tag's dial covers.</p>");
        out.add("<button type='submit'>Save</button>");
        out.flush();

        if (tag.configured) {
            out.add("</form><form method='POST' action='/ruuvi/remove' style='display:inline'>");
            out.add("<input type='hidden' name='slot' value='");
            out.add(String(i));
            out.add("'><button class='danger' type='submit'>Remove</button></form></div>");
        } else {
            out.add("</form></div>");
        }
        out.flush();
    }

    sendHtmlFooter(out);
}

static void handleRuuviSave() {
    noteActivity();
    int slot = server.arg("slot").toInt();
    if (slot < 0 || slot >= MAX_RUUVITAGS) {
        server.sendHeader("Location", "/ruuvi");
        server.send(303);
        return;
    }

    uint8_t mac[6];
    if (!macFromString(server.arg("mac"), mac)) {
        // Malformed MAC -- bounce back without saving anything for this
        // slot. A future pass could show an inline error instead.
        server.sendHeader("Location", "/ruuvi");
        server.send(303);
        return;
    }

    String label = server.arg("label");
    label.trim();
    if (label.length() == 0) label = "RuuviTag";

    RuuviTagConfig &tag = cfgPtr->ruuviTags[slot];
    strncpy(tag.label, label.c_str(), LABEL_MAX_LEN - 1);
    tag.label[LABEL_MAX_LEN - 1] = '\0';
    memcpy(tag.mac, mac, 6);
    tag.lowTempF = server.arg("lo").toFloat();
    tag.hiTempF = server.arg("hi").toFloat();

    float scaleLo = server.arg("scale_lo").toFloat();
    float scaleHi = server.arg("scale_hi").toFloat();
    if (scaleHi <= scaleLo) {
        // Guard against a misconfigured/empty scale collapsing the ring
        // gradient math to a single point -- fall back to the defaults
        // rather than saving something that'll divide by zero at render
        // time.
        scaleLo = -10.0f;
        scaleHi = 110.0f;
    }
    tag.scaleLowF = scaleLo;
    tag.scaleHighF = scaleHi;

    tag.configured = true;

    saveAndNotify();
    server.sendHeader("Location", "/ruuvi");
    server.send(303);
}

static void handleRuuviRemove() {
    noteActivity();
    int slot = server.arg("slot").toInt();
    if (slot >= 0 && slot < MAX_RUUVITAGS) {
        cfgPtr->ruuviTags[slot] = RuuviTagConfig{};
        saveAndNotify();
    }
    server.sendHeader("Location", "/ruuvi");
    server.send(303);
}

// --- Victron -------------------------------------------------------------
static void handleVictronGet() {
    noteActivity();
    HtmlChunker out(server);
    out.begin();
    sendHtmlHeader(out, "Victron Devices");

    VictronConfig &v = cfgPtr->victron;

    out.add("<div class='card'><h3>SmartShunt");
    out.add(v.shuntConfigured ? " (configured)" : " (empty)");
    out.add("</h3><form method='POST' action='/victron/shunt/save'>");
    out.add("<label>MAC Address</label><input name='mac' value='");
    out.add(v.shuntConfigured ? macToString(v.shuntMac) : "");
    out.add("' placeholder='AA:BB:CC:DD:EE:FF'>");
    out.add("<label>AES Encryption Key (32 hex chars)</label>"
            "<input name='key' maxlength='32' placeholder='leave blank to keep current key'>");
    out.add("<button type='submit'>Save</button>");
    out.flush();
    if (v.shuntConfigured) {
        out.add("</form><form method='POST' action='/victron/shunt/clear' style='display:inline'>"
                "<button class='danger' type='submit'>Remove</button></form></div>");
    } else {
        out.add("</form></div>");
    }
    out.flush();

    out.add("<div class='card'><h3>MPPT Solar Charger");
    out.add(v.mpptConfigured ? " (configured)" : " (empty)");
    out.add("</h3><form method='POST' action='/victron/mppt/save'>");
    out.add("<label>MAC Address</label><input name='mac' value='");
    out.add(v.mpptConfigured ? macToString(v.mpptMac) : "");
    out.add("' placeholder='AA:BB:CC:DD:EE:FF'>");
    out.add("<label>AES Encryption Key (32 hex chars)</label>"
            "<input name='key' maxlength='32' placeholder='leave blank to keep current key'>");
    out.add("<button type='submit'>Save</button>");
    out.flush();
    if (v.mpptConfigured) {
        out.add("</form><form method='POST' action='/victron/mppt/clear' style='display:inline'>"
                "<button class='danger' type='submit'>Remove</button></form></div>");
    } else {
        out.add("</form></div>");
    }

    out.add("<p class='hint'>Find the AES key in the Victron app: tap the device, "
            "the gear icon, then the Bluetooth pairing/encryption key screen. "
            "It's the same key used for Instant Readout decoding.</p>");

    out.add("<div class='card'><h3>Solar Panel Capacity</h3>"
            "<form method='POST' action='/victron/mppt/capacity/save'>"
            "<label>Array Capacity (W)</label><input name='capacity' type='number' step='1' value='");
    out.add(String(static_cast<int>(v.solarCapacityW)));
    out.add("'><p class='hint'>Sets the PV Power ring's fill ceiling on the MPPT page. "
            "Independent of pairing -- set this whether or not a device is paired above.</p>"
            "<button type='submit'>Save</button></form></div>");

    sendHtmlFooter(out);
}

static void handleVictronSaveShunt() {
    noteActivity();
    VictronConfig &v = cfgPtr->victron;

    uint8_t mac[6];
    if (!macFromString(server.arg("mac"), mac)) {
        server.sendHeader("Location", "/victron");
        server.send(303);
        return;
    }
    memcpy(v.shuntMac, mac, 6);

    String keyHex = server.arg("key");
    keyHex.trim();
    if (keyHex.length() > 0) {
        uint8_t key[16];
        if (hexToBytes(keyHex, key, 16)) {
            memcpy(v.shuntAesKey, key, 16);
        }
        // Malformed key hex: silently keep whatever key was already
        // stored rather than blocking the MAC save. Good enough for a
        // single-user local tool; a stricter version would show an error.
    }
    v.shuntConfigured = true;

    saveAndNotify();
    server.sendHeader("Location", "/victron");
    server.send(303);
}

static void handleVictronClearShunt() {
    noteActivity();
    cfgPtr->victron.shuntConfigured = false;
    memset(cfgPtr->victron.shuntMac, 0, 6);
    memset(cfgPtr->victron.shuntAesKey, 0, 16);
    saveAndNotify();
    server.sendHeader("Location", "/victron");
    server.send(303);
}

static void handleVictronSaveMppt() {
    noteActivity();
    VictronConfig &v = cfgPtr->victron;

    uint8_t mac[6];
    if (!macFromString(server.arg("mac"), mac)) {
        server.sendHeader("Location", "/victron");
        server.send(303);
        return;
    }
    memcpy(v.mpptMac, mac, 6);

    String keyHex = server.arg("key");
    keyHex.trim();
    if (keyHex.length() > 0) {
        uint8_t key[16];
        if (hexToBytes(keyHex, key, 16)) {
            memcpy(v.mpptAesKey, key, 16);
        }
    }
    v.mpptConfigured = true;

    saveAndNotify();
    server.sendHeader("Location", "/victron");
    server.send(303);
}

static void handleVictronClearMppt() {
    noteActivity();
    cfgPtr->victron.mpptConfigured = false;
    memset(cfgPtr->victron.mpptMac, 0, 6);
    memset(cfgPtr->victron.mpptAesKey, 0, 16);
    saveAndNotify();
    server.sendHeader("Location", "/victron");
    server.send(303);
}

static void handleVictronSaveCapacity() {
    noteActivity();
    float capacity = server.arg("capacity").toFloat();
    if (capacity > 0.0f) {
        cfgPtr->victron.solarCapacityW = capacity;
        saveAndNotify();
    }
    server.sendHeader("Location", "/victron");
    server.send(303);
}

// --- Display settings ------------------------------------------------
static void handleDisplayGet() {
    noteActivity();
    HtmlChunker out(server);
    out.begin();
    sendHtmlHeader(out, "Display Settings");

    DisplayConfig &d = cfgPtr->display;
    static const char *rotationLabels[4] = {"0 (normal)", "90 clockwise", "180", "270 clockwise"};

    out.add("<div class='card'><form method='POST' action='/display/save'>");
    out.add("<label>Rotation</label><select name='rotation'>");
    for (uint8_t i = 0; i < 4; i++) {
        out.add("<option value='");
        out.add(String(i));
        out.add("'");
        if (d.rotation == i) out.add(" selected");
        out.add(">");
        out.add(rotationLabels[i]);
        out.add("</option>");
    }
    out.add("</select>");

    out.add("<label>Whole Device Brightness (%)</label><input name='dimming' type='number' min='0' max='100' value='");
    out.add(String(d.dimmingPct));
    out.add("'>");

    out.add("<label>Night Mode Page Brightness (%, 0 = use device brightness)</label>"
            "<input name='night_dimming' type='number' min='0' max='100' value='");
    out.add(String(d.nightModeDimmingPct));
    out.add("'>");

    out.add("<label><input type='checkbox' name='grid' value='1'");
    if (d.showValueGrid) out.add(" checked");
    out.add(" style='width:auto;display:inline-block'> Show value grid</label>");

    out.add("<label>Sleep after (seconds, 0 = never) &mdash; normal pages</label>"
            "<input name='timeout' type='number' min='0' value='");
    out.add(String(d.timeoutS));
    out.add("'>");

    out.add("<label>Sleep after (seconds, 0 = never) &mdash; Night Mode page</label>"
            "<input name='night_timeout' type='number' min='0' value='");
    out.add(String(d.nightModeTimeoutS));
    out.add("'>");

    out.add("<button type='submit'>Save</button></form></div>");
    out.add("<p class='hint'>All settings here apply immediately, no reboot needed. "
            "Sleep turns the backlight off after that many seconds with no touch -- "
            "the screen keeps rendering underneath, and the next touch just wakes it "
            "back up without also acting as a swipe or tap. The Night Mode page has "
            "its own separate sleep timer and brightness (0 = follow the whole-device "
            "brightness instead), so it can be dimmer/brighter and stay on longer or "
            "shorter than normal pages, independently.</p>");

    sendHtmlFooter(out);
}

static void handleDisplaySave() {
    noteActivity();
    DisplayConfig &d = cfgPtr->display;

    int rotation = server.arg("rotation").toInt();
    if (rotation >= 0 && rotation <= 3) {
        d.rotation = static_cast<uint8_t>(rotation);
    }

    int dimming = server.arg("dimming").toInt();
    d.dimmingPct = static_cast<uint8_t>(constrain(dimming, 0, 100));

    int nightDimming = server.arg("night_dimming").toInt();
    d.nightModeDimmingPct = static_cast<uint8_t>(constrain(nightDimming, 0, 100));

    d.showValueGrid = server.hasArg("grid");

    int timeout = server.arg("timeout").toInt();
    d.timeoutS = static_cast<uint16_t>(constrain(timeout, 0, 65535));

    int nightTimeout = server.arg("night_timeout").toInt();
    d.nightModeTimeoutS = static_cast<uint16_t>(constrain(nightTimeout, 0, 65535));

    saveAndNotify();
    server.sendHeader("Location", "/display");
    server.send(303);
}

// --- Debug log -----------------------------------------------------------
static void handleDebugGet() {
    noteActivity();
    HtmlChunker out(server);
    out.begin();
    sendHtmlHeader(out, "Debug Log");

    out.add("<p class='hint'>Diagnostic lines captured while BLE scanning was active "
            "(scanning pauses while this portal is open, so this only shows what "
            "happened before you opened it). Most recent at the bottom.</p>");
    out.add("<pre style='white-space:pre-wrap;background:#1a1a1a;padding:10px;"
            "border-radius:6px;font-size:12px;line-height:1.5'>");

    String log = diagLogGetAll();
    out.add(log.length() > 0 ? log : "(nothing captured yet -- close this page, "
            "stay in range for a minute or two, then reopen)");

    out.add("</pre>");
    sendHtmlFooter(out);
}

// --- Status ping (background activity keepalive) -----------------------
static void handleStatusPing() {
    noteActivity();
    server.send(200, "application/json", "{\"ok\":true}");
}

// --- Time sync (phone's local clock, pushed by the JS in sendHtmlFooter) ---
static void handleTimeSync() {
    noteActivity();
    if (server.hasArg("minutes")) {
        int minutes = server.arg("minutes").toInt();
        if (minutes >= 0 && minutes < 1440) {
            clockSetMinutesSinceMidnight(minutes);
        }
    }
    server.send(200, "application/json", "{\"ok\":true}");
}

static void registerRoutes() {
    server.on("/", HTTP_GET, handleRoot);
    server.on("/status", HTTP_GET, handleStatusPing);
    server.on("/time/sync", HTTP_POST, handleTimeSync);

    server.on("/ruuvi", HTTP_GET, handleRuuviGet);
    server.on("/ruuvi/save", HTTP_POST, handleRuuviSave);
    server.on("/ruuvi/remove", HTTP_POST, handleRuuviRemove);

    server.on("/victron", HTTP_GET, handleVictronGet);
    server.on("/victron/shunt/save", HTTP_POST, handleVictronSaveShunt);
    server.on("/victron/shunt/clear", HTTP_POST, handleVictronClearShunt);
    server.on("/victron/mppt/save", HTTP_POST, handleVictronSaveMppt);
    server.on("/victron/mppt/clear", HTTP_POST, handleVictronClearMppt);
    server.on("/victron/mppt/capacity/save", HTTP_POST, handleVictronSaveCapacity);

    server.on("/display", HTTP_GET, handleDisplayGet);
    server.on("/display/save", HTTP_POST, handleDisplaySave);

    server.on("/debug", HTTP_GET, handleDebugGet);
}

static void drawPortalInfoScreen() {
    gfx->fillScreen(BLACK);
    gfx->setTextColor(WHITE);
    gfx->setTextSize(2);
    gfx->setCursor(30, 40);
    gfx->print("Settings Mode");

    gfx->setTextSize(1);
    gfx->setCursor(20, 80);
    gfx->print("Join WiFi:");
    gfx->setCursor(20, 92);
    gfx->print("SensorDisplay-Setup");

    gfx->setCursor(20, 116);
    gfx->print("Browse to:");
    gfx->setCursor(20, 128);
    gfx->print(WiFi.softAPIP().toString());

    gfx->setCursor(20, 170);
    gfx->print("Tap dots below to exit");

    drawSettingsButton(BLACK, WHITE);
    gfx->flush();
}

bool settingsPortalStart(AppConfig &cfg) {
    uint32_t now = millis();
    if (!portalActive && (now - lastStopMs) < RESTART_COOLDOWN_MS) {
        Serial.println("[portal] start rejected: within restart cooldown");
        return false;
    }

    cfgPtr = &cfg;

    bleScanPause(); // radio contention mitigation — one shared 2.4GHz radio on ESP32-C3

    WiFi.mode(WIFI_AP);
    WiFi.softAP("SensorDisplay-Setup");

    registerRoutes();
    server.begin();

    portalActive = true;
    lastActivityMs = now;

    drawPortalInfoScreen();

    Serial.print("[portal] started, AP IP: ");
    Serial.println(WiFi.softAPIP());
    return true;
}

void settingsPortalStop() {
    server.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);

    bleScanResume();

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

void settingsPortalSetOnSave(void (*callback)()) {
    onSaveCallback = callback;
}

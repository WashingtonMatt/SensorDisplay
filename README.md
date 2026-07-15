# SensorDisplay

Personal ESP32-C3 battery/environment dashboard. Clean-break fork of
[ChargeScreen](https://github.com/ricopicouk/ChargeScreen) (previously
forked at github.com/WashingtonMatt/ChargeScreen), scoped tightly to my
own RV hardware — no more upstream compatibility or generic-device
support.

## Hardware

- ESP32-C3 (single core, single shared 2.4GHz radio — WiFi and BLE contend)
- Round GC9A01 display, SPI, `Arduino_Canvas` offscreen double-buffer
- PlatformIO, `esp32-c3-devkitm-1`, `arduino` framework
- Testing device: iPad (iPadOS)

## Devices supported

- Victron SmartShunt
- Victron MPPT 100/30
- Up to 4 RuuviTags (RAWv2), each with MAC pairing, a label, and a
  user-defined low/high temp range for gauge coloring

**Core UI rule:** a device's page only appears on-screen if that device
is configured. The page list is built at runtime (`pages.cpp`), never
fixed — applies to Victron pages and RuuviTag pages alike.

## Repo layout

```
include/
  config.h           AppConfig struct, NVS keys, device limits
  storage.h
  pages.h            PageType/PageEntry, buildPageList()
  watchdog.h         task watchdog (old esp_task_wdt_init signature — see notes)
  ble_decoders.h      Victron + RuuviTag decoder interfaces
  settings_portal.h
src/
  main.cpp           setup()/loop() skeleton
  storage.cpp         NVS load/save, RuuviTag JSON blob (ArduinoJson)
  pages.cpp
  watchdog.cpp
  ble_decoders.cpp    STUBS — port real decode logic from previous fork
  settings_portal.cpp guardrails wired, HTTP routes are TODO
platformio.ini
```

## Status: scaffolding only

This is the initial skeleton. Nothing here has been flashed or tested
yet. Roadmap, in the order agreed on:

1. ~~Repo scaffolding + platformio.ini + project skeleton~~ (this commit)
2. Multi-RuuviTag NVS storage — `storage.cpp` has a first pass (JSON
   blob via ArduinoJson); needs testing on hardware
3. Dynamic page-list logic — `pages.cpp` has a first pass; needs
   wiring into actual rendering once the display driver is ported in
4. Simplified settings portal — guardrails (heap floor, cooldown,
   BLE pause, watchdog feed, activity-tracking `/status` endpoint) are
   wired in `settings_portal.cpp`; the actual HTML routes
   (RuuviTag add/edit/remove, Victron keys, display settings) are
   still TODO and are their own session
5. Port the display driver + `Arduino_Canvas` double buffer, Victron
   BLE decoder, and RuuviTag RAWv2 decoder from the previous fork
   (these are "known-good, keep as-is" — not reimplementing from
   scratch)
6. Wire NimBLE init/scan + touch input + actual page rendering into
   `main.cpp` (currently all TODO stubs)

## Known landmines (carried forward from the previous fork's debugging)

See comments in `watchdog.h` and `settings_portal.h` for the specifics,
but in short:

- `NetworkServer::accept()` in this arduino-esp32 version doesn't set
  socket timeouts on accepted clients — a stalled client can hang
  `handleClient()` forever at the kernel level. The task watchdog
  (10s, panic-reboot) is the mitigation, not a nice-to-have.
- Build settings-portal HTML in ~1KB chunks via `sendContent()`, never
  as one large `String` — heap fragmentation risk under WiFi AP + BLE +
  canvas double-buffer memory pressure.
- Pause BLE scanning while the portal AP is active (shared radio).
- ~8s cooldown before restarting the portal after a stop, to avoid a
  heap-fragmentation / WiFi-driver-error spiral from rapid retoggle.
- Background `/status` JS polling must reset the idle-timeout clock,
  or the portal can close itself mid-session while still being viewed.

## Build

```
pio run                # build
pio run -t upload      # flash over USB, COM4 @ 115200
pio device monitor      # symbolized backtraces via esp32_exception_decoder
```

#pragma once
#include "config.h"

// Loads all persisted config (Victron keys, RuuviTag array, display settings)
// from NVS into `out`. Any field not yet set in NVS is left at its
// AppConfig default (see config.h).
void storageLoadAll(AppConfig &out);

// Persists the full AppConfig back to NVS. Call after any settings-portal
// save action. This is a full-namespace write, not a per-field diff —
// keep it off the hot path (only call on explicit save, not per-loop).
void storageSaveAll(const AppConfig &cfg);

// --- RuuviTag blob specifically -------------------------------------------
// Stored as a single JSON array under NVS_KEY_RUUVI_BLOB, one object per
// configured slot: {"label":"Outdoor","mac":"AA:BB:CC:DD:EE:FF","lo":32,"hi":90}
// Empty/unconfigured slots are simply omitted from the array, not written
// as null entries.
//
// TODO: if you were migrating from the old single-tag scalar NVS keys
// (pre-fork), that one-time migration/cutover code would live here. Since
// this is a clean-break repo with no upstream compatibility requirement,
// current plan is a clean cutover — no migration path implemented.
bool storageLoadRuuviTags(RuuviTagConfig (&tags)[MAX_RUUVITAGS]);
bool storageSaveRuuviTags(const RuuviTagConfig (&tags)[MAX_RUUVITAGS]);

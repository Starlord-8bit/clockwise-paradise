#pragma once

/**
 * CWClockfaceDriver — v3 flat function-pointer clockface API
 *
 * Inspired by WLED's effect registration pattern. No classes, no vtables,
 * no heap allocation on switch. All 6 clockfaces compile in simultaneously;
 * each exposes a CWClockfaceDriver struct with plain function pointers.
 *
 * ## Switching (instant, no reboot, no heap alloc):
 *   const CWClockfaceDriver* next = CWDriverRegistry::get(index);
 *   if (current && current->teardown) current->teardown();
 *   current = next;
 *   current->setup(display, dateTime);
 *
 * ## Adding a new clockface:
 *   1. Implement cf_XXX_setup / cf_XXX_update (and optionally cf_XXX_teardown)
 *   2. Define a CWClockfaceDriver cf_XXX = { "Name", INDEX, setup, update, teardown }
 *   3. Add &cf_XXX to CWDriverRegistry::REGISTRY[]
 *
 * ## Clockface contract (what each driver must implement):
 *   - setup(): initialise state, set display pointer, set datetime pointer
 *   - update(): called every loop iteration while this face is active
 *   - teardown() [optional]: release resources (set ptrs to null, etc.)
 *
 * NOTE: setup() is called once per activation, NOT once per boot.
 *       Drivers must handle being called setup() → teardown() → setup() repeatedly.
 */

#include <Adafruit_GFX.h>
#include "CWDateTime.h"

// ─── Driver descriptor ───────────────────────────────────────────────────────

struct CWClockfaceDriver {
    const char* name;    // human-readable name shown in WebUI
    uint8_t     index;   // 0-based index matching WebUI selector

    void (*setup)   (Adafruit_GFX* display, CWDateTime* dateTime);
    void (*update)  ();
    void (*teardown)();  // may be nullptr — called before switching away
};


// ─── Forward declarations for all built-in drivers ──────────────────────────
// These are defined in the cw-cf-0xNN submodule driver files.

extern CWClockfaceDriver cf_mario;       // index 0 — cw-cf-0x01
extern CWClockfaceDriver cf_words;       // index 1 — cw-cf-0x02
extern CWClockfaceDriver cf_worldmap;    // index 2 — cw-cf-0x03
extern CWClockfaceDriver cf_castlevania; // index 3 — cw-cf-0x04
extern CWClockfaceDriver cf_pacman;      // index 4 — cw-cf-0x05
extern CWClockfaceDriver cf_pokedex;     // index 5 — cw-cf-0x06


// ─── Registry ────────────────────────────────────────────────────────────────

struct CWDriverRegistry {

    static constexpr uint8_t COUNT = 6;

    static const CWClockfaceDriver* REGISTRY[COUNT];

    /**
     * Get driver by index. Returns nullptr for out-of-range index.
     */
    static const CWClockfaceDriver* get(uint8_t index) {
        if (index >= COUNT) {
            Serial.printf("[Registry] Unknown clockface index %d\n", index);
            return nullptr;
        }
        return REGISTRY[index];
    }

    /**
     * Switch to a new clockface. Tears down the current driver (if any),
     * sets up the new one.
     *
     * @param current   Pointer to current driver pointer (updated in-place)
     * @param index     Target clockface index
     * @param display   HUB75 display pointer
     * @param dateTime  CWDateTime pointer
     * @return          true on success, false if index invalid (current unchanged)
     */
    static bool switchTo(const CWClockfaceDriver** current, uint8_t index,
                         Adafruit_GFX* display, CWDateTime* dateTime) {
        const CWClockfaceDriver* next = get(index);
        if (!next) return false;

        // Tear down current if any
        if (*current && (*current)->teardown) {
            (*current)->teardown();
        }

        *current = next;
        next->setup(display, dateTime);

        Serial.printf("[Registry] Switched to '%s' (index %d)\n", next->name, index);
        return true;
    }
};

// Registry definition — in CWClockfaceDriver.cpp
// (declared here, defined once in a .cpp to avoid multiple-definition errors)

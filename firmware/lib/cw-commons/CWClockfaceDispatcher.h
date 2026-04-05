#pragma once

/**
 * CWClockfaceDispatcher — runtime clockface switching
 *
 * All 6 clockface implementations are compiled in simultaneously.
 * Each lives in its own namespace (CF01–CF06) to avoid symbol collisions.
 *
 * Usage:
 *   IClockface* face = CWClockfaceDispatcher::create(index, display);
 *   face->setup(&cwDateTime);
 *   // ... in loop:
 *   face->update();
 *
 * Index mapping (0-based, matches WebUI selector):
 *   0 = CF01 Super Mario
 *   1 = CF02 Time in Words
 *   2 = CF03 World Map
 *   3 = CF04 Castlevania
 *   4 = CF05 Pac-Man
 *   5 = CF06 Pokedex
 *   6 = CF07 Canvas (handled separately via canvasFile/canvasServer)
 *
 * NOTE: Clockface submodule headers are wrapped in namespaces by the
 * Starlord-8bit forks of each cw-cf-0xNN repo. If you see compile errors
 * about redefined class Clockface, ensure the correct fork URLs are used
 * in .gitmodules and CMakeLists.txt.
 */

#include <Adafruit_GFX.h>
#include <IClockface.h>

// Include each namespaced clockface (component name = cw-cf-0xNN)
// Files are named Clockface0N.h to avoid flat include path collisions in ESP-IDF
#include <Clockface01.h>
#include <Clockface02.h>
#include <Clockface03.h>
#include <Clockface04.h>
#include <Clockface05.h>
#include <Clockface06.h>
// CF07 (Canvas) included separately — it needs canvasFile/canvasServer params

struct CWClockfaceDispatcher {

    /**
     * Create a new clockface by index (0-based).
     * Caller takes ownership and must delete the previous instance first.
     * Returns nullptr for unknown index (caller should keep the existing face).
     */
    static IClockface* create(uint8_t index, Adafruit_GFX* display) {
        switch (index) {
            case 0: return new CF01::Clockface(display);
            case 1: return new CF02::Clockface(display);
            case 2: return new CF03::Clockface(display);
            case 3: return new CF04::Clockface(display);
            case 4: return new CF05::Clockface(display);
            case 5: return new CF06::Clockface(display);
            // case 6: Canvas — handled by main.cpp (uses canvasFile/canvasServer)
            default:
                Serial.printf("[Dispatcher] Unknown clockface index %d, keeping current\n", index);
                return nullptr;
        }
    }

    /**
     * Switch to a new clockface at runtime.
     * Deletes the old instance, creates the new one, and calls setup().
     * Returns the new face pointer (or old if index is invalid).
     *
     * @param current   Pointer to current clockface (will be deleted on success)
     * @param index     New clockface index (0-based)
     * @param display   HUB75 display pointer
     * @param dateTime  CWDateTime pointer for setup()
     * @return          New IClockface* (or original current on failure)
     */
    static IClockface* switchTo(IClockface* current, uint8_t index,
                                 Adafruit_GFX* display, CWDateTime* dateTime) {
        IClockface* next = create(index, display);
        if (!next) return current;

        // Tear down old
        delete current;

        // Set up new
        next->setup(dateTime);
        Serial.printf("[Dispatcher] Switched to clockface index %d\n", index);
        return next;
    }

    static const char* name(uint8_t index) {
        switch (index) {
            case 0: return "Super Mario";
            case 1: return "Time in Words";
            case 2: return "World Map";
            case 3: return "Castlevania";
            case 4: return "Pac-Man";
            case 5: return "Pokedex";
            case 6: return "Canvas";
            default: return "Unknown";
        }
    }
};

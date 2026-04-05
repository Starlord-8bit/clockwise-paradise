// CWClockfaceRegistry.cpp — lives in main/ (links last, sees all components)
// Do not put this in cw-commons — the cf_XXX symbols are in cw-cf-0xNN components.

#include "CWClockfaceDriver.h"

const CWClockfaceDriver* CWDriverRegistry::REGISTRY[CWDriverRegistry::COUNT] = {
    &cf_mario,        // 0
    &cf_words,        // 1
    &cf_worldmap,     // 2
    &cf_castlevania,  // 3
    &cf_pacman,       // 4
    &cf_pokedex,      // 5
};

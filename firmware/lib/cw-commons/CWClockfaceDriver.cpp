#include "CWClockfaceDriver.h"

// Registry array — order must match index values in each driver
const CWClockfaceDriver* CWDriverRegistry::REGISTRY[CWDriverRegistry::COUNT] = {
    &cf_mario,        // 0
    &cf_words,        // 1
    &cf_worldmap,     // 2
    &cf_castlevania,  // 3
    &cf_pacman,       // 4
    &cf_pokedex,      // 5
};

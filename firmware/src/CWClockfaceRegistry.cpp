#include <CWClockfaceDriver.h>

// Registry definition must live in main/src so clockface symbols resolve at link time.

const CWClockfaceDriver* CWDriverRegistry::REGISTRY[CWDriverRegistry::COUNT] = {
    &cf_mario,
    &cf_words,
    &cf_worldmap,
    &cf_castlevania,
    &cf_pacman,
    &cf_pokedex,
};

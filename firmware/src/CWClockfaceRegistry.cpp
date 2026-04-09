#include <CWClockfaceDriver.h>

// Definition of the static registry array declared in CWClockfaceDriver.h.
// Must live here (in main/src) so all clockface component symbols are visible
// to the linker at this point. A missing definition causes REGISTRY to resolve
// to address 0x0, producing a LoadProhibited crash immediately after DMA setup.

const CWClockfaceDriver* CWDriverRegistry::REGISTRY[CWDriverRegistry::COUNT] = {
    &cf_mario,       // index 0 — cw-cf-0x01
    &cf_words,       // index 1 — cw-cf-0x02
    &cf_worldmap,    // index 2 — cw-cf-0x03
    &cf_castlevania, // index 3 — cw-cf-0x04
    &cf_pacman,      // index 4 — cw-cf-0x05
    &cf_pokedex,     // index 5 — cw-cf-0x06
};

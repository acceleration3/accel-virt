/* Puts a WireArena into states no call can, so a test can prove that its
 * invariant notices each one. */
#ifndef AV_WIRE_ARENA_PROBE_HPP
#define AV_WIRE_ARENA_PROBE_HPP

#include <cstddef>

namespace av {

class WireArena;

struct WireArenaProbe {
    explicit WireArenaProbe(WireArena &arena);

    /* More used than there is room for. */
    void set_used(std::size_t used);

    WireArena &arena_;
};

}  // namespace av

#endif  // AV_WIRE_ARENA_PROBE_HPP

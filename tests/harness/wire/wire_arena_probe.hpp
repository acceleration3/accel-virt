#ifndef AV_WIRE_ARENA_PROBE_HPP
#define AV_WIRE_ARENA_PROBE_HPP

#include <cstddef>

namespace av {

class WireArena;

struct WireArenaProbe {
    explicit WireArenaProbe(WireArena &arena);

    void set_used(std::size_t used);

    WireArena &arena_;
};

}

#endif

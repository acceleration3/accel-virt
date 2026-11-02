#include "harness/wire/wire_arena_probe.hpp"

#include "proto/wire/wire_arena.hpp"

namespace av {

WireArenaProbe::WireArenaProbe(WireArena &arena) : arena_(arena) {}

void WireArenaProbe::set_used(std::size_t used) { arena_.used_ = used; }

}

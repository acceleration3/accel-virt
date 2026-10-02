#include "executor/decode_arena.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

DecodeArena::DecodeArena() : arena_(nullptr, 0u)
{
    AV_SCOPE("DecodeArena::DecodeArena");
}

void DecodeArena::attach(uint8_t *block, size_t bytes)
{
    AV_SCOPE("DecodeArena::attach");
    arena_.attach(block, bytes);
}

WireArena &DecodeArena::arena()
{
    AV_SCOPE("DecodeArena::arena");
    return arena_;
}

void DecodeArena::reset()
{
    AV_SCOPE("DecodeArena::reset");
    arena_.reset();
}

size_t DecodeArena::refusals() const
{
    AV_SCOPE("DecodeArena::refusals");
    return arena_.refusals();
}

size_t DecodeArena::capacity() const
{
    AV_SCOPE("DecodeArena::capacity");
    return arena_.capacity();
}

bool DecodeArena::invariants_hold() const
{
    AV_SCOPE("DecodeArena::invariants_hold");
    return arena_.invariants_hold();
}

}  // namespace av

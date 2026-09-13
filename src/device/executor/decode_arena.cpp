#include "device/executor/decode_arena.hpp"

namespace av {

DecodeArena::DecodeArena() : arena_(nullptr, 0u) {}

void DecodeArena::attach(uint8_t *block, size_t bytes) { arena_.attach(block, bytes); }

WireArena &DecodeArena::arena() { return arena_; }

void DecodeArena::reset() { arena_.reset(); }

size_t DecodeArena::refusals() const { return arena_.refusals(); }

size_t DecodeArena::capacity() const { return arena_.capacity(); }

bool DecodeArena::invariants_hold() const { return arena_.invariants_hold(); }

}  // namespace av

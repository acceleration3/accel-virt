#ifndef AV_ARENA_SLOTS_HPP
#define AV_ARENA_SLOTS_HPP

#include <stdint.h>

namespace av {

struct ArenaSlotsProbe;

class ArenaSlots {
public:
    ArenaSlots();
    ArenaSlots(const ArenaSlots &) = delete;
    ArenaSlots &operator=(const ArenaSlots &) = delete;

    [[nodiscard]] bool take_at(uint64_t base, uint64_t bytes);
    [[nodiscard]] bool take(uint64_t bytes, uint64_t &base);
    void               give(uint64_t base, uint64_t bytes);
    uint32_t           taken() const;

    bool invariants_hold() const;

private:
    friend struct ArenaSlotsProbe;

    uint64_t span_of(uint64_t base, uint64_t bytes) const;

    uint64_t used_;
    uint64_t starts_;
};

}

#endif

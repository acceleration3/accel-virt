#ifndef AV_DDI_TABLE_HPP
#define AV_DDI_TABLE_HPP

#include "miniport/driver/ddi_slot.hpp"
#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

struct DdiTableProbe;

using DdiRoutine = void (*)();

class DdiTable {
public:
    static constexpr uint32_t wddm_3_2        = 0x11007u;
    static constexpr uint16_t slot_count      = 193u;
    static constexpr size_t   first_routine_offset = 8u;

    DdiTable();

    [[nodiscard]] Status set(DdiSlot slot, DdiRoutine routine);
    DdiRoutine           routine(DdiSlot slot) const;
    uint32_t             version() const;

    bool invariants_hold() const;

private:
    static constexpr uint16_t routine_count = slot_count - 1u;

    bool is_routine_slot(DdiSlot slot) const;

    uint32_t   version_;
    uint32_t   padding_;
    DdiRoutine routines_[routine_count];

    friend struct DdiTableProbe;
};

}

#endif

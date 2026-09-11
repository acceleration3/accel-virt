/* The table a WDDM miniport registers with dxgkrnl: DRIVER_INITIALIZATION_DATA
 * at DXGKDDI_INTERFACE_VERSION_WDDM3_2, rebuilt from fixed-width types so core
 * can fill it and a Linux test can drive it. src/km/ddi_table_layout.cpp proves
 * at compile time that the layout is byte-for-byte the WDK's; do not add, remove
 * or reorder members. */
#ifndef AV_DDI_TABLE_HPP
#define AV_DDI_TABLE_HPP

#include "ddi_slot.hpp"
#include "status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

struct DdiTableProbe;

/* Every slot after the version is a pointer - 187 DDI routines and 5 Reserved
 * fields - and its real signature belongs to the caller. */
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

}  // namespace av

#endif  // AV_DDI_TABLE_HPP

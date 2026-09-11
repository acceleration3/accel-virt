#include "core/miniport/ddi_table.hpp"

namespace av {

DdiTable::DdiTable() : version_(wddm_3_2), padding_(0u), routines_{}
{
    static_assert(offsetof(DdiTable, routines_) == first_routine_offset,
                  "routines must start where DRIVER_INITIALIZATION_DATA's first DDI does");
}

bool DdiTable::is_routine_slot(DdiSlot slot) const
{
    const uint16_t index = static_cast<uint16_t>(slot);
    return index != 0u && index < slot_count;
}

Status DdiTable::set(DdiSlot slot, DdiRoutine routine)
{
    if (!is_routine_slot(slot)) return Status::invalid_parameter;
    routines_[static_cast<uint16_t>(slot) - 1u] = routine;
    return Status::ok;
}

DdiRoutine DdiTable::routine(DdiSlot slot) const
{
    if (!is_routine_slot(slot)) return nullptr;
    return routines_[static_cast<uint16_t>(slot) - 1u];
}

uint32_t DdiTable::version() const { return version_; }

bool DdiTable::invariants_hold() const
{
    return version_ == wddm_3_2 && padding_ == 0u;
}

}  // namespace av

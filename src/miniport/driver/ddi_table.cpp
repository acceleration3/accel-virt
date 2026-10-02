#include "miniport/driver/ddi_table.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

DdiTable::DdiTable() : version_(wddm_3_2), padding_(0u), routines_{}
{
    AV_SCOPE("DdiTable::DdiTable");
    static_assert(offsetof(DdiTable, routines_) == first_routine_offset,
                  "routines must start where DRIVER_INITIALIZATION_DATA's first DDI does");
}

bool DdiTable::is_routine_slot(DdiSlot slot) const
{
    AV_SCOPE("DdiTable::is_routine_slot");
    const uint16_t index = static_cast<uint16_t>(slot);
    return index != 0u && index < slot_count;
}

Status DdiTable::set(DdiSlot slot, DdiRoutine routine)
{
    AV_SCOPE("DdiTable::set");
    if (!is_routine_slot(slot)) return Status::invalid_parameter;
    routines_[static_cast<uint16_t>(slot) - 1u] = routine;
    return Status::ok;
}

DdiRoutine DdiTable::routine(DdiSlot slot) const
{
    AV_SCOPE("DdiTable::routine");
    if (!is_routine_slot(slot)) return nullptr;
    return routines_[static_cast<uint16_t>(slot) - 1u];
}

uint32_t DdiTable::version() const
{
    AV_SCOPE("DdiTable::version");
    return version_;
}

bool DdiTable::invariants_hold() const
{
    AV_SCOPE("DdiTable::invariants_hold");
    return version_ == wddm_3_2 && padding_ == 0u;
}

}  // namespace av

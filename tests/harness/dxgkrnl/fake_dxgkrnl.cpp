#include "harness/dxgkrnl/fake_dxgkrnl.hpp"

namespace av {

namespace {
const DdiSlot required_slots[] = {
    DdiSlot::DxgkDdiAddDevice,         DdiSlot::DxgkDdiStartDevice,
    DdiSlot::DxgkDdiStopDevice,        DdiSlot::DxgkDdiRemoveDevice,
    DdiSlot::DxgkDdiDispatchIoRequest, DdiSlot::DxgkDdiInterruptRoutine,
    DdiSlot::DxgkDdiDpcRoutine,        DdiSlot::DxgkDdiQueryChildRelations,
    DdiSlot::DxgkDdiQueryChildStatus,  DdiSlot::DxgkDdiQueryDeviceDescriptor,
    DdiSlot::DxgkDdiSetPowerState,     DdiSlot::DxgkDdiResetDevice,
    DdiSlot::DxgkDdiUnload,
};

}

FakeDxgkrnl::FakeDxgkrnl()
    : table_(nullptr), failed_(RegistrationRule::none), missing_(DdiSlot::Version) {}

Status FakeDxgkrnl::refuse(RegistrationRule rule, Status status)
{
    failed_ = rule;
    return status;
}

bool FakeDxgkrnl::has_required_slots(const DdiTable &table)
{
    for (const DdiSlot slot : required_slots) {
        const bool present = table.routine(slot) != nullptr ||
                             (slot == DdiSlot::DxgkDdiQueryChildStatus &&
                              table.routine(DdiSlot::DxgkDdiQueryConnectionChange) != nullptr);
        if (!present) {
            missing_ = slot;
            return false;
        }
    }
    return true;
}

Status FakeDxgkrnl::initialize(const DdiTable *table)
{
    missing_ = DdiSlot::Version;
    if (table == nullptr) return refuse(RegistrationRule::no_table, Status::invalid_parameter);
    if (table->version() < oldest_accepted_version)
        return refuse(RegistrationRule::version_too_old, Status::not_supported);
    if (!has_required_slots(*table))
        return refuse(RegistrationRule::required_slot_missing, Status::not_supported);
    if (table_ != nullptr) return refuse(RegistrationRule::already_registered, Status::conflict);
    table_ = table;
    failed_ = RegistrationRule::none;
    return Status::ok;
}

RegistrationRule FakeDxgkrnl::failed_rule() const { return failed_; }
DdiSlot          FakeDxgkrnl::missing_slot() const { return missing_; }
const DdiTable  *FakeDxgkrnl::registered_table() const { return table_; }

}

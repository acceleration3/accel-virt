#include "miniport.hpp"

#include "ddi/entry_points.hpp"

namespace av {

Miniport::Miniport(Platform &platform)
    : platform_(platform), table_(), table_status_(0u), adapters_{}
{
    struct Entry {
        DdiSlot    slot;
        DdiRoutine routine;
    };
    const Entry entries[] = {
        {DdiSlot::DxgkDdiAddDevice, reinterpret_cast<DdiRoutine>(&AvDdiAddDevice)},
        {DdiSlot::DxgkDdiStartDevice, reinterpret_cast<DdiRoutine>(&AvDdiStartDevice)},
        {DdiSlot::DxgkDdiStopDevice, reinterpret_cast<DdiRoutine>(&AvDdiStopDevice)},
        {DdiSlot::DxgkDdiRemoveDevice, reinterpret_cast<DdiRoutine>(&AvDdiRemoveDevice)},
        {DdiSlot::DxgkDdiDispatchIoRequest, reinterpret_cast<DdiRoutine>(&AvDdiDispatchIoRequest)},
        {DdiSlot::DxgkDdiInterruptRoutine, reinterpret_cast<DdiRoutine>(&AvDdiInterruptRoutine)},
        {DdiSlot::DxgkDdiDpcRoutine, reinterpret_cast<DdiRoutine>(&AvDdiDpcRoutine)},
        {DdiSlot::DxgkDdiQueryChildRelations, reinterpret_cast<DdiRoutine>(&AvDdiQueryChildRelations)},
        {DdiSlot::DxgkDdiQueryChildStatus, reinterpret_cast<DdiRoutine>(&AvDdiQueryChildStatus)},
        {DdiSlot::DxgkDdiQueryDeviceDescriptor, reinterpret_cast<DdiRoutine>(&AvDdiQueryDeviceDescriptor)},
        {DdiSlot::DxgkDdiSetPowerState, reinterpret_cast<DdiRoutine>(&AvDdiSetPowerState)},
        {DdiSlot::DxgkDdiNotifyAcpiEvent, reinterpret_cast<DdiRoutine>(&AvDdiNotifyAcpiEvent)},
        {DdiSlot::DxgkDdiResetDevice, reinterpret_cast<DdiRoutine>(&AvDdiResetDevice)},
        {DdiSlot::DxgkDdiUnload, reinterpret_cast<DdiRoutine>(&AvDdiUnload)},
        {DdiSlot::DxgkDdiQueryInterface, reinterpret_cast<DdiRoutine>(&AvDdiQueryInterface)},
        {DdiSlot::DxgkDdiControlEtwLogging, reinterpret_cast<DdiRoutine>(&AvDdiControlEtwLogging)},
        {DdiSlot::DxgkDdiLinkDevice, reinterpret_cast<DdiRoutine>(&AvDdiLinkDevice)},
    };
    for (const Entry &entry : entries)
        table_status_ |= static_cast<unsigned>(table_.set(entry.slot, entry.routine));
}

const DdiTable &Miniport::ddi_table() const { return table_; }

ObjectStorage<Adapter> *Miniport::slot_of(void *context)
{
    for (ObjectStorage<Adapter> &slot : adapters_)
        if (slot.constructed() && context == slot.address()) return &slot;
    return nullptr;
}

Status Miniport::add_device(void *physical_device_object, void *&context)
{
    platform_.require_irql_at_most(Irql::passive, "Miniport::add_device");
    context = nullptr;
    for (ObjectStorage<Adapter> &slot : adapters_) {
        if (slot.constructed()) continue;
        const Status constructed = slot.construct(platform_, physical_device_object);
        context = slot.address();
        return constructed;
    }
    platform_.log("miniport: no free adapter slot");
    return Status::no_memory;
}

Status Miniport::remove_device(void *context)
{
    platform_.require_irql_at_most(Irql::passive, "Miniport::remove_device");
    ObjectStorage<Adapter> *slot = slot_of(context);
    if (slot == nullptr) return Status::invalid_parameter;
    slot->destroy();
    return Status::ok;
}

void Miniport::unload()
{
    platform_.require_irql_at_most(Irql::passive, "Miniport::unload");
    for (ObjectStorage<Adapter> &slot : adapters_) slot.destroy();
}

uint32_t Miniport::adapter_count() const
{
    uint32_t count = 0u;
    for (const ObjectStorage<Adapter> &slot : adapters_) count += slot.constructed() ? 1u : 0u;
    return count;
}

bool Miniport::invariants_hold() const
{
    return table_status_ == 0u && table_.invariants_hold();
}

}  // namespace av

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
        {DdiSlot::DxgkDdiQueryAdapterInfo, reinterpret_cast<DdiRoutine>(&AvDdiQueryAdapterInfo)},
        {DdiSlot::DxgkDdiEscape, reinterpret_cast<DdiRoutine>(&AvDdiEscape)},
        {DdiSlot::DxgkDdiCollectDbgInfo, reinterpret_cast<DdiRoutine>(&AvDdiCollectDbgInfo)},
        {DdiSlot::DxgkDdiCreateDevice, reinterpret_cast<DdiRoutine>(&AvDdiCreateDevice)},
        {DdiSlot::DxgkDdiDestroyDevice, reinterpret_cast<DdiRoutine>(&AvDdiDestroyDevice)},
        {DdiSlot::DxgkDdiCreateAllocation, reinterpret_cast<DdiRoutine>(&AvDdiCreateAllocation)},
        {DdiSlot::DxgkDdiDestroyAllocation, reinterpret_cast<DdiRoutine>(&AvDdiDestroyAllocation)},
        {DdiSlot::DxgkDdiDescribeAllocation, reinterpret_cast<DdiRoutine>(&AvDdiDescribeAllocation)},
        {DdiSlot::DxgkDdiGetStandardAllocationDriverData,
         reinterpret_cast<DdiRoutine>(&AvDdiGetStandardAllocationDriverData)},
        {DdiSlot::DxgkDdiOpenAllocation, reinterpret_cast<DdiRoutine>(&AvDdiOpenAllocation)},
        {DdiSlot::DxgkDdiCloseAllocation, reinterpret_cast<DdiRoutine>(&AvDdiCloseAllocation)},
        {DdiSlot::DxgkDdiSetDisplayPrivateDriverFormat, reinterpret_cast<DdiRoutine>(&AvDdiSetDisplayPrivateDriverFormat)},
        {DdiSlot::DxgkDdiCreateContext, reinterpret_cast<DdiRoutine>(&AvDdiCreateContext)},
        {DdiSlot::DxgkDdiDestroyContext, reinterpret_cast<DdiRoutine>(&AvDdiDestroyContext)},
        {DdiSlot::DxgkDdiRender, reinterpret_cast<DdiRoutine>(&AvDdiRender)},
        {DdiSlot::DxgkDdiPatch, reinterpret_cast<DdiRoutine>(&AvDdiPatch)},
        {DdiSlot::DxgkDdiSubmitCommand, reinterpret_cast<DdiRoutine>(&AvDdiSubmitCommand)},
        {DdiSlot::DxgkDdiPreemptCommand, reinterpret_cast<DdiRoutine>(&AvDdiPreemptCommand)},
        {DdiSlot::DxgkDdiQueryCurrentFence, reinterpret_cast<DdiRoutine>(&AvDdiQueryCurrentFence)},
        {DdiSlot::DxgkDdiBuildPagingBuffer, reinterpret_cast<DdiRoutine>(&AvDdiBuildPagingBuffer)},
        {DdiSlot::DxgkDdiResetFromTimeout, reinterpret_cast<DdiRoutine>(&AvDdiResetFromTimeout)},
        {DdiSlot::DxgkDdiRestartFromTimeout, reinterpret_cast<DdiRoutine>(&AvDdiRestartFromTimeout)},
        {DdiSlot::DxgkDdiIsSupportedVidPn, reinterpret_cast<DdiRoutine>(&AvDdiIsSupportedVidPn)},
        {DdiSlot::DxgkDdiRecommendFunctionalVidPn, reinterpret_cast<DdiRoutine>(&AvDdiRecommendFunctionalVidPn)},
        {DdiSlot::DxgkDdiEnumVidPnCofuncModality, reinterpret_cast<DdiRoutine>(&AvDdiEnumVidPnCofuncModality)},
        {DdiSlot::DxgkDdiCommitVidPn, reinterpret_cast<DdiRoutine>(&AvDdiCommitVidPn)},
        {DdiSlot::DxgkDdiUpdateActiveVidPnPresentPath,
         reinterpret_cast<DdiRoutine>(&AvDdiUpdateActiveVidPnPresentPath)},
        {DdiSlot::DxgkDdiRecommendMonitorModes, reinterpret_cast<DdiRoutine>(&AvDdiRecommendMonitorModes)},
        {DdiSlot::DxgkDdiRecommendVidPnTopology, reinterpret_cast<DdiRoutine>(&AvDdiRecommendVidPnTopology)},
        {DdiSlot::DxgkDdiGetScanLine, reinterpret_cast<DdiRoutine>(&AvDdiGetScanLine)},
        {DdiSlot::DxgkDdiControlInterrupt, reinterpret_cast<DdiRoutine>(&AvDdiControlInterrupt)},
        {DdiSlot::DxgkDdiSetVidPnSourceAddress, reinterpret_cast<DdiRoutine>(&AvDdiSetVidPnSourceAddress)},
        {DdiSlot::DxgkDdiSetVidPnSourceVisibility, reinterpret_cast<DdiRoutine>(&AvDdiSetVidPnSourceVisibility)},
        {DdiSlot::DxgkDdiPresent, reinterpret_cast<DdiRoutine>(&AvDdiPresent)},
        {DdiSlot::DxgkDdiSetPointerShape, reinterpret_cast<DdiRoutine>(&AvDdiSetPointerShape)},
        {DdiSlot::DxgkDdiSetPointerPosition, reinterpret_cast<DdiRoutine>(&AvDdiSetPointerPosition)},
        {DdiSlot::DxgkDdiAcquireSwizzlingRange, reinterpret_cast<DdiRoutine>(&AvDdiAcquireSwizzlingRange)},
        {DdiSlot::DxgkDdiReleaseSwizzlingRange, reinterpret_cast<DdiRoutine>(&AvDdiReleaseSwizzlingRange)},
        {DdiSlot::DxgkDdiSetPalette, reinterpret_cast<DdiRoutine>(&AvDdiSetPalette)},
        {DdiSlot::DxgkDdiStopCapture, reinterpret_cast<DdiRoutine>(&AvDdiStopCapture)},
        {DdiSlot::DxgkDdiCreateOverlay, reinterpret_cast<DdiRoutine>(&AvDdiCreateOverlay)},
        {DdiSlot::DxgkDdiUpdateOverlay, reinterpret_cast<DdiRoutine>(&AvDdiUpdateOverlay)},
        {DdiSlot::DxgkDdiFlipOverlay, reinterpret_cast<DdiRoutine>(&AvDdiFlipOverlay)},
        {DdiSlot::DxgkDdiDestroyOverlay, reinterpret_cast<DdiRoutine>(&AvDdiDestroyOverlay)},
        {DdiSlot::DxgkDdiQueryVidPnHWCapability, reinterpret_cast<DdiRoutine>(&AvDdiQueryVidPnHWCapability)},
        {DdiSlot::DxgkDdiSetPowerComponentFState, reinterpret_cast<DdiRoutine>(&AvDdiSetPowerComponentFState)},
        {DdiSlot::DxgkDdiQueryDependentEngineGroup, reinterpret_cast<DdiRoutine>(&AvDdiQueryDependentEngineGroup)},
        {DdiSlot::DxgkDdiQueryEngineStatus, reinterpret_cast<DdiRoutine>(&AvDdiQueryEngineStatus)},
        {DdiSlot::DxgkDdiResetEngine, reinterpret_cast<DdiRoutine>(&AvDdiResetEngine)},
        {DdiSlot::DxgkDdiStopDeviceAndReleasePostDisplayOwnership,
         reinterpret_cast<DdiRoutine>(&AvDdiStopDeviceAndReleasePostDisplayOwnership)},
        {DdiSlot::DxgkDdiSystemDisplayEnable, reinterpret_cast<DdiRoutine>(&AvDdiSystemDisplayEnable)},
        {DdiSlot::DxgkDdiSystemDisplayWrite, reinterpret_cast<DdiRoutine>(&AvDdiSystemDisplayWrite)},
        {DdiSlot::DxgkDdiCancelCommand, reinterpret_cast<DdiRoutine>(&AvDdiCancelCommand)},
        {DdiSlot::DxgkDdiGetChildContainerId, reinterpret_cast<DdiRoutine>(&AvDdiGetChildContainerId)},
        {DdiSlot::DxgkDdiPowerRuntimeControlRequest, reinterpret_cast<DdiRoutine>(&AvDdiPowerRuntimeControlRequest)},
        {DdiSlot::DxgkDdiSetVidPnSourceAddressWithMultiPlaneOverlay,
         reinterpret_cast<DdiRoutine>(&AvDdiSetVidPnSourceAddressWithMultiPlaneOverlay)},
        {DdiSlot::DxgkDdiNotifySurpriseRemoval, reinterpret_cast<DdiRoutine>(&AvDdiNotifySurpriseRemoval)},
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

/* dxgkrnl's FDO for one adapter: drives the entry points of a registered DDI
 * table in the order and at the IRQL dxgkrnl.sys 10.0.26100.9444 uses
 * (docs/ddi-sequences.md sections 2-4), including its unwinding. Each step
 * answers the first NTSTATUS that stopped it. */
#ifndef AV_FAKE_DXGKRNL_FDO_HPP
#define AV_FAKE_DXGKRNL_FDO_HPP

#include "miniport/driver/ddi_table.hpp"
#include "harness/dxgkrnl/fake_dxgadapter.hpp"
#include "harness/dxgkrnl/fake_dxgkrnl_adapter.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

namespace av {

class FakeDxgkrnlFdo {
public:
    static constexpr std::uint32_t max_children = 4u;
    static constexpr std::uint32_t max_links = 0x100u;

    FakeDxgkrnlFdo(MockPlatform &platform, const DdiTable &table, FakeDxgkrnlAdapter &adapter);

    /* What dxgkrnl destroys before it stops the adapter, because it is adapter
     * state a process holds a handle to: its doorbells and hardware queues
     * (feature 049). Called with `context` at the top of every stop. */
    using Stopping = void (*)(void *context);
    void         on_stopping(Stopping stopping, void *context);

    /* DpiAddDevice: AddDevice, then LinkDevice; a failed LinkDevice removes. */
    std::int32_t add_device();
    /* DpiFdoStartAdapter: the interrupt is already connected, then StartDevice,
     * QueryChildRelations and DXGADAPTER::Initialize's capability queries; a
     * failure after StartDevice stops the device. */
    std::int32_t start_device();
    /* PnP enumeration of each child: QueryChildStatus, QueryDeviceDescriptor. */
    std::int32_t enumerate_children();
    /* The message routine at device IRQL; a claimed interrupt's DPC runs at DISPATCH. */
    std::uint8_t interrupt(std::uint32_t message);
    /* The DPC a DDI queued with DxgkCbQueueDpc outside the ISR, at DISPATCH. */
    void         dpc();
    std::int32_t set_power_state(std::int32_t device_power_state);
    void         reset_device();
    std::int32_t stop_device();
    /* DpiFdoHandleSurpriseRemoval with the device gone: NotifySurpriseRemoval
     * (DxgkRemovalPnPNotify), then StopDevice if started. */
    std::int32_t surprise_removal();
    /* DpiFdoHandleRemoveDevice: a started device is stopped first. */
    std::int32_t remove_device();

    FakeDxgadapter &dxgadapter();
    void        *context() const;
    bool         started() const;
    std::uint32_t sources() const;
    std::uint32_t children() const;
    std::uint32_t connected_children() const;

private:
    Stopping      stopping_ = nullptr;
    void         *stopping_context_ = nullptr;

    MockPlatform       &platform_;
    const DdiTable     &table_;
    FakeDxgkrnlAdapter &adapter_;
    FakeDxgadapter      dxgadapter_;
    void               *context_;
    bool                started_;
    std::uint32_t       sources_;
    std::uint32_t       children_;
    std::uint32_t       connected_;
};

}  // namespace av

#endif  // AV_FAKE_DXGKRNL_FDO_HPP

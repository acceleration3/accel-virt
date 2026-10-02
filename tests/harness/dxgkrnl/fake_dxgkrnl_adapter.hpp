/* dxgkrnl's side of one adapter's callbacks (DXGKRNL_INTERFACE), over a fake
 * virtio-gpu PCI function: device information with its resource list, config
 * space reads, and BAR mappings that attach the function's register regions to
 * MockPlatform - so a register access through an unmapped BAR is a violation.
 * Callbacks answer STATUS_SUCCESS unless a knob says otherwise; a mapping still
 * live at destruction fails the test. */
#ifndef AV_FAKE_DXGKRNL_ADAPTER_HPP
#define AV_FAKE_DXGKRNL_ADAPTER_HPP

#include "miniport/ddi/dxgk_device_info_record.hpp"
#include "miniport/ddi/dxgkrnl_interface_record.hpp"
#include "harness/virtio/fake_virtio_gpu_function.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

namespace av {

struct DxgkargcbQueryFeatureSupportRecord;

class FakeDxgkrnlAdapter {
public:
    static constexpr std::uint32_t max_mappings = 6u;
    static constexpr std::uint32_t max_config_bytes = FakeVirtioGpuFunction::config_bytes;

    /* `index` separates the fake kernel addresses of adapters sharing a platform. */
    FakeDxgkrnlAdapter(MockPlatform &platform, FakeVirtioGpuFunction &function, std::uint8_t index = 0u);
    ~FakeDxgkrnlAdapter();
    FakeDxgkrnlAdapter(const FakeDxgkrnlAdapter &) = delete;
    FakeDxgkrnlAdapter &operator=(const FakeDxgkrnlAdapter &) = delete;

    DxgkrnlInterfaceRecord &record();
    /* Which features this OS lets the driver turn on, and what it answers when
     * the ask itself fails (feature 049). Nothing is allowed until asked for. */
    void                    allow_feature(std::uint32_t feature_id);
    void                    refuse_feature_queries(std::uint32_t status);
    std::int32_t            query_feature_support(DxgkargcbQueryFeatureSupportRecord &request);
    std::uint32_t           feature_queries() const;

    /* Knobs: the NTSTATUS the next such callback answers. */
    void refuse_device_information(std::uint32_t status);
    void refuse_config_read(std::uint32_t status);
    void short_config_read();
    /* DXGK_DEVICE_INFO.SystemMemorySize; 8 GiB unless set. */
    void set_system_memory(std::int64_t bytes);
    /* The `nth` map from now (1-based) answers `status`. */
    void refuse_map(std::uint32_t nth, std::uint32_t status);
    /* Unmaps still release the mapping, then answer `status`. */
    void refuse_unmap(std::uint32_t status);

    std::uint32_t live_mappings() const;
    std::uint32_t maps() const;
    std::uint32_t dpcs_queued() const;
    /* DxgkCbNotifyInterrupt calls, in order; only the first max_interrupts kept. */
    static constexpr std::uint32_t max_interrupts = 64u;
    std::uint32_t                           interrupts() const;
    const DxgkargcbNotifyInterruptDataRecord &interrupt(std::uint32_t index) const;
    std::uint32_t                           dpc_notifications() const;
    /* DxgkCbIndicateChildStatus calls: how many, and each one's child and
     * connection, in order; only the first max_child_statuses kept. */
    static constexpr std::uint32_t max_child_statuses = 16u;
    std::uint32_t                           child_statuses() const;
    std::uint32_t                           child_status_uid(std::uint32_t index) const;
    bool                                    child_status_connected(std::uint32_t index) const;
    /* Every DxgkCbNotifyInterrupt from now, past the ones kept: what a caller
     * waiting on many fences reads. Null stops it. */
    using InterruptSink = void (*)(void *context, const DxgkargcbNotifyInterruptDataRecord &data);
    void                                    on_interrupt(InterruptSink sink, void *context);
    /* Every DxgkCbNotifyDpc from now: the moment dxgkrnl reads what the DPC
     * reported. Null stops it. */
    using DpcSink = void (*)(void *context);
    void                                    on_notify_dpc(DpcSink sink, void *context);
    void                                    refuse_synchronize(std::uint32_t status);

    std::int32_t get_device_information(DxgkDeviceInfoRecord *info);
    std::int32_t read_device_space(std::uint32_t space, void *buffer, std::uint32_t offset,
                                   std::uint32_t length, std::uint32_t *read);
    std::int32_t map_memory(std::int64_t address, std::uint32_t length, std::uint8_t io_space,
                            std::uint8_t user_mode, std::int32_t cache, void **mapped);
    std::int32_t unmap_memory(void *mapped);
    std::uint8_t queue_dpc();
    std::int32_t synchronize_execution(SynchronizeRoutine routine, void *context, std::uint32_t message,
                                       std::uint8_t *returned);
    void         notify_interrupt(const DxgkargcbNotifyInterruptDataRecord &data);
    void         notify_dpc();

private:
    static constexpr std::uintptr_t virtual_base = 0x7FA000000000u;
    static constexpr std::uint64_t  region_step  = 0x1000u;
    /* DXGK_INTERRUPT_TYPE */
    static constexpr std::int32_t   dma_faulted  = 4;

    struct Mapping {
        void         *address;
        std::uint8_t  bar;
        std::uint32_t length;
    };

    MockPlatform          &platform_;
    FakeVirtioGpuFunction &function_;
    DxgkrnlInterfaceRecord record_;
    std::uint32_t          device_information_status_;
    std::uint32_t          config_read_status_;
    bool                   short_config_read_;
    std::uint32_t          map_refusal_countdown_;
    std::uint32_t          map_refusal_status_;
    std::uint32_t          unmap_status_;
    /* Feature 049: which features this OS allows, one bit per feature id, and
     * how many times the driver asked. */
    std::uint64_t          allowed_features_ = 0u;
    std::uint32_t          feature_query_status_ = 0u;
    std::uint32_t          feature_queries_ = 0u;
    Mapping                mappings_[max_mappings];
    std::uint32_t          mapping_count_;
    std::uint32_t          maps_;
    std::uint32_t          dpcs_;
    std::uint8_t           index_;
    std::int64_t           system_memory_;
    DxgkargcbNotifyInterruptDataRecord interrupts_[max_interrupts];
    std::uint32_t          interrupt_count_;
    std::uint32_t          dpc_notifications_;
    std::uint32_t          child_status_count_ = 0u;
    std::uint32_t          child_status_uids_[max_child_statuses] = {};
    bool                   child_status_connected_[max_child_statuses] = {};
    InterruptSink          sink_;
    void                  *sink_context_;
    DpcSink                dpc_sink_;
    void                  *dpc_sink_context_;
    std::uint32_t          synchronize_status_;
    bool                   synchronized_;
};

}  // namespace av

#endif  // AV_FAKE_DXGKRNL_ADAPTER_HPP

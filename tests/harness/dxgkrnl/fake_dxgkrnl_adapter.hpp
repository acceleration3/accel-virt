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

    FakeDxgkrnlAdapter(MockPlatform &platform, FakeVirtioGpuFunction &function, std::uint8_t index = 0u);
    ~FakeDxgkrnlAdapter();
    FakeDxgkrnlAdapter(const FakeDxgkrnlAdapter &) = delete;
    FakeDxgkrnlAdapter &operator=(const FakeDxgkrnlAdapter &) = delete;

    DxgkrnlInterfaceRecord &record();
    void                    allow_feature(std::uint32_t feature_id);
    void                    refuse_feature_queries(std::uint32_t status);
    std::int32_t            query_feature_support(DxgkargcbQueryFeatureSupportRecord &request);
    std::uint32_t           feature_queries() const;

    void refuse_device_information(std::uint32_t status);
    void refuse_config_read(std::uint32_t status);
    void short_config_read();
    void set_system_memory(std::int64_t bytes);
    void refuse_map(std::uint32_t nth, std::uint32_t status);
    void refuse_unmap(std::uint32_t status);

    std::uint32_t live_mappings() const;
    std::uint32_t maps() const;
    std::uint32_t dpcs_queued() const;
    static constexpr std::uint32_t max_interrupts = 64u;
    std::uint32_t                           interrupts() const;
    const DxgkargcbNotifyInterruptDataRecord &interrupt(std::uint32_t index) const;
    std::uint32_t                           dpc_notifications() const;
    static constexpr std::uint32_t max_child_statuses = 16u;
    std::uint32_t                           child_statuses() const;
    std::uint32_t                           child_status_uid(std::uint32_t index) const;
    bool                                    child_status_connected(std::uint32_t index) const;
    using InterruptSink = void (*)(void *context, const DxgkargcbNotifyInterruptDataRecord &data);
    void                                    on_interrupt(InterruptSink sink, void *context);
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

}

#endif

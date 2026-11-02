#ifndef AV_LOOPBACK_KMT_HPP
#define AV_LOOPBACK_KMT_HPP

#include "guest/kmt/kmt.hpp"
#include "harness/loopback/loopback_device.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

namespace av {

class LoopbackKmt final : public Kmt {
public:
    static constexpr std::uint32_t max_allocations = 32u;
    static constexpr std::uint32_t max_contexts    = 8u;
    static constexpr std::uint32_t first_resource  = 0x00100000u;
    static constexpr std::uint64_t luid            = 0x0000000100000001ull;
    static constexpr std::uint64_t budget          = std::uint64_t{64} << 20u;

    LoopbackKmt(MockPlatform &platform, LoopbackDevice &device);

    Status   enumerate_adapters(std::uint32_t *adapters, std::uint32_t capacity, std::uint32_t &count) override;
    Status   query_identity(std::uint32_t adapter, PciIdentity &identity) override;
    Status   query_driver_version(std::uint32_t adapter, std::uint32_t &version) override;
    Status   query_adapter_location(std::uint32_t adapter, PciLocation &location) override;
    Status   query_adapter_name(std::uint32_t adapter, std::uint16_t *name, std::uint32_t capacity) override;
    Status   query_umd_driver_version(std::uint32_t adapter, std::uint64_t &version) override;
    Status   query_segment_sizes(std::uint32_t adapter, std::uint64_t &dedicated_video, std::uint64_t &dedicated_system,
                                   std::uint64_t &shared_system) override;
    Status   query_native_gpu_fences(std::uint32_t adapter, bool &supported) override;
    Status   query_adapter_features(std::uint32_t adapter, UmdAdapterInfo &info) override;
    void     sleep_briefly() override;
    Status   close_adapter(std::uint32_t adapter) override;
    Status   adapter_luid(std::uint32_t adapter, std::uint64_t &luid) override;
    Status   primary_display_luid(std::uint64_t &luid) override;
    Status   query_video_memory(std::uint32_t adapter, MemorySegmentGroup group, std::uint64_t &budget,
                                std::uint64_t &usage) override;
    Status   create_device(std::uint32_t adapter, std::uint32_t &device) override;
    Status   destroy_device(std::uint32_t device) override;
    Status   create_allocation(std::uint32_t device, const std::uint8_t *descriptor, std::uint32_t size,
                               std::uint32_t &allocation) override;
    Status   destroy_allocation(std::uint32_t device, std::uint32_t allocation) override;
    Status   create_shared_allocation(std::uint32_t device, std::uint8_t *private_data, std::uint32_t size,
                                      std::uint32_t &resource) override;
    Status   share_object(std::uint32_t device, std::uint32_t resource, std::uint64_t &handle) override;
    Status   destroy_shared_allocation(std::uint32_t device, std::uint32_t resource) override;
    Status   open_shared_object(std::uint32_t device, std::uint64_t handle, std::uint32_t &resource,
                                std::uint8_t *private_data, std::uint32_t capacity,
                                std::uint32_t &written) override;
    Status   close_shared_object(std::uint64_t handle) override;
    Status   lock(std::uint32_t device, std::uint32_t allocation, void *&view) override;
    Status   unlock(std::uint32_t device, std::uint32_t allocation) override;
    Status   create_paging_queue(std::uint32_t device, std::uint32_t &queue, std::uint32_t &fence_object) override;
    Status   destroy_paging_queue(std::uint32_t queue) override;
    Status   make_resident(std::uint32_t queue, std::uint32_t allocation, std::uint64_t &paging_fence) override;
    Status   map_gpu_virtual_address(std::uint32_t queue, std::uint32_t allocation, std::uint64_t pages,
                                     std::uint64_t &address, std::uint64_t &paging_fence) override;
    Status   free_gpu_virtual_address(std::uint32_t adapter, std::uint64_t address, std::uint64_t pages) override;
    Status   wait_for_fence(std::uint32_t device, std::uint32_t fence_object, std::uint64_t fence) override;
    Status   create_context(std::uint32_t device, std::uint32_t &context) override;
    Status   create_hw_queue_context(std::uint32_t device, const std::uint8_t *private_data, std::uint32_t size,
                                     std::uint32_t &context) override;
    Status   create_context_virtual(std::uint32_t device, const std::uint8_t *private_data, std::uint32_t size,
                                    std::uint32_t &context) override;
    Status   destroy_context(std::uint32_t context) override;
    Status   submit(std::uint32_t context, std::uint64_t commands, std::uint64_t length,
                    const std::uint8_t *private_data, std::uint32_t private_size) override;
    Status   wait_for_context(std::uint32_t device, std::uint32_t context) override;
    Status   create_hw_queue(std::uint32_t device, std::uint32_t context, GuestHwQueue &queue) override;
    Status   submit_to_hw_queue(std::uint32_t queue, std::uint64_t commands, std::uint64_t length,
                                const std::uint8_t *private_data, std::uint32_t private_size,
                                std::uint64_t progress_fence_id) override;
    Status   wait_for_hw_queue(std::uint32_t device, std::uint32_t progress_fence, std::uint64_t value) override;
    Status   destroy_hw_queue(std::uint32_t queue) override;
    Status   create_doorbell(std::uint32_t queue, std::uint32_t ring, std::uint32_t &doorbell, void *&page,
                             const std::uint32_t *&status, std::uint64_t *&queued) override;
    Status   connect_doorbell(std::uint32_t doorbell) override;
    Status   destroy_doorbell(std::uint32_t doorbell) override;
    Status   notify_work_submission(std::uint32_t doorbell) override;

    Status   wait_for_vertical_blank(std::uint32_t adapter, std::uint32_t device, std::uint32_t source) override;
    Status   scan_line(std::uint32_t adapter, std::uint32_t source, BeamPosition &position) override;
    Status   refresh_millihz(std::uint32_t adapter, std::uint32_t source, std::uint32_t &millihz) override;
    Status   escape(std::uint32_t adapter, std::uint32_t device, std::uint32_t context, std::uint8_t *data,
                    std::uint32_t size) override;
    std::uint64_t performance_counter(std::uint64_t &frequency) const override;
    std::uint32_t last_code() const override;

private:
    static constexpr std::uint64_t page_bytes = 4096u;

    struct Allocation {
        std::uint32_t handle;
        std::uint8_t *view;
        std::uint64_t physical;
        std::uint64_t bytes;
    };
    struct Context {
        std::uint32_t handle;
        Allocation    ring;
        Allocation    fence;
        Allocation    reply;
        std::uint32_t doorbell;
        std::uint32_t blobs[3];
        std::uint32_t seen;
    };

    Allocation *find_allocation(std::uint32_t handle);
    Context    *find_context(std::uint32_t handle);
    bool        control(const std::uint8_t *request, std::size_t length);
    bool        publish(std::uint32_t resource_id, const Allocation &allocation);
    Allocation  make_memory(std::uint64_t bytes);
    void        free_memory(Allocation &memory);
    void        free_context_memory(Context &context);
    Status      context_views(std::uint32_t context, std::uint8_t *data, std::uint32_t size);

    MockPlatform  &platform_;
    LoopbackDevice &device_;
    Allocation     allocations_[max_allocations];
    Context        contexts_[max_contexts];
    std::uint32_t  allocation_count_;
    std::uint32_t  context_count_;
    std::uint32_t  next_handle_;
    std::uint32_t  next_resource_;
    std::uint32_t  next_doorbell_;
};

}

#endif

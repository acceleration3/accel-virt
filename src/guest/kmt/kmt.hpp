#ifndef AV_KMT_HPP
#define AV_KMT_HPP

#include "core/support/status.hpp"
#include "guest/kmt/guest_hw_queue.hpp"
#include "guest/kmt/memory_segment_group.hpp"

#include <stdint.h>

namespace av {

class UmdAdapterInfo;

class BeamPosition;
class PciLocation;
class PciIdentity;

class Kmt {
public:
    virtual Status   enumerate_adapters(uint32_t *adapters, uint32_t capacity, uint32_t &count) = 0;
    virtual Status   query_identity(uint32_t adapter, PciIdentity &identity) = 0;
    virtual Status   query_driver_version(uint32_t adapter, uint32_t &version) = 0;
    virtual Status   query_adapter_location(uint32_t adapter, PciLocation &location) = 0;
    virtual Status   query_adapter_name(uint32_t adapter, uint16_t *name, uint32_t capacity) = 0;
    virtual Status   query_umd_driver_version(uint32_t adapter, uint64_t &version) = 0;
    virtual Status   query_segment_sizes(uint32_t adapter, uint64_t &dedicated_video, uint64_t &dedicated_system,
                                         uint64_t &shared_system) = 0;
    virtual Status   query_native_gpu_fences(uint32_t adapter, bool &supported) = 0;
    virtual Status   query_adapter_features(uint32_t adapter, UmdAdapterInfo &info) = 0;
    virtual Status   close_adapter(uint32_t adapter) = 0;
    virtual Status   adapter_luid(uint32_t adapter, uint64_t &luid) = 0;
    virtual Status   primary_display_luid(uint64_t &luid) = 0;
    virtual Status   query_video_memory(uint32_t adapter, MemorySegmentGroup group, uint64_t &budget,
                                        uint64_t &usage) = 0;
    virtual Status   create_device(uint32_t adapter, uint32_t &device) = 0;
    virtual Status   destroy_device(uint32_t device) = 0;
    virtual Status   create_allocation(uint32_t device, const uint8_t *descriptor, uint32_t size,
                                       uint32_t &allocation) = 0;
    virtual Status   destroy_allocation(uint32_t device, uint32_t allocation) = 0;
    virtual Status   create_shared_allocation(uint32_t device, uint8_t *private_data, uint32_t size,
                                              uint32_t &resource) = 0;
    virtual Status   share_object(uint32_t device, uint32_t resource, uint64_t &handle) = 0;
    virtual Status   destroy_shared_allocation(uint32_t device, uint32_t resource) = 0;
    virtual Status   open_shared_object(uint32_t device, uint64_t handle, uint32_t &resource,
                                        uint8_t *private_data, uint32_t capacity, uint32_t &written) = 0;
    virtual Status   close_shared_object(uint64_t handle) = 0;
    virtual Status   lock(uint32_t device, uint32_t allocation, void *&view) = 0;
    virtual Status   unlock(uint32_t device, uint32_t allocation) = 0;
    virtual Status   create_paging_queue(uint32_t device, uint32_t &queue, uint32_t &fence_object) = 0;
    virtual Status   destroy_paging_queue(uint32_t queue) = 0;
    virtual Status   make_resident(uint32_t queue, uint32_t allocation, uint64_t &paging_fence) = 0;
    virtual Status   map_gpu_virtual_address(uint32_t queue, uint32_t allocation, uint64_t pages, uint64_t &address,
                                             uint64_t &paging_fence) = 0;
    virtual Status   free_gpu_virtual_address(uint32_t adapter, uint64_t address, uint64_t pages) = 0;
    virtual Status   wait_for_fence(uint32_t device, uint32_t fence_object, uint64_t fence) = 0;
    virtual Status   create_context(uint32_t device, uint32_t &context) = 0;
    virtual Status   create_context_virtual(uint32_t device, const uint8_t *private_data, uint32_t size,
                                            uint32_t &context) = 0;
    virtual Status   create_hw_queue_context(uint32_t device, const uint8_t *private_data, uint32_t size,
                                             uint32_t &context) = 0;
    virtual Status   destroy_context(uint32_t context) = 0;
    virtual Status   submit(uint32_t context, uint64_t commands, uint64_t length, const uint8_t *private_data,
                            uint32_t private_size) = 0;
    virtual Status   wait_for_context(uint32_t device, uint32_t context) = 0;
    virtual Status   create_hw_queue(uint32_t device, uint32_t context, GuestHwQueue &queue) = 0;
    virtual Status   submit_to_hw_queue(uint32_t queue, uint64_t commands, uint64_t length,
                                        const uint8_t *private_data, uint32_t private_size,
                                        uint64_t progress_fence_id) = 0;
    virtual Status   wait_for_hw_queue(uint32_t device, uint32_t progress_fence, uint64_t value) = 0;
    virtual Status   destroy_hw_queue(uint32_t queue) = 0;
    virtual Status   create_doorbell(uint32_t queue, uint32_t ring, uint32_t &doorbell, void *&page,
                                     const uint32_t *&status, uint64_t *&queued) = 0;
    virtual Status   connect_doorbell(uint32_t doorbell) = 0;
    virtual Status   destroy_doorbell(uint32_t doorbell) = 0;
    virtual Status   notify_work_submission(uint32_t doorbell) = 0;
    virtual Status   wait_for_vertical_blank(uint32_t adapter, uint32_t device, uint32_t source) = 0;
    virtual Status   scan_line(uint32_t adapter, uint32_t source, BeamPosition &position) = 0;
    virtual Status   refresh_millihz(uint32_t adapter, uint32_t source, uint32_t &millihz) = 0;
    virtual Status   escape(uint32_t adapter, uint32_t device, uint32_t context, uint8_t *data, uint32_t size) = 0;
    virtual uint64_t performance_counter(uint64_t &frequency) const = 0;
    virtual void     sleep_briefly() = 0;
    virtual uint32_t last_code() const = 0;

protected:
    ~Kmt() = default;
};

}

#endif

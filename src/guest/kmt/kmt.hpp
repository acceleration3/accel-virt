/* The D3DKMT calls the in-guest suite makes, in core's types: handles are the
 * kernel's D3DKMT_HANDLE values, results are Status, and the NTSTATUS behind
 * the last call is kept for the report. GdiKmt (src/um) calls gdi32; MockKmt
 * (tests/harness/guest) answers what a test scripts. A handle or address of 0
 * is never valid. Every wait is bounded by the implementation. */
#ifndef AV_KMT_HPP
#define AV_KMT_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class PciIdentity;

class Kmt {
public:
    /* Opens every adapter: at most `capacity` handles, how many in `count`. */
    virtual Status   enumerate_adapters(uint32_t *adapters, uint32_t capacity, uint32_t &count) = 0;
    virtual Status   query_identity(uint32_t adapter, PciIdentity &identity) = 0;
    /* KMT_DRIVERVERSION: 3200 for WDDM 3.2. */
    virtual Status   query_driver_version(uint32_t adapter, uint32_t &version) = 0;
    virtual Status   close_adapter(uint32_t adapter) = 0;
    virtual Status   create_device(uint32_t adapter, uint32_t &device) = 0;
    virtual Status   destroy_device(uint32_t device) = 0;
    virtual Status   create_allocation(uint32_t device, const uint8_t *descriptor, uint32_t size,
                                       uint32_t &allocation) = 0;
    virtual Status   destroy_allocation(uint32_t device, uint32_t allocation) = 0;
    /* The process's CPU view of an allocation, in `view`: for one in a
     * CPU-visible segment, the host's own memory through the hostmem BAR. It
     * stays addressable until unlock. */
    virtual Status   lock(uint32_t device, uint32_t allocation, void *&view) = 0;
    virtual Status   unlock(uint32_t device, uint32_t allocation) = 0;
    /* A paging queue and the synchronization object its fences are waited on. */
    virtual Status   create_paging_queue(uint32_t device, uint32_t &queue, uint32_t &fence_object) = 0;
    virtual Status   destroy_paging_queue(uint32_t queue) = 0;
    /* The paging fence that reaching makes it resident. */
    virtual Status   make_resident(uint32_t queue, uint32_t allocation, uint64_t &paging_fence) = 0;
    virtual Status   map_gpu_virtual_address(uint32_t queue, uint32_t allocation, uint64_t pages, uint64_t &address,
                                             uint64_t &paging_fence) = 0;
    virtual Status   free_gpu_virtual_address(uint32_t adapter, uint64_t address, uint64_t pages) = 0;
    virtual Status   wait_for_fence(uint32_t device, uint32_t fence_object, uint64_t fence) = 0;
    virtual Status   create_context(uint32_t device, uint32_t &context) = 0;
    virtual Status   destroy_context(uint32_t context) = 0;
    virtual Status   submit(uint32_t context, uint64_t commands, uint64_t length) = 0;
    /* Until everything submitted on the context so far has completed. */
    virtual Status   wait_for_context(uint32_t device, uint32_t context) = 0;
    /* Until the next vertical blank of `source` on that device: what DWM's
     * frame pacing waits on, and the only way in to see the rate the driver
     * reports. */
    virtual Status   wait_for_vertical_blank(uint32_t adapter, uint32_t device, uint32_t source) = 0;
    /* The system's performance counter and its frequency: a vertical blank
     * carries no timestamp, so a rate is measured against this. */
    virtual uint64_t performance_counter(uint64_t &frequency) const = 0;
    virtual uint32_t last_code() const = 0;

protected:
    ~Kmt() = default;
};

}  // namespace av

#endif  // AV_KMT_HPP

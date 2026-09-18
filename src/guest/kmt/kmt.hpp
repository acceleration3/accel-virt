/* The D3DKMT calls the in-guest suite makes, in core's types: handles are the
 * kernel's D3DKMT_HANDLE values, results are Status, and the NTSTATUS behind
 * the last call is kept for the report. GdiKmt (src/um) calls gdi32; MockKmt
 * (tests/harness/guest) answers what a test scripts. A handle or address of 0
 * is never valid. Every wait is bounded by the implementation. */
#ifndef AV_KMT_HPP
#define AV_KMT_HPP

#include "core/support/status.hpp"
#include "guest/kmt/memory_segment_group.hpp"

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
    /* The adapter's LUID as enumeration reports it, LowPart in the low 32 bits
     * and HighPart in the high: what the Vulkan loader matches an ICD's
     * physical device against. */
    virtual Status   adapter_luid(uint32_t adapter, uint64_t &luid) = 0;
    /* D3DKMTQueryVideoMemoryInfo for this process: how much of `group` it may
     * use, and how much its devices use now. */
    virtual Status   query_video_memory(uint32_t adapter, MemorySegmentGroup group, uint64_t &budget,
                                        uint64_t &usage) = 0;
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
    /* A virtual context carrying private driver data - the RvkContextRecord an
     * rvk transport asks its context with; the miniport decodes it. */
    virtual Status   create_context_virtual(uint32_t device, const uint8_t *private_data, uint32_t size,
                                            uint32_t &context) = 0;
    virtual Status   destroy_context(uint32_t context) = 0;
    /* On a context made with create_context_virtual, dxgkrnl routes this to the
     * DxgkDdiSubmitCommandVirtual DDI; `commands` is the DMA buffer's GPU
     * virtual address. `private_data` is copied by dxgkrnl and handed to the
     * driver beside it - where an rvk transport puts its submission
     * descriptor, since the driver cannot read a GPU address (feature 043).
     * Null and 0 for none. */
    virtual Status   submit(uint32_t context, uint64_t commands, uint64_t length, const uint8_t *private_data,
                            uint32_t private_size) = 0;
    /* Until everything submitted on the context so far has completed. */
    virtual Status   wait_for_context(uint32_t device, uint32_t context) = 0;
    /* A hardware queue on `context`, which is what a doorbell is created for
     * (feature 049, research.md §1). `fence` is where the driver writes the
     * queue's progress, which the process reads. not_supported when the OS
     * will not give this adapter one - hardware scheduling is its decision,
     * not the driver's (research.md §11) - and the transport then submits the
     * way it always has. */
    virtual Status   create_hw_queue(uint32_t device, uint32_t context, void *fence, uint32_t &queue) = 0;
    virtual Status   destroy_hw_queue(uint32_t queue) = 0;
    /* D3DKMTCreateDoorbell on `queue` over the process's `ring` allocation: the
     * three addresses it answers are the doorbell itself, the status page a
     * process reads before every store, and where it writes the fence value it
     * has queued. A doorbell created is not connected. */
    virtual Status   create_doorbell(uint32_t queue, uint32_t ring, uint32_t &doorbell, void *&page,
                                     const uint32_t *&status, uint64_t *&queued) = 0;
    /* Connected, the process may store; dxgkrnl disconnects it for suspend,
     * reset and migration without telling the process, which learns it from
     * the status page (contracts, research.md §6). */
    virtual Status   connect_doorbell(uint32_t doorbell) = 0;
    virtual Status   destroy_doorbell(uint32_t doorbell) = 0;
    /* What a process calls after a store when the status asked it to. This
     * device's never does, so nothing calls it in the steady state. */
    virtual Status   notify_work_submission(uint32_t doorbell) = 0;
    /* Until the next vertical blank of `source` on that device: what DWM's
     * frame pacing waits on, and the only way in to see the rate the driver
     * reports. */
    virtual Status   wait_for_vertical_blank(uint32_t adapter, uint32_t device, uint32_t source) = 0;
    /* D3DKMTEscape with driver-private data on `context` of `device`: the
     * miniport reads the `size` bytes and may write its answer back into them,
     * which the caller finds in `data` on return. The status is the driver's. */
    virtual Status   escape(uint32_t adapter, uint32_t device, uint32_t context, uint8_t *data, uint32_t size) = 0;
    /* The system's performance counter and its frequency: a vertical blank
     * carries no timestamp, so a rate is measured against this. */
    virtual uint64_t performance_counter(uint64_t &frequency) const = 0;
    virtual uint32_t last_code() const = 0;

protected:
    ~Kmt() = default;
};

}  // namespace av

#endif  // AV_KMT_HPP

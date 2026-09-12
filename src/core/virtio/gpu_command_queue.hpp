/* One virtqueue the driver sends virtio-gpu requests on. Each request, and the
 * response buffer after it when the queue has responses, lives in one of a
 * fixed set of slots of contiguous memory from send until the harvest that
 * finds it used; nothing allocates after activation. A spin lock serializes
 * senders with the DPC's harvest.
 *
 * One request is much larger than the rest: RESOURCE_ATTACH_BACKING for a
 * primary carries a memory entry per run of adjacent guest pages, so a queue
 * that must carry it is given a single wide slot as well - wide enough for any
 * run count a mode can produce, and at most one of those requests is in flight
 * at a time. */
#ifndef AV_GPU_COMMAND_QUEUE_HPP
#define AV_GPU_COMMAND_QUEUE_HPP

#include "core/platform/lock_rank.hpp"
#include "core/platform/platform.hpp"
#include "core/platform/spin_lock.hpp"
#include "core/support/status.hpp"
#include "core/virtio/virt_queue.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class VirtioPciTransport;

class GpuCommandQueue {
public:
    static constexpr uint32_t slot_count = 16u;
    /* The slot the requests that do not fit a narrow one go in. */
    static constexpr uint32_t wide_slot = slot_count;

    /* `response_bytes` zero: the device writes nothing back. `wide_bytes` zero:
     * the queue carries no request wider than GpuRequestWriter's maximum and
     * refuses one that is. */
    GpuCommandQueue(Platform &platform, VirtioPciTransport &transport, uint16_t index, uint32_t response_bytes,
                    uint32_t wide_bytes, LockRank rank);
    ~GpuCommandQueue();
    GpuCommandQueue(const GpuCommandQueue &) = delete;
    GpuCommandQueue &operator=(const GpuCommandQueue &) = delete;

    /* Allocates the slots and activates the queue on MSI-X `vector`. */
    [[nodiscard]] Status activate(uint16_t vector);
    /* no_space while every slot is in flight. */
    [[nodiscard]] Status send(const uint8_t *request, size_t length);
    /* Reclaims every used request; `refused` counts responses shorter than a
     * header or not VIRTIO_GPU_RESP_OK_NODATA. A used element the queue never
     * published is invalid_state. */
    [[nodiscard]] Status harvest(uint32_t &completed, uint32_t &refused);
    uint32_t             in_flight() const;

    bool invariants_hold() const;

private:
    size_t slot_bytes() const;
    /* Bytes of the whole slot area, the wide slot included. */
    size_t slots_bytes() const;
    /* Where slot `slot` starts, and how much of it the request may use. */
    size_t offset_of(uint32_t slot) const;
    size_t request_bytes_of(uint32_t slot) const;
    static constexpr uint32_t no_slot = slot_count + 1u;

    /* The slot a request of `length` bytes goes in, or no_slot when none is
     * free or none is wide enough. */
    uint32_t slot_for(size_t length) const;
    void   release_slots();

    Platform           &platform_;
    VirtioPciTransport &transport_;
    VirtQueue           queue_;
    SpinLock            lock_;
    uint32_t            response_bytes_;
    uint8_t            *slots_;
    uint64_t            slots_address_;
    uint32_t            wide_bytes_;
    bool                busy_[slot_count + 1u];
    uint8_t             slot_of_head_[VirtQueue::max_size];
    uint32_t            in_flight_;
};

}  // namespace av

#endif  // AV_GPU_COMMAND_QUEUE_HPP

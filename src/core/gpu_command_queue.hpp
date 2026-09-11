/* One virtqueue the driver sends virtio-gpu requests on. Each request, and the
 * response buffer after it when the queue has responses, lives in one of a
 * fixed set of slots of contiguous memory from send until the harvest that
 * finds it used; nothing allocates after activation. A spin lock serializes
 * senders with the DPC's harvest. */
#ifndef AV_GPU_COMMAND_QUEUE_HPP
#define AV_GPU_COMMAND_QUEUE_HPP

#include "lock_rank.hpp"
#include "platform.hpp"
#include "spin_lock.hpp"
#include "status.hpp"
#include "virt_queue.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class VirtioPciTransport;

class GpuCommandQueue {
public:
    static constexpr uint32_t slot_count = 16u;

    /* `response_bytes` zero: the device writes nothing back. */
    GpuCommandQueue(Platform &platform, VirtioPciTransport &transport, uint16_t index, uint32_t response_bytes,
                    LockRank rank);
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
    void   release_slots();

    Platform           &platform_;
    VirtioPciTransport &transport_;
    VirtQueue           queue_;
    SpinLock            lock_;
    uint32_t            response_bytes_;
    uint8_t            *slots_;
    uint64_t            slots_address_;
    bool                busy_[slot_count];
    uint8_t             slot_of_head_[VirtQueue::max_size];
    uint32_t            in_flight_;
};

}  // namespace av

#endif  // AV_GPU_COMMAND_QUEUE_HPP

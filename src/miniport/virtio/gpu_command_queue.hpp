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
#include "miniport/virtio/virt_queue.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class VirtioPciTransport;
class VirtqSegment;
struct GpuCommandQueueProbe;

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
    /* The same, the request continuing in `payload` as exchange_carrying's
     * does, not waited on: the caller keeps the payload until the device has
     * read it. */
    [[nodiscard]] Status send_carrying(const uint8_t *request, size_t length, const VirtqSegment &payload);
    /* Sends `request` and waits for the device's answer to it, copying at most
     * `capacity` bytes into `response` and counting them in `written`. Passive
     * only: it stalls between polls, and harvests the queue itself, so it
     * answers before dxgkrnl has connected the interrupt. One exchange at a
     * time; not_found when the device never answers. */
    [[nodiscard]] Status exchange(const uint8_t *request, size_t length, uint8_t *response, size_t capacity,
                                  uint32_t &written);
    /* The same, the request continuing in `payload` - memory of the caller's,
     * read by the device after the slot's bytes as one request: what carries
     * a request too large for any slot. */
    [[nodiscard]] Status exchange_carrying(const uint8_t *request, size_t length, const VirtqSegment &payload,
                                           uint8_t *response, size_t capacity, uint32_t &written);
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
    /* A hundred milliseconds of polling: far longer than a control request
     * takes, short enough that a dead device does not hang StartDevice. */
    static constexpr uint32_t poll_microseconds = 100u;
    static constexpr uint32_t polls = 1000u;

    /* The slot a request of `length` bytes goes in, or no_slot when none is
     * free or none is wide enough. */
    uint32_t slot_for(size_t length) const;
    /* Publishes one request, followed by `payload_count` (0 or 1) buffers of
     * the caller's; the caller holds the lock and notifies. */
    [[nodiscard]] Status send_locked(const uint8_t *request, size_t length, const VirtqSegment *payload,
                                     uint16_t payload_count, uint32_t &slot);
    [[nodiscard]] Status exchange_segments(const uint8_t *request, size_t length, const VirtqSegment *payload,
                                           uint16_t payload_count, uint8_t *response, size_t capacity,
                                           uint32_t &written);
    void   release_slots();

    Platform           &platform_;
    VirtioPciTransport &transport_;
    VirtQueue           queue_;
    SpinLock            lock_;
    uint32_t            response_bytes_;
    uint8_t            *slots_;
    uint64_t            slots_address_;
    uint32_t            wide_bytes_;
    /* The slot an exchange is waiting on, and where its answer goes. */
    uint32_t            awaited_slot_;
    uint8_t            *awaited_response_;
    size_t              awaited_capacity_;
    uint32_t            awaited_written_;
    bool                busy_[slot_count + 1u];
    uint8_t             slot_of_head_[VirtQueue::max_size];
    uint32_t            in_flight_;

    friend struct GpuCommandQueueProbe;
};

}  // namespace av

#endif  // AV_GPU_COMMAND_QUEUE_HPP

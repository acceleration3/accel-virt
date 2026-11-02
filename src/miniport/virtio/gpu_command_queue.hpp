#ifndef AV_GPU_COMMAND_QUEUE_HPP
#define AV_GPU_COMMAND_QUEUE_HPP

#include "core/platform/lock_rank.hpp"
#include "core/platform/platform.hpp"
#include "core/platform/spin_lock.hpp"
#include "core/platform/wake_event.hpp"
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
    static constexpr uint32_t wide_slot = slot_count;

    GpuCommandQueue(Platform &platform, VirtioPciTransport &transport, uint16_t index, uint32_t response_bytes,
                    uint32_t wide_bytes, LockRank rank);
    ~GpuCommandQueue();
    GpuCommandQueue(const GpuCommandQueue &) = delete;
    GpuCommandQueue &operator=(const GpuCommandQueue &) = delete;

    [[nodiscard]] Status activate(uint16_t vector);
    [[nodiscard]] Status send(const uint8_t *request, size_t length);
    [[nodiscard]] Status send_carrying(const uint8_t *request, size_t length, const VirtqSegment &payload);
    [[nodiscard]] Status exchange(const uint8_t *request, size_t length, uint8_t *response, size_t capacity,
                                  uint32_t &written);
    [[nodiscard]] Status exchange_carrying(const uint8_t *request, size_t length, const VirtqSegment &payload,
                                           uint8_t *response, size_t capacity, uint32_t &written);
    [[nodiscard]] Status harvest(uint32_t &completed, uint32_t &refused);
    uint32_t             in_flight() const;

    bool invariants_hold() const;

private:
    size_t slot_bytes() const;
    size_t slots_bytes() const;
    size_t offset_of(uint32_t slot) const;
    size_t request_bytes_of(uint32_t slot) const;
    static constexpr uint32_t no_slot = slot_count + 1u;
    static constexpr uint32_t poll_microseconds = 20u;
    static constexpr uint64_t spin_ticks = 20000u;
    static constexpr uint64_t sleep_ticks = 100000u;
    static constexpr uint64_t patience_ticks = 30000000u;

    uint32_t slot_for(size_t length) const;
    [[nodiscard]] Status send_locked(const uint8_t *request, size_t length, const VirtqSegment *payload,
                                     uint16_t payload_count, uint32_t &slot);
    [[nodiscard]] Status exchange_segments(const uint8_t *request, size_t length, const VirtqSegment *payload,
                                           uint16_t payload_count, uint8_t *response, size_t capacity,
                                           uint32_t &written);
    [[nodiscard]] Status await_answer(uint32_t &written);
    [[nodiscard]] Status reclaim_used(uint32_t &completed, uint32_t &refused, bool &answered);
    void   release_slots();

    Platform           &platform_;
    VirtioPciTransport &transport_;
    VirtQueue           queue_;
    SpinLock            lock_;
    uint32_t            response_bytes_;
    uint8_t            *slots_;
    uint64_t            slots_address_;
    uint32_t            wide_bytes_;
    uint32_t            awaited_slot_;
    uint8_t            *awaited_response_;
    size_t              awaited_capacity_;
    uint32_t            awaited_written_;
    uint32_t            abandoned_slot_;
    WakeEvent           answer_;
    bool                busy_[slot_count + 1u];
    uint8_t             slot_of_head_[VirtQueue::max_size];
    uint32_t            in_flight_;

    friend struct GpuCommandQueueProbe;
};

}

#endif

#ifndef AV_VULKAN_PARKED_SUBMITS_HPP
#define AV_VULKAN_PARKED_SUBMITS_HPP

#include "core/support/growable_array.hpp"
#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class BlockSource;
class PollClock;
class VulkanParkHost;
class VulkanSubmitSet;
class WireReader;

class VulkanParkedSubmits {
public:
    static constexpr uint32_t most_parked = 32u;
    static constexpr uint64_t park_nanoseconds = 1000000000u;

    struct Request {
        uint64_t               queue_;
        uint64_t               device_;
        uint64_t               mark_;
        uint32_t               command_;
        const WireReader      *frame_;
        const VulkanSubmitSet *set_;
    };

    VulkanParkedSubmits(BlockSource &blocks, VulkanParkHost &host, const PollClock &clock);
    ~VulkanParkedSubmits();
    VulkanParkedSubmits(const VulkanParkedSubmits &) = delete;
    VulkanParkedSubmits &operator=(const VulkanParkedSubmits &) = delete;

    void clock_from(const PollClock &clock);

    bool                 behind(uint64_t queue, const uint64_t *waits, uint32_t count) const;
    [[nodiscard]] Status park(const Request &request);
    void                 service();
    void                 drop_event(uint64_t event);
    void                 drop_device(uint64_t device);
    void                 drop_all();

    bool     pending() const;
    uint32_t count() const;

    bool invariants_hold() const;

private:
    struct Batch {
        Batch(BlockSource &blocks, const Request &request);
        ~Batch();
        Batch(const Batch &) = delete;
        Batch &operator=(const Batch &) = delete;

        BlockSource            &blocks_;
        GrowableArray<uint64_t> events_;
        GrowableArray<uint64_t> waits_;
        GrowableArray<uint64_t> signals_;
        uint8_t                *frame_;
        size_t                  frame_bytes_;
        uint64_t                queue_;
        uint64_t                device_;
        uint64_t                mark_;
        uint64_t                since_;
        uint32_t                command_;
    };

    [[nodiscard]] Status fill(Batch &batch, const Request &request);
    bool                 held_by_earlier(uint32_t index) const;
    bool                 events_unset(const Batch &batch) const;
    void                 discard(uint32_t index);
    void                 discard_silently();

    BlockSource                    &blocks_;
    VulkanParkHost                 &host_;
    const PollClock                *clock_;
    ObjectPool<Batch>               pool_;
    GrowableArray<Batch *>          order_;
    bool                            serving_;
};

}

#endif

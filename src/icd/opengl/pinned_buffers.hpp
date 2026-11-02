#ifndef AV_GLICD_PINNED_BUFFERS_HPP
#define AV_GLICD_PINNED_BUFFERS_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
class HostAllocation;
}

namespace av::icd {
class HostChannel;
}

namespace av::glicd {

class PinnedBuffers {
public:
    static constexpr uint32_t target = 0x9160u;

    PinnedBuffers(BlockSource &memory, icd::HostChannel &channel);
    PinnedBuffers(const PinnedBuffers &) = delete;
    PinnedBuffers &operator=(const PinnedBuffers &) = delete;

    void     bind(uint32_t buffer);
    uint32_t bound() const;
    [[nodiscard]] Status pin(const void *address, uint64_t bytes, HostAllocation *&lent, uint64_t &offset);
    void                 forget(int32_t count, const uint32_t *buffers);
    void                 reclaim();

private:
    static constexpr uint64_t page_bytes = 4096u;

    icd::HostChannel                       &channel_;
    uint32_t                                bound_;
    KeyedSlots<uint32_t, HostAllocation *>  pinned_;
};

}

#endif

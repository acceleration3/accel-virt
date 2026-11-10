#ifndef AV_HOST_SVM_RANGES_HPP
#define AV_HOST_SVM_RANGES_HPP

#include "core/support/growable_array.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class BlockSource;
struct HostSvmRangesProbe;

class HostSvmRanges {
public:
    explicit HostSvmRanges(BlockSource &blocks);
    HostSvmRanges(const HostSvmRanges &) = delete;
    HostSvmRanges &operator=(const HostSvmRanges &) = delete;

    [[nodiscard]] Status make_room();
    [[nodiscard]] Status remember(void *base, uint64_t bytes, uint64_t owner);
    [[nodiscard]] Status share(void *base, uint64_t bytes, uint64_t owner);
    uint32_t             forget_owned(uint64_t owner);
    bool                 forget(void *base);
    void                 forget_all();

    void    *resolve(uint64_t address, uint64_t bytes) const;
    uint32_t count() const;

    bool invariants_hold() const;

private:
    friend struct HostSvmRangesProbe;

    struct Range {
        uint64_t base_;
        uint64_t size_;
        uint64_t owner_;
        uint32_t holds_;
    };

    uint32_t above(uint64_t address) const;

    GrowableArray<Range> ranges_;
    uint64_t             widest_;
};

}

#endif

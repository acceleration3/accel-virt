#ifndef AV_BATCH_MARK_HPP
#define AV_BATCH_MARK_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class Kmt;

class BatchMark {
public:
    static constexpr uint32_t wait_timed_out = 0xC00000B5u;

    BatchMark();
    BatchMark(uint32_t device, uint32_t context, uint32_t progress_fence, uint64_t fence);

    bool     waitable() const;
    uint64_t fence() const;
    [[nodiscard]] Status wait(Kmt &kmt) const;

private:
    uint32_t device_;
    uint32_t context_;
    uint32_t progress_fence_;
    uint64_t fence_;
};

}

#endif

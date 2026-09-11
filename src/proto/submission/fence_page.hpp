/* One snapshot of a context's fence page, written by the device
 * (docs/protocol.md). */
#ifndef AV_FENCE_PAGE_HPP
#define AV_FENCE_PAGE_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class FencePage {
public:
    static constexpr size_t encoded_size = 32u;

    FencePage();
    /* A healthy context. */
    FencePage(uint64_t completed_fence, uint64_t progress);
    /* A faulted context, with the device's reason. */
    FencePage(uint64_t completed_fence, uint64_t progress, uint32_t fault_code);

    bool     faulted() const;
    uint64_t completed_fence() const;
    uint64_t progress() const;
    uint32_t fault_code() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* Structure only; FenceProgress judges the values. Unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    static constexpr uint32_t magic        = 0x464B5652u;  /* "RVKF" */
    static constexpr uint32_t faulted_flag = 1u;

    uint32_t flags_;
    uint64_t completed_fence_;
    uint64_t progress_;
    uint32_t fault_code_;
};

}  // namespace av

#endif  // AV_FENCE_PAGE_HPP

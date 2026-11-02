#ifndef AV_FENCE_PAGE_HPP
#define AV_FENCE_PAGE_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class FencePage {
public:
    static constexpr size_t encoded_size = 48u;
    static constexpr size_t beat_at = 40u;

    FencePage();
    FencePage(uint64_t completed_fence, uint64_t progress);
    FencePage(uint64_t completed_fence, uint64_t progress, uint32_t fault_code);

    FencePage raising(uint64_t raised) const;
    FencePage beating(uint64_t beat) const;
    bool     faulted() const;
    uint64_t completed_fence() const;
    uint64_t progress() const;
    uint32_t fault_code() const;
    uint64_t raised() const;
    uint64_t beat() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    static constexpr uint32_t magic        = 0x00465641u;
    static constexpr uint32_t faulted_flag = 1u;

    uint32_t flags_;
    uint64_t completed_fence_;
    uint64_t progress_;
    uint32_t fault_code_;
    uint64_t raised_;
    uint64_t beat_;
};

}

#endif

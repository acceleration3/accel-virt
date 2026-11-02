#ifndef AV_PRESENT_PROGRESS_HPP
#define AV_PRESENT_PROGRESS_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class PresentProgress {
public:
    static constexpr size_t page_size = 4096u;
    static constexpr size_t encoded_size = 24u;
    static constexpr size_t completed_at = 8u;
    static constexpr size_t refused_at = 16u;

    PresentProgress();
    PresentProgress(uint64_t completed, uint64_t refused);

    uint64_t completed() const;
    uint64_t refused() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    static constexpr uint32_t magic = 0x00505641u;
    static constexpr uint32_t version = 1u;

    uint64_t completed_;
    uint64_t refused_;
};

}

#endif

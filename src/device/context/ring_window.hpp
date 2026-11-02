#ifndef AV_RING_WINDOW_HPP
#define AV_RING_WINDOW_HPP

#include "core/support/status.hpp"
#include "device/memory/blob_window.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class RingHeader;

class RingWindow {
public:
    RingWindow();
    RingWindow(const BlobWindow &blob, uint8_t order);

    uint8_t  order() const;
    uint64_t capacity() const;
    [[nodiscard]] Status snapshot(RingHeader &header) const;
    [[nodiscard]] Status copy(uint64_t counter, uint8_t *bytes, size_t count) const;
    [[nodiscard]] Status publish_head(uint64_t head) const;

    [[nodiscard]] Status announce_awake(bool awake) const;

    bool invariants_hold() const;

private:
    static constexpr uint64_t tail_offset = 8u;
    static constexpr uint64_t head_offset = 16u;
    static constexpr uint64_t awake_offset = 5u;

    BlobWindow blob_;
    uint8_t    order_;
};

}

#endif

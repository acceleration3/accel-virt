/* The head of the display clock page (docs/protocol.md, "Display clock"): what
 * the device says the page is, and how many head records follow. The device
 * writes it once when the page is registered; the guest ignores a page whose
 * header it does not accept. */
#ifndef AV_CLOCK_PAGE_HEADER_HPP
#define AV_CLOCK_PAGE_HEADER_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ClockPageHeader {
public:
    static constexpr size_t   page_size = 4096u;
    static constexpr size_t   encoded_size = 32u;
    static constexpr size_t   record_size = 64u;
    /* VIRTIO_GPU_MAX_SCANOUTS. */
    static constexpr uint32_t max_heads = 16u;

    ClockPageHeader();
    explicit ClockPageHeader(uint32_t heads);

    uint32_t heads() const;
    /* Where head `head`'s record starts in the page; the caller keeps `head`
     * below heads(). */
    size_t   record_offset(uint32_t head) const;

    /* too_large for less room than the header, invalid_parameter for a head
     * count no page describes. */
    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* malformed for a wrong magic or version, a head count of 0 or past
     * max_heads, or reserved bytes that are not 0. Unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    static constexpr uint32_t magic = 0x434B5652u;   /* "RVKC" */
    static constexpr uint32_t version = 1u;
    static constexpr size_t   reserved_bytes = 20u;

    uint32_t heads_;
};

}  // namespace av

#endif  // AV_CLOCK_PAGE_HEADER_HPP

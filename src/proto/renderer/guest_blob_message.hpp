#ifndef AV_GUEST_BLOB_MESSAGE_HPP
#define AV_GUEST_BLOB_MESSAGE_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class RendererExtent;

class GuestBlobMessage {
public:
    static constexpr size_t   fixed_size = 16u;
    static constexpr uint32_t max_extents = 16384u;
    static constexpr uint32_t descriptors = 0u;

    GuestBlobMessage();
    GuestBlobMessage(uint32_t blob, uint64_t size, const RendererExtent *extents, uint32_t count);

    uint32_t       blob() const;
    uint64_t       size() const;
    uint32_t       count() const;
    RendererExtent extent(uint32_t index) const;
    size_t         encoded_length() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count,
                                RendererExtent *extents, uint32_t capacity);

private:
    bool fields_valid() const;
    bool extents_cover_size() const;

    uint32_t              blob_;
    uint64_t              size_;
    const RendererExtent *extents_;
    uint32_t              count_;
};

}

#endif

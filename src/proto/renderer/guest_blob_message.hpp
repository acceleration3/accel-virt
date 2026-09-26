/* QEMU -> renderer: a guest blob, as extents of the regions the renderer has
 * mapped, so its BlobTable mirrors QEMU's without either reading the other's
 * page tables. The extents are the caller's: an encoder's storage, or the
 * array decode filled; they must outlive the message. */
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
    /* The device's own bound on a guest blob's memory entries. */
    static constexpr uint32_t max_extents = 16384u;
    static constexpr uint32_t descriptors = 0u;

    GuestBlobMessage();
    /* A null `extents` is none. */
    GuestBlobMessage(uint32_t blob, uint64_t size, const RendererExtent *extents, uint32_t count);

    uint32_t       blob() const;
    uint64_t       size() const;
    uint32_t       count() const;
    /* An empty extent past the last. */
    RendererExtent extent(uint32_t index) const;
    size_t         encoded_length() const;

    /* too_large for less room than encoded_length(); invalid_parameter for a
     * message decode would refuse. */
    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* Reads the extents into `extents`, room for `capacity` of them, which
     * hold nothing meaningful unless ok - so not the array this message
     * views. no_space for more extents than that; malformed for any
     * descriptor, blob 0, an empty blob, no extents or more than
     * max_extents, a length other than they give, an extent that does not fit
     * its region, or lengths that do not add up to the size. Unchanged unless
     * ok. */
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

}  // namespace av

#endif  // AV_GUEST_BLOB_MESSAGE_HPP

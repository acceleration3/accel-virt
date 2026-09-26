/* renderer -> QEMU: the answer to export_image. The image is found or not;
 * when it is, its layout, and - only when it is not the image last sent for
 * its slot - the dma-buf beside the message. QEMU keeps the descriptor it was
 * last sent for each slot, since its display imports an image once per
 * descriptor. */
#ifndef AV_EXPORTED_IMAGE_MESSAGE_HPP
#define AV_EXPORTED_IMAGE_MESSAGE_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ExportedImageMessage {
public:
    static constexpr size_t   encoded_size = 40u;
    /* The executor exports into two images, alternately. */
    static constexpr uint32_t slots = 2u;

    /* Nothing is published under the key. */
    ExportedImageMessage();
    ExportedImageMessage(bool fresh, uint32_t slot, uint32_t width, uint32_t height, uint32_t stride, uint32_t offset,
                         uint32_t fourcc, uint64_t modifier);

    bool     found() const;
    /* A descriptor comes with the message. */
    bool     fresh() const;
    uint32_t slot() const;
    uint32_t width() const;
    uint32_t height() const;
    uint32_t stride() const;
    uint32_t offset() const;
    uint32_t fourcc() const;
    uint64_t modifier() const;
    uint32_t descriptors() const;

    /* too_large for less room than encoded_size; invalid_parameter for a
     * message decode would refuse. */
    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* malformed for any other length, a flag other than 0 or 1, a found image
     * with no size or a slot past the last, anything but zeros when nothing
     * was found, or a descriptor count other than descriptors(). Unchanged
     * unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count);

private:
    bool fields_valid() const;

    uint32_t found_;
    uint32_t fresh_;
    uint32_t slot_;
    uint32_t width_;
    uint32_t height_;
    uint32_t stride_;
    uint32_t offset_;
    uint32_t fourcc_;
    uint64_t modifier_;
};

}  // namespace av

#endif  // AV_EXPORTED_IMAGE_MESSAGE_HPP

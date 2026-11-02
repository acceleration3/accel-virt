#ifndef AV_EXPORTED_IMAGE_MESSAGE_HPP
#define AV_EXPORTED_IMAGE_MESSAGE_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ExportedImageMessage {
public:
    static constexpr size_t   encoded_size = 40u;
    static constexpr uint32_t slots = 2u;

    ExportedImageMessage();
    ExportedImageMessage(bool fresh, uint32_t slot, uint32_t width, uint32_t height, uint32_t stride, uint32_t offset,
                         uint32_t fourcc, uint64_t modifier);

    bool     found() const;
    bool     fresh() const;
    uint32_t slot() const;
    uint32_t width() const;
    uint32_t height() const;
    uint32_t stride() const;
    uint32_t offset() const;
    uint32_t fourcc() const;
    uint64_t modifier() const;
    uint32_t descriptors() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
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

}

#endif

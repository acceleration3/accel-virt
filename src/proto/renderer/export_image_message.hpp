#ifndef AV_EXPORT_IMAGE_MESSAGE_HPP
#define AV_EXPORT_IMAGE_MESSAGE_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ExportImageMessage {
public:
    static constexpr size_t   encoded_size = 8u;
    static constexpr uint32_t descriptors = 0u;

    ExportImageMessage();
    explicit ExportImageMessage(uint64_t key);

    uint64_t key() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count);

private:
    uint64_t key_;
};

}

#endif

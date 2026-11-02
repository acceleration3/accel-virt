#ifndef AV_BLOB_RETIRED_MESSAGE_HPP
#define AV_BLOB_RETIRED_MESSAGE_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class BlobRetiredMessage {
public:
    static constexpr size_t   encoded_size = 4u;
    static constexpr uint32_t descriptors = 0u;

    BlobRetiredMessage();
    explicit BlobRetiredMessage(uint32_t blob);

    uint32_t blob() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count);

private:
    uint32_t blob_;
};

}

#endif

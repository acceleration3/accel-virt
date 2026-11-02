#ifndef AV_ALLOCATE_MEMORY_MESSAGE_HPP
#define AV_ALLOCATE_MEMORY_MESSAGE_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class AllocateMemoryMessage {
public:
    static constexpr size_t encoded_size = 24u;

    AllocateMemoryMessage();
    AllocateMemoryMessage(uint32_t context, uint32_t request, uint64_t key, uint64_t size);

    uint32_t context() const;
    uint32_t request() const;
    uint64_t key() const;
    uint64_t size() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count);

private:
    bool fields_valid() const;

    uint32_t context_;
    uint32_t request_;
    uint64_t key_;
    uint64_t size_;
};

}

#endif

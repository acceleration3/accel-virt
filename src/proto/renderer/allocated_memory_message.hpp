#ifndef AV_ALLOCATED_MEMORY_MESSAGE_HPP
#define AV_ALLOCATED_MEMORY_MESSAGE_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class AllocatedMemoryMessage {
public:
    static constexpr size_t   encoded_size = 16u;
    static constexpr uint32_t write_back = 1u;
    static constexpr uint32_t write_combined = 2u;

    AllocatedMemoryMessage();
    AllocatedMemoryMessage(uint32_t caching, uint64_t size);

    bool     allocated() const;
    uint32_t caching() const;
    uint64_t size() const;
    uint32_t descriptors() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count);

private:
    bool fields_valid() const;

    uint32_t caching_;
    uint64_t size_;
};

}

#endif

#ifndef AV_HOST_MEMORY_REQUEST_HPP
#define AV_HOST_MEMORY_REQUEST_HPP

#include <stdint.h>

namespace av {

class HostMemoryRequest {
public:
    enum class Kind : uint8_t { upload = 1u, readback = 2u, vulkan = 3u, placed = 4u, placed_heap = 5u };

    static constexpr uint32_t memory_types = 32u;

    HostMemoryRequest();
    HostMemoryRequest(Kind kind, uint32_t vulkan_type);
    explicit HostMemoryRequest(uint32_t encoded);

    bool     valid() const;
    bool     places() const;
    Kind     kind() const;
    uint32_t vulkan_type() const;
    uint32_t encoded() const;

private:
    static constexpr uint32_t kind_mask = 0xFFu;
    static constexpr uint32_t type_shift = 8u;

    uint32_t encoded_;
};

}

#endif

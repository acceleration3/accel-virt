/* What kind of host GPU memory a guest asks for (docs/host-memory.md): one
 * the CPU writes and the GPU reads, one the CPU reads back, or exactly one of
 * the host GPU's Vulkan memory types. The escape that opens a chunk carries
 * it, the kernel passes it on as the blob id of RESOURCE_CREATE_BLOB, and the
 * device hands it to the executor's memory source, so it has one encoding on
 * every boundary and every side judges it the same way. */
#ifndef AV_HOST_MEMORY_REQUEST_HPP
#define AV_HOST_MEMORY_REQUEST_HPP

#include <stdint.h>

namespace av {

class HostMemoryRequest {
public:
    enum class Kind : uint8_t { upload = 1u, readback = 2u, vulkan = 3u };

    /* VK_MAX_MEMORY_TYPES. */
    static constexpr uint32_t memory_types = 32u;

    /* No request, which nothing accepts. */
    HostMemoryRequest();
    /* `vulkan_type` names a memory type for Kind::vulkan and is 0 otherwise. */
    HostMemoryRequest(Kind kind, uint32_t vulkan_type);
    /* From the wire; valid() says whether it is one. */
    explicit HostMemoryRequest(uint32_t encoded);

    /* A known kind, and a type below memory_types for vulkan and 0 for the
     * others: anything else came from a guest that is not this driver. */
    bool     valid() const;
    Kind     kind() const;
    uint32_t vulkan_type() const;
    uint32_t encoded() const;

private:
    static constexpr uint32_t kind_mask = 0xFFu;
    static constexpr uint32_t type_shift = 8u;

    uint32_t encoded_;
};

}  // namespace av

#endif  // AV_HOST_MEMORY_REQUEST_HPP

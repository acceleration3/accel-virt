/* The allocation descriptor of one client allocation the suite asks for
 * (docs/protocol.md): CPU visible, no surface, `bytes` long - encoded, and
 * corruptible into one the driver must refuse. */
#ifndef AV_CLIENT_DESCRIPTOR_HPP
#define AV_CLIENT_DESCRIPTOR_HPP

#include "proto/resources/allocation_descriptor.hpp"

#include <stdint.h>

namespace av {

class ClientDescriptor {
public:
    static constexpr uint64_t page_bytes = 4096u;

    explicit ClientDescriptor(uint64_t bytes);

    /* A magic no driver accepts. */
    void           corrupt_magic();
    const uint8_t *bytes() const;
    uint32_t       size() const;
    uint64_t       pages() const;

private:
    uint8_t  bytes_[AllocationDescriptor::encoded_size];
    uint64_t pages_;
};

}  // namespace av

#endif  // AV_CLIENT_DESCRIPTOR_HPP

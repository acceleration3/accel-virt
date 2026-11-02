#ifndef AV_CLIENT_DESCRIPTOR_HPP
#define AV_CLIENT_DESCRIPTOR_HPP

#include "proto/resources/allocation_descriptor.hpp"

#include "proto/resources/allocation_kind.hpp"

#include <stdint.h>

namespace av {

class ClientDescriptor {
public:
    static constexpr uint64_t page_bytes = 4096u;

    explicit ClientDescriptor(uint64_t bytes);
    ClientDescriptor(uint64_t bytes, AllocationKind kind);

    void           corrupt_magic();
    const uint8_t *bytes() const;
    uint32_t       size() const;
    uint64_t       pages() const;

private:
    uint8_t  bytes_[AllocationDescriptor::encoded_size];
    uint64_t pages_;
};

}

#endif

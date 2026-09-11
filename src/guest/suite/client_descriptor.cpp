#include "guest/suite/client_descriptor.hpp"

#include "proto/resources/allocation_kind.hpp"
#include "proto/resources/scanout_target.hpp"
#include "proto/resources/surface_layout.hpp"

namespace av {

ClientDescriptor::ClientDescriptor(uint64_t bytes)
    : bytes_{}, pages_((bytes + page_bytes - 1u) / page_bytes)
{
    /* Sized for it, and a client allocation of any non-zero size is valid. */
    static_cast<void>(AllocationDescriptor(AllocationKind::client, true, SurfaceLayout(), bytes, ScanoutTarget())
                          .encode(bytes_, sizeof bytes_));
}

void ClientDescriptor::corrupt_magic() { bytes_[0] = static_cast<uint8_t>(~bytes_[0]); }

const uint8_t *ClientDescriptor::bytes() const { return bytes_; }
uint32_t       ClientDescriptor::size() const  { return AllocationDescriptor::encoded_size; }
uint64_t       ClientDescriptor::pages() const { return pages_; }

}  // namespace av

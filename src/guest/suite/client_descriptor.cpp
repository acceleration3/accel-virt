#include "guest/suite/client_descriptor.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/resources/allocation_kind.hpp"
#include "proto/resources/scanout_target.hpp"
#include "proto/resources/surface_layout.hpp"

namespace av {

ClientDescriptor::ClientDescriptor(uint64_t bytes) : ClientDescriptor(bytes, AllocationKind::client)
{
    AV_SCOPE("ClientDescriptor::ClientDescriptor");
}

ClientDescriptor::ClientDescriptor(uint64_t bytes, AllocationKind kind)
    : bytes_{}, pages_((bytes + page_bytes - 1u) / page_bytes)
{
    AV_SCOPE("ClientDescriptor::ClientDescriptor");
    /* Sized for it, and a client allocation of any non-zero size is valid. */
    static_cast<void>(AllocationDescriptor(kind, true, SurfaceLayout(), bytes, ScanoutTarget())
                          .encode(bytes_, sizeof bytes_));
}

void ClientDescriptor::corrupt_magic()
{
    AV_SCOPE("ClientDescriptor::corrupt_magic");
    bytes_[0] = static_cast<uint8_t>(~bytes_[0]);
}

const uint8_t *ClientDescriptor::bytes() const
{
    AV_SCOPE("ClientDescriptor::bytes");
    return bytes_;
}
uint32_t       ClientDescriptor::size() const
{
    AV_SCOPE("ClientDescriptor::size");
    return AllocationDescriptor::encoded_size;
}
uint64_t       ClientDescriptor::pages() const
{
    AV_SCOPE("ClientDescriptor::pages");
    return pages_;
}

}  // namespace av

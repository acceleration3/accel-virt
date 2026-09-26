/* A guest blob QEMU mirrors into rvk-renderer, fed attacker-chosen bytes: the
 * first byte is how many descriptors arrived beside it and the rest is its
 * body. What decodes must re-encode to exactly the bytes read, and every
 * extent must end inside its region and the extents cover the blob exactly -
 * the renderer reads guest memory through them. */
#include "core/support/status.hpp"
#include "proto/renderer/guest_blob_message.hpp"
#include "proto/renderer/renderer_extent.hpp"
#include "proto/renderer/renderer_message_header.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    if (size == 0u) return 0;
    static av::RendererExtent extents[av::GuestBlobMessage::max_extents];
    av::GuestBlobMessage      blob;
    if (blob.decode(data + 1u, size - 1u, data[0], extents, av::GuestBlobMessage::max_extents) != av::Status::ok)
        return 0;
    static std::uint8_t again[av::RendererMessageHeader::max_body_length];
    if (blob.encoded_length() != size - 1u || blob.encode(again, sizeof again) != av::Status::ok) __builtin_trap();
    if (std::memcmp(again, data + 1u, size - 1u) != 0) __builtin_trap();
    std::uint64_t covered = 0u;
    for (std::uint32_t index = 0u; index < blob.count(); index++) {
        const av::RendererExtent extent = blob.extent(index);
        if (!extent.fits() || extent.offset() + extent.length() > av::RendererExtent::region_span) __builtin_trap();
        covered += extent.length();
    }
    if (blob.count() == 0u || covered != blob.size()) __builtin_trap();
    return 0;
}

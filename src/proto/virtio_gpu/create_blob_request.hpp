/* The fixed part of a virtio-gpu RESOURCE_CREATE_BLOB request (virtio 1.2
 * 5.7.6.8): the context it is made for (the header's ctx_id), the resource,
 * its memory kind and flags, how many memory entries follow, the context
 * object it names, and its size. The entries follow at `fixed_size`,
 * `entry_size` bytes each. A host blob's context object is the
 * HostMemoryRequest it is made of (docs/host-memory.md). */
#ifndef AV_CREATE_BLOB_REQUEST_HPP
#define AV_CREATE_BLOB_REQUEST_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class WireReader;

class CreateBlobRequest {
public:
    static constexpr size_t   fixed_size = 56u;
    static constexpr size_t   entry_size = 16u;
    static constexpr uint32_t memory_guest = 1u;          /* VIRTIO_GPU_BLOB_MEM_GUEST */
    static constexpr uint32_t memory_host3d = 2u;         /* VIRTIO_GPU_BLOB_MEM_HOST3D */
    /* Host GPU memory over guest pages: the pages a process lent, which the
     * device makes host memory where they are (docs/host-memory.md,
     * "Application memory"). Created with no entries; RESOURCE_ATTACH_BACKING
     * then names the pages, in order, a piece at a time. */
    static constexpr uint32_t memory_host3d_guest = 3u;   /* VIRTIO_GPU_BLOB_MEM_HOST3D_GUEST */
    static constexpr uint32_t flag_mappable = 1u;         /* VIRTIO_GPU_BLOB_FLAG_USE_MAPPABLE */
    static constexpr uint32_t flag_shareable = 2u;
    static constexpr uint32_t flag_cross_device = 4u;

    CreateBlobRequest();

    /* From the start of the request; malformed when shorter than fixed_size. */
    [[nodiscard]] Status decode(WireReader &reader);
    uint32_t             context_id() const;
    uint32_t             resource_id() const;
    uint32_t             memory() const;
    uint32_t             flags() const;
    uint32_t             entries() const;
    uint64_t             blob_id() const;
    uint64_t             size() const;

private:
    static constexpr size_t header_before_context = 16u;
    static constexpr size_t header_after_context = 4u;

    uint32_t context_id_;
    uint32_t resource_id_;
    uint32_t memory_;
    uint32_t flags_;
    uint32_t entries_;
    uint64_t blob_id_;
    uint64_t size_;
};

}  // namespace av

#endif  // AV_CREATE_BLOB_REQUEST_HPP

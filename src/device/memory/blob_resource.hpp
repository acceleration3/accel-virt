/* One blob resource: guest-backed pages the device holds mapped, or a
 * host-visible block the guest reaches through the hostmem region once mapped. */
#ifndef AV_BLOB_RESOURCE_HPP
#define AV_BLOB_RESOURCE_HPP

#include "core/support/status.hpp"
#include "device/memory/guest_page_mapping.hpp"
#include "device/memory/host_memory_block.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ControlRequest;
class Host;

class BlobResource {
public:
    BlobResource(Host &host, uint32_t id);
    ~BlobResource();
    BlobResource(const BlobResource &) = delete;
    BlobResource &operator=(const BlobResource &) = delete;

    /* Main loop. The GuestPageMapping's answer, or invalid_parameter when the
     * entries do not cover `size`. */
    [[nodiscard]] Status back_with_guest(ControlRequest &request, size_t offset, uint32_t entries, uint64_t size);
    [[nodiscard]] Status back_with_host(uint64_t size);
    /* Main loop. invalid_parameter unless host-visible and unmapped; the host's
     * answer otherwise. The caller has judged the offset. */
    [[nodiscard]] Status map(uint64_t offset);
    /* invalid_state unless mapped. */
    [[nodiscard]] Status unmap();

    uint32_t id() const;
    uint64_t size() const;
    bool     mapped() const;
    /* Where in the hostmem region it is mapped. */
    uint64_t offset() const;
    /* Whether this blob, mapped, covers part of [offset, offset + size). */
    bool     overlaps(uint64_t offset, uint64_t size) const;
    /* A context bound to this blob holds a pin; a pinned blob is not released. */
    void     pin();
    void     unpin();
    bool     pinned() const;

private:
    Host            &host_;
    uint32_t         id_;
    GuestPageMapping guest_;
    HostMemoryBlock  block_;
    uint64_t         offset_;
    bool             mapped_;
    uint32_t         pins_;
};

}  // namespace av

#endif  // AV_BLOB_RESOURCE_HPP

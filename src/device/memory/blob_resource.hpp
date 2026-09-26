/* One blob resource: guest-backed pages the device holds mapped, or a
 * host-visible block the guest reaches through the hostmem region once mapped. */
#ifndef AV_BLOB_RESOURCE_HPP
#define AV_BLOB_RESOURCE_HPP

#include "core/support/status.hpp"
#include "device/memory/guest_page_alias.hpp"
#include "device/memory/guest_page_mapping.hpp"
#include "device/memory/host_memory_block.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class BlobWindow;
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
    /* The blob's memory for a worker: valid while pinned. */
    BlobWindow window() const;
    /* The host memory behind a host blob, or null for guest pages. */
    void      *host_memory() const;
    /* Any thread, while nothing can release it: its guest pages as one host
     * range, made for the first holder and shared by the rest, and as the
     * extents they are - a holder needs both, and only the hold keeps either
     * valid. invalid_parameter for a blob the host backs; the alias's answer
     * when it cannot be made, holding nothing. */
    [[nodiscard]] Status hold_alias(const GuestPageAlias *&alias, BlobWindow &pages);
    /* Any thread, under the same rule. One hold let go; the range goes with
     * the last, and a blob no one holds is left as it is. */
    void                 release_alias();
    /* An aliased blob, like a pinned one, is not released: its pages are in a
     * host API's hands. */
    bool                 aliased() const;

private:
    Host            &host_;
    uint32_t         id_;
    GuestPageMapping guest_;
    HostMemoryBlock  block_;
    uint64_t         offset_;
    bool             mapped_;
    uint32_t         pins_;
    GuestPageAlias   alias_;
    uint32_t         alias_holds_;
};

}  // namespace av

#endif  // AV_BLOB_RESOURCE_HPP

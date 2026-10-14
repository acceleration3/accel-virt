/* One host range over a guest blob's pages, which the guest scattered wherever
 * its allocator put them (feature 045, research.md section 7): a display
 * surface is drawn from contiguous memory, so each run of the blob's extents
 * is mapped a second time, in order, into one reserved range
 * (BlobResource::view). The pages are the guest's own - no byte is copied,
 * and a byte written through either address is read through the other. No
 * host GPU is handed them (docs/host-memory.md). */
#ifndef AV_GUEST_PAGE_ALIAS_HPP
#define AV_GUEST_PAGE_ALIAS_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class BlobWindow;
class Host;
struct GuestPageAliasProbe;

class GuestPageAlias {
public:
    /* The unit a run is aliased in: a host page. */
    static constexpr uint64_t page_size = 4096u;
    /* A 64 MiB chunk scattered page by page is 16384 runs, and each run is a
     * mapping of the QEMU process, whose count Linux caps (vm.max_map_count,
     * 65530 by default): one blob may not spend more than a chunk's worst case. */
    static constexpr uint32_t max_runs = 16384u;

    explicit GuestPageAlias(Host &host);
    ~GuestPageAlias();
    GuestPageAlias(const GuestPageAlias &) = delete;
    GuestPageAlias &operator=(const GuestPageAlias &) = delete;

    /* Any thread. One range over `pages`, a guest blob's extents, which must
     * stay mapped while it is made. invalid_state once mapped; malformed for no extents
     * or one that is not whole pages; no_memory for more runs than max_runs or a
     * host with no range to give; not_supported when the host cannot alias a
     * run. Holds nothing unless ok. */
    [[nodiscard]] Status map(const BlobWindow &pages);
    /* Any thread. */
    void                 release();
    bool                 mapped() const;
    /* The range's first byte; null until mapped. */
    void                *base() const;
    uint64_t             length() const;
    uint32_t             runs() const;

    bool invariants_hold() const;

private:
    /* Every run of `pages` aliased in order from the base. */
    [[nodiscard]] Status map_runs(const BlobWindow &pages);

    Host    &host_;
    void    *base_;
    uint64_t length_;
    uint32_t runs_;

    friend struct GuestPageAliasProbe;
};

}  // namespace av

#endif  // AV_GUEST_PAGE_ALIAS_HPP

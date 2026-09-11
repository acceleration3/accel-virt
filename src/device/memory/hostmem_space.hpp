/* The hostmem region host-visible blobs are placed in (the hostmem BAR): which
 * placements fit it. Overlap between blobs is the blob table's to judge. */
#ifndef AV_HOSTMEM_SPACE_HPP
#define AV_HOSTMEM_SPACE_HPP

#include <stdint.h>

namespace av {

class Host;

class HostmemSpace {
public:
    static constexpr uint64_t page_size = 4096u;

    explicit HostmemSpace(Host &host);
    HostmemSpace(const HostmemSpace &) = delete;
    HostmemSpace &operator=(const HostmemSpace &) = delete;

    /* A page-aligned `offset` whose `size` bytes lie wholly inside the region. */
    bool admits(uint64_t offset, uint64_t size) const;

private:
    Host &host_;
};

}  // namespace av

#endif  // AV_HOSTMEM_SPACE_HPP

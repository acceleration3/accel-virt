/* One dma-buf over a blob's memory: a guest blob's pages, or the memfd behind
 * a host blob (docs/host-window.md).
 *
 * The same pages GuestPageAlias maps a second time, handed to a host GPU as a
 * dma-buf instead of as a pointer. Both exist because the two imports fail in
 * different places: a host pointer is a userptr, and the GPU driver that
 * matters here refuses to pin one whose pages are file-backed - which guest
 * RAM always is (docs/testing.md), and a host blob's memfd is too. A dma-buf
 * carries the same pages with no pinning at all.
 *
 * A guest blob's extents become the dma-buf's items, in order, so the host GPU
 * sees one contiguous range exactly as the alias gives one. No byte is copied. */
#ifndef AV_BLOB_DMA_BUF_HPP
#define AV_BLOB_DMA_BUF_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class BlobWindow;
class Host;
struct BlobDmaBufProbe;

class BlobDmaBuf {
public:
    /* What a dma-buf over guest pages is made of on this host. */
    static constexpr uint64_t page_size = 4096u;
    /* One item per extent, bounded as GuestPageAlias bounds its runs: a blob
     * scattered page by page is the worst case either has to carry. */
    static constexpr uint32_t max_items = 16384u;

    explicit BlobDmaBuf(Host &host);
    ~BlobDmaBuf();
    BlobDmaBuf(const BlobDmaBuf &) = delete;
    BlobDmaBuf &operator=(const BlobDmaBuf &) = delete;

    /* Any thread. One dma-buf over `size` bytes from `offset` of `pages`,
     * whose extents must stay mapped while it is made. The range is exactly
     * what the dma-buf carries, because a Vulkan fd import names no offset:
     * whoever imports it gets these bytes and no others.
     *
     * invalid_state once made; too_large for a range past the blob; malformed
     * for a range or an extent that is not whole pages; no_memory for more
     * items than max_items; not_supported where the host cannot make one -
     * RAM with no file behind it, or no udmabuf - which is the caller's
     * signal to import a pointer instead. Holds nothing unless ok. */
    [[nodiscard]] Status make(const BlobWindow &pages, uint64_t offset, uint64_t size);
    /* Any thread. The same over `size` bytes from `offset` of host memory
     * create_host_memory made, `length` bytes long: invalid_state once made;
     * too_large for a range past it; malformed for a range that is not whole
     * pages; not_supported where the host cannot make one. Holds nothing
     * unless ok. */
    [[nodiscard]] Status make_over_host(void *memory, uint64_t length, uint64_t offset, uint64_t size);
    /* Any thread. Safe on one that holds nothing. */
    void                 release();
    /* The descriptor, and this no longer holds it. A Vulkan fd import takes
     * ownership on success, so what was imported must not be closed here as
     * well; -1 when there is nothing to give. */
    int                  take();

    bool     made() const;
    /* The descriptor, or -1 until made. It stays this object's: a backend
     * that imports it duplicates or is handed it for the import alone. */
    int      handle() const;
    uint64_t length() const;
    uint32_t items() const;

    bool invariants_hold() const;

private:
    Host    &host_;
    int      handle_;
    uint64_t length_;
    uint32_t items_;

    friend struct BlobDmaBufProbe;
};

}  // namespace av

#endif  // AV_BLOB_DMA_BUF_HPP

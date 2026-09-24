/* A pinned blob's memory as a worker reaches it: the extents of its guest
 * pages or its one host block, read and written by offset. It holds no
 * mapping of its own, so it is valid only while the blob it came from is
 * pinned; any thread may use it. */
#ifndef AV_BLOB_WINDOW_HPP
#define AV_BLOB_WINDOW_HPP

#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

struct BlobWindowProbe;

class BlobWindow {
public:
    BlobWindow();
    BlobWindow(void *const *pointers, const uint64_t *lengths, uint32_t extents, uint64_t length);

    uint64_t length() const;
    uint32_t extents() const;
    /* Extent `index`'s memory, with `bytes` its length: null and 0 past the
     * last, so a walk over them cannot read beyond the table. */
    void    *extent(uint32_t index, uint64_t &bytes) const;
    /* invalid_parameter, copying nothing, unless [offset, offset + count) lies
     * inside the blob. */
    [[nodiscard]] Status read(uint64_t offset, uint8_t *bytes, size_t count) const;
    [[nodiscard]] Status write(uint64_t offset, const uint8_t *bytes, size_t count) const;
    /* The eight bytes at `offset` in one load or one store: a field another
     * processor stores whole - a ring's tail, its head - is never seen half
     * old and half new, which a copy a byte at a time cannot promise. In the
     * machine's byte order, little-endian as the wire's on every machine this
     * runs on. invalid_parameter, touching nothing, unless the eight bytes
     * are aligned and in one extent. */
    [[nodiscard]] Status load_u64(uint64_t offset, uint64_t &value) const;
    [[nodiscard]] Status store_u64(uint64_t offset, uint64_t value) const;

    bool invariants_hold() const;

private:
    bool contains(uint64_t offset, size_t count) const;
    /* The aligned word at `offset`, null unless one extent holds all of it. */
    uint64_t *word(uint64_t offset) const;
    /* Calls copy(extent_bytes, done, chunk) for each piece of the range. */
    template <typename Copy>
    void walk(uint64_t offset, size_t count, Copy copy) const;

    void *const    *pointers_;
    const uint64_t *lengths_;
    uint32_t        extents_;
    uint64_t        length_;

    friend struct BlobWindowProbe;
};

template <typename Copy>
void BlobWindow::walk(uint64_t offset, size_t count, Copy copy) const
{
    AV_SCOPE("BlobWindow::walk");
    size_t done = 0u;
    for (uint32_t extent = 0u; done < count; extent++) {
        if (offset >= lengths_[extent]) {
            offset -= lengths_[extent];
            continue;
        }
        const uint64_t available = lengths_[extent] - offset;
        const size_t   chunk = available < count - done ? static_cast<size_t>(available) : count - done;
        copy(static_cast<uint8_t *>(pointers_[extent]) + offset, done, chunk);
        done += chunk;
        offset = 0u;
    }
}

}  // namespace av

#endif  // AV_BLOB_WINDOW_HPP

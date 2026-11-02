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
    void    *extent(uint32_t index, uint64_t &bytes) const;
    [[nodiscard]] Status read(uint64_t offset, uint8_t *bytes, size_t count) const;
    [[nodiscard]] Status write(uint64_t offset, const uint8_t *bytes, size_t count) const;
    [[nodiscard]] Status load_u64(uint64_t offset, uint64_t &value) const;
    [[nodiscard]] Status store_u64(uint64_t offset, uint64_t value) const;

    bool invariants_hold() const;

private:
    bool contains(uint64_t offset, size_t count) const;
    uint64_t *word(uint64_t offset) const;
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

}

#endif

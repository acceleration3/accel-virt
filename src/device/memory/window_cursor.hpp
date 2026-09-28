/* A BlobWindow walked forward. BlobWindow finds an offset by walking its
 * extents from the first, which for a surface read a row at a time is a walk
 * per row across every page before it - quadratic in the surface. This
 * remembers the extent the last offset was in and goes on from there, so rows
 * asked for in order cost a step each; an offset before the last starts over.
 * It holds no mapping of its own: valid while the window's pages are. */
#ifndef AV_WINDOW_CURSOR_HPP
#define AV_WINDOW_CURSOR_HPP

#include "core/support/status.hpp"
#include "device/memory/blob_window.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class WindowCursor {
public:
    explicit WindowCursor(const BlobWindow &window);

    /* invalid_parameter, copying nothing, unless [offset, offset + count) lies
     * inside the window; a window whose extents hold less than its length
     * says so where they end. */
    [[nodiscard]] Status read(uint64_t offset, uint8_t *bytes, size_t count);
    [[nodiscard]] Status write(uint64_t offset, const uint8_t *bytes, size_t count);

private:
    /* The byte at `offset`, with `bytes` how many of the extent's follow from
     * it; null and 0 past the end. */
    uint8_t *at(uint64_t offset, uint64_t &bytes);
    bool     contains(uint64_t offset, size_t count) const;

    BlobWindow window_;
    uint32_t   extent_;
    /* Where extent_ starts in the window. */
    uint64_t   start_;
};

}  // namespace av

#endif  // AV_WINDOW_CURSOR_HPP

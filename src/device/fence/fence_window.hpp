/* One context's fence page, in the pinned blob the setup named
 * (docs/protocol.md). A context's reporter only ever writes it; the page is
 * read back only when the reporter is in another process that died, so the
 * loss can be reported with the fences it had already completed. Any
 * thread. */
#ifndef AV_FENCE_WINDOW_HPP
#define AV_FENCE_WINDOW_HPP

#include "core/support/status.hpp"
#include "device/memory/blob_window.hpp"

#include <stdint.h>

namespace av {

class FencePage;

class FenceWindow {
public:
    FenceWindow();
    explicit FenceWindow(const BlobWindow &blob);

    /* Everything but the beat word, which is the pulse's alone: a worker that
     * wrote it back would carry it backwards past a beat stored meanwhile.
     * invalid_parameter when the blob cannot hold a page. */
    [[nodiscard]] Status publish(const FencePage &page) const;
    /* The beat word alone, in one store. */
    [[nodiscard]] Status publish_beat(uint64_t beat) const;
    /* What was last published: malformed for a page that does not decode. */
    [[nodiscard]] Status read(FencePage &page) const;

    bool invariants_hold() const;

private:
    BlobWindow blob_;
};

}  // namespace av

#endif  // AV_FENCE_WINDOW_HPP

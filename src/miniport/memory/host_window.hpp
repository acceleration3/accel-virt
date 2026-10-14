/* The device's host window (docs/host-memory.md): which pages of its hostmem
 * region hold a chunk. The driver places every window blob itself - VidMm is
 * never told the region exists - so this page map is the only record of what
 * is free. One per adapter. */
#ifndef AV_HOST_WINDOW_HPP
#define AV_HOST_WINDOW_HPP

#include "core/platform/mutex.hpp"
#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

struct HostWindowProbe;

class HostWindow {
public:
    static constexpr uint64_t page_bytes = 0x1000u;

    explicit HostWindow(Platform &platform);
    ~HostWindow();
    HostWindow(const HostWindow &) = delete;
    HostWindow &operator=(const HostWindow &) = delete;

    /* PASSIVE. The region at guest physical `base`, `bytes` long, offered from
     * now on. The page map is allocated the first time and kept for the
     * adapter's life: what was taken before a withdraw is still given back into
     * it. invalid_parameter for less than a page or more pages than a page
     * index holds; invalid_state for another size than the map was made for,
     * or another base while pages are taken; no_memory. */
    [[nodiscard]] Status initialize(uint64_t base, uint64_t bytes);
    /* PASSIVE. Nothing more is taken until the next initialize; what was
     * taken can still be given back. */
    void                 withdraw();
    bool                 offered() const;
    /* PASSIVE. The first run of `pages` free pages, in `first`. not_supported
     * when the window is not offered, invalid_parameter for no pages, no_space
     * when no run is that long. */
    [[nodiscard]] Status take(uint32_t pages, uint32_t &first);
    /* PASSIVE. Pages taken from `first` on are free again; a page not taken,
     * or past the window, is left as it is. */
    void                 give_back(uint32_t first, uint32_t pages);
    /* Where page `first` of the window is in guest physical memory. */
    uint64_t             physical(uint32_t first) const;
    uint32_t             pages_in_use() const;

    /* As many pages are in use as the map has bits set. */
    bool invariants_hold() const;

private:
    static constexpr uint32_t bits_per_word = 64u;

    /* Marks the run from `first` taken. */
    void mark(uint32_t first, uint32_t pages);

    Platform &platform_;
    Mutex     lock_;
    uint64_t *map_;
    uint64_t  base_;
    uint32_t  pages_;
    uint32_t  used_;
    bool      offered_;

    friend struct HostWindowProbe;
};

}  // namespace av

#endif  // AV_HOST_WINDOW_HPP

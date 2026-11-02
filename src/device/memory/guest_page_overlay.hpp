#ifndef AV_GUEST_PAGE_OVERLAY_HPP
#define AV_GUEST_PAGE_OVERLAY_HPP

#include "core/platform/block_source.hpp"
#include "core/support/growable_array.hpp"
#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class BlockSource;
class ControlRequest;
class GuestRamLending;
class Host;
class LentPages;
struct GuestPageOverlayProbe;

class GuestPageOverlay {
public:
    static constexpr size_t   entry_size = 16u;
    static constexpr uint64_t page_size = 4096u;

    GuestPageOverlay(BlockSource &blocks, LentPages &lent, GuestRamLending &owner);
    ~GuestPageOverlay();
    GuestPageOverlay(const GuestPageOverlay &) = delete;
    GuestPageOverlay &operator=(const GuestPageOverlay &) = delete;

    [[nodiscard]] Status add(Host &host, ControlRequest &request, size_t offset, uint32_t entries, int fd,
                             uint64_t size);
    void                 restore(Host &host);
    uint64_t             covered() const;
    uint32_t             runs() const;
    uint32_t             extents() const;
    uint64_t             extent_address(uint32_t index) const;
    uint64_t             extent_length(uint32_t index) const;

    bool invariants_hold() const;

private:
    struct Run {
        void    *pointer_;
        uint64_t length_;
        uint64_t address_;
    };

    struct Extent {
        uint64_t address_;
        uint64_t length_;
    };

    [[nodiscard]] Status lay_entry(Host &host, uint64_t address, uint64_t length, int fd, uint64_t size);
    [[nodiscard]] Status lay_run(Host &host, void *pointer, uint64_t length, uint64_t address, int fd);

    LentPages           &lent_;
    GuestRamLending     &owner_;
    GrowableArray<Run>    runs_;
    GrowableArray<Extent> extents_;
    uint64_t              covered_;

    friend struct GuestPageOverlayProbe;
};

}

#endif

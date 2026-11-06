#ifndef AV_GUEST_RAM_LENDING_HPP
#define AV_GUEST_RAM_LENDING_HPP

#include "core/platform/block_source.hpp"
#include "core/support/growable_array.hpp"
#include "core/support/status.hpp"
#include "device/memory/guest_page_overlay.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ContextPlane;
class ControlRequest;
class Host;
class LentPages;

class GuestRamLending {
public:
    GuestRamLending(BlockSource &blocks, LentPages &pages, uint64_t size, int fd, ContextPlane &plane, uint64_t key);
    ~GuestRamLending();
    GuestRamLending(const GuestRamLending &) = delete;
    GuestRamLending &operator=(const GuestRamLending &) = delete;

    uint64_t size() const;
    uint64_t start() const;
    bool     complete() const;

    [[nodiscard]] Status lay(Host &host, ControlRequest &request, size_t offset, uint32_t entries);
    [[nodiscard]] Status verify(ControlRequest &request, size_t offset, uint32_t entries, uint32_t &next) const;

    [[nodiscard]] Status join(ContextPlane &plane, uint64_t key);
    uint32_t             leave(const ContextPlane &plane, uint64_t key);
    bool                 holder_at(uint32_t index, ContextPlane *&plane, uint64_t &key) const;
    uint32_t             holders() const;
    bool                 held_beyond(const ContextPlane &plane) const;
    void                 give_back(Host &host);

    bool invariants_hold() const;

private:
    struct Holder {
        ContextPlane *plane_;
        uint64_t      key_;
    };

    uint64_t              size_;
    int                   fd_;
    GuestPageOverlay      overlay_;
    GrowableArray<Holder> holders_;

    friend struct GuestRamLendingProbe;
};

}

#endif

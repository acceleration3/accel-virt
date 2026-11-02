/* A process's guest RAM pages laid under one host GPU memory, and the blobs
 * that hold that (docs/host-memory.md, "Application memory"). One lending is
 * one memory, however many devices share it: two adapters' devices of one QEMU
 * process lend the very pages when a Direct3D resource is opened by both - the
 * device that lends them first makes the memory and lays it under the pages
 * (the lender), and one that lends the same pages after is joined to it, its
 * blob naming the lender's memory in the executor instead of a copy
 * (ContextPlane::alias_memory). The pages are given their RAM back once the
 * last holder leaves; the lender may leave first.
 *
 * Every holder has its own guest page lock: the guest kernel locks a process's
 * pages once for each adapter that lends them, and unlocks them when that
 * adapter's blob is released, so the pages are pinned for as long as any holder
 * lives. Main loop. */
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
class LendingLife;
class LentPages;
class RunSwapGate;

class GuestRamLending {
public:
    /* A lending of `size` bytes of the dma-buf `fd`, which it owns from now
     * on, made by blob `key` of `plane`. */
    GuestRamLending(BlockSource &blocks, LentPages &pages, RunSwapGate &gate, LendingLife &life, uint64_t id,
                    uint64_t size, int fd, ContextPlane &plane, uint64_t key);
    ~GuestRamLending();
    GuestRamLending(const GuestRamLending &) = delete;
    GuestRamLending &operator=(const GuestRamLending &) = delete;

    /* What names this memory for as long as it lives and never again after:
     * a holder of something imported from it forgets it by this. */
    uint64_t id() const;
    /* The dma-buf, which the lending owns: valid until give_back, so a user
     * on another thread duplicates it on the main loop. */
    int      descriptor() const;
    /* Whether it still exists, which a present on another thread may ask after
     * the lending is gone (GuestRamLendings::watch). */
    LendingLife &life() const;
    uint64_t size() const;
    /* The guest address the first page lent starts at; 0 before any is laid. */
    uint64_t start() const;
    /* Whether every byte of it is laid over pages. */
    bool     complete() const;

    /* The lender's pieces: the next `entries` runs named at `offset` of
     * `request` laid over the memory (GuestPageOverlay::add). */
    [[nodiscard]] Status lay(Host &host, ControlRequest &request, size_t offset, uint32_t entries);
    /* Where in the memory the guest bytes [address, address + length) start,
     * if they lie in one piece of it (GuestPageOverlay::locate). */
    [[nodiscard]] bool   locate(uint64_t address, uint64_t length, uint32_t &hint, uint64_t &offset) const;
    /* A joiner's pieces: the next `entries` named at `offset` of `request`
     * must be the entries the lender laid, from entry `next` on, which it
     * advances. invalid_parameter for any difference. */
    [[nodiscard]] Status verify(ControlRequest &request, size_t offset, uint32_t entries, uint32_t &next) const;

    /* Blob `key` of `plane` holds this lending too; no_memory without room. */
    [[nodiscard]] Status join(ContextPlane &plane, uint64_t key);
    /* The blob no longer holds it; how many holders remain. */
    uint32_t             leave(const ContextPlane &plane, uint64_t key);
    /* The holder at `index` of holders(), whose memory another plane's blob
     * may be aliased to: false past the last. */
    bool                 holder_at(uint32_t index, ContextPlane *&plane, uint64_t &key) const;
    uint32_t             holders() const;
    /* Whether a plane other than `plane` holds it. */
    bool                 held_beyond(const ContextPlane &plane) const;
    /* Every run its RAM again, and the dma-buf closed: the last holder left. */
    void                 give_back(Host &host);

    bool invariants_hold() const;

private:
    struct Holder {
        ContextPlane *plane_;
        uint64_t      key_;
    };

    LendingLife          &life_;
    uint64_t              id_;
    uint64_t              size_;
    int                   fd_;
    GuestPageOverlay      overlay_;
    GrowableArray<Holder> holders_;
};

}  // namespace av

#endif  // AV_GUEST_RAM_LENDING_HPP

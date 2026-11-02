/* What is held still while runs of guest RAM are swapped for a lending's
 * dma-buf, or back (GuestPageOverlay): a run is copied and then remapped, so
 * a write to it between the two is lost, and the writers are the guest's
 * processors (Host::pause_guest) and the present engines' threads, which move
 * a frame into the same pages. Every engine that may move into a lent surface
 * registers here, and begin() holds each out - waiting for the present it is
 * moving, or waiting on the GPU to copy, to finish - before the guest is held.
 * Main loop. */
#ifndef AV_RUN_SWAP_GATE_HPP
#define AV_RUN_SWAP_GATE_HPP

#include "core/platform/block_source.hpp"
#include "core/support/growable_array.hpp"
#include "core/support/status.hpp"

namespace av {

class Host;
class PresentEngine;

class RunSwapGate {
public:
    explicit RunSwapGate(BlockSource &blocks);
    RunSwapGate(const RunSwapGate &) = delete;
    RunSwapGate &operator=(const RunSwapGate &) = delete;

    /* `engine` is held out by every swap from now until removed; no_memory when
     * the list cannot grow. */
    [[nodiscard]] Status add(PresentEngine &engine);
    void                 remove(PresentEngine &engine);
    /* Every engine held out of moving, then the guest held out of its memory:
     * whether the guest was held, which end() must be told. */
    [[nodiscard]] bool   begin(Host &host);
    void                 end(Host &host, bool guest_held);
    uint32_t             engines() const;

private:
    using Engines = GrowableArray<PresentEngine *>;

    Engines engines_;
};

}  // namespace av

#endif  // AV_RUN_SWAP_GATE_HPP

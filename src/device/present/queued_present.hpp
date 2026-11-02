/* One present the engine took (QueuedPresentRequest): its surfaces' pages
 * mapped and its regions read out of the request on the main loop, where the
 * request's buffers are, then its pixels moved on the engine's thread through
 * the mappings alone - or, from an image source, out of the present snapshot
 * its key names - and the mappings let go on the main loop again. A snapshot
 * whose destination pages are one piece of a lending (GuestRamLendings) is
 * copied by the executor's GPU into the lending's memory instead, with no
 * mapping read or written; the descriptor it is copied to is a duplicate
 * made on the main loop, since the lending may be given back while the
 * engine's thread still waits for the GPU. The
 * tickets refused after it and before the next one taken complete with it, so
 * a slot counts them. A slot of the engine's, used once per ticket. */
#ifndef AV_QUEUED_PRESENT_HPP
#define AV_QUEUED_PRESENT_HPP

#include "core/support/status.hpp"
#include "device/memory/guest_page_mapping.hpp"
#include "device/abi/av_executor.h"
#include "proto/dma/present_region.hpp"
#include "proto/virtio_gpu/queued_present_request.hpp"

#include <stdint.h>

namespace av {

class ControlRequest;
class GuestRamLendings;
class Host;
class LendingLife;
class PresentFrames;
class RegionMover;

class QueuedPresent {
public:
    /* Why a present was moved through the CPU when the GPU could have been
     * asked: what the engine says once in the log. */
    enum class Fallback : uint8_t {
        none,
        swapped_colours,
        not_lent,
        scattered,
        no_descriptor,
        lending_gone,
        unplaceable,
        executor_declined,
        count
    };

    explicit QueuedPresent(Host &host);
    ~QueuedPresent();
    QueuedPresent(const QueuedPresent &) = delete;
    QueuedPresent &operator=(const QueuedPresent &) = delete;

    /* Main loop, on a released slot. `present`'s surfaces mapped and its
     * regions read from `request`: invalid_parameter for a list or a region
     * the request does not hold, not_supported for an image source where
     * `snapshots` cannot be read; the mapping's refusal otherwise. Holds
     * nothing on failure. An image source whose destination `lendings` knows
     * is lent is copied by the GPU. */
    [[nodiscard]] Status accept(ControlRequest &request, const QueuedPresentRequest &present,
                                const PresentFrames &snapshots, GuestRamLendings &lendings);
    /* The engine's thread, once accepted: every region moved, in order, from
     * the pages or from the snapshot in `snapshots`, which is let go after.
     * invalid_parameter at the first region outside either surface, the
     * regions before it moved; not_found for a snapshot that never came or a
     * GPU that never finished its copy. */
    [[nodiscard]] Status move(const PresentFrames &snapshots);
    /* The engine's thread, after move: why the CPU moved it, none when the
     * GPU did or the CPU was the only way. */
    Fallback             fallback() const;
    /* Main loop. The mappings let go and the refusals forgotten; the slot
     * takes another present. */
    void                 release();
    uint64_t             ticket() const;
    /* The ticket after the last counted is refused, and completes with this. */
    void                 follow_with_refusal();
    uint64_t             refusals_after() const;

    bool invariants_hold() const;

private:
    [[nodiscard]] Status map(ControlRequest &request);
    [[nodiscard]] Status read_regions(ControlRequest &request);
    /* The regions through `mover`, in order. */
    [[nodiscard]] Status move_regions(RegionMover &mover);
    [[nodiscard]] Status move_snapshot(const PresentFrames &snapshots);
    /* Main loop: the destination's lending found and its descriptor
     * duplicated, or the reason it will not be copied by the GPU noted. */
    void                 lend(ControlRequest &request, GuestRamLendings &lendings);
    /* The engine's thread: the snapshot copied by the GPU, and let go; ok
     * done, not_found when refused, not_supported (the snapshot still kept) to
     * move on the CPU. */
    [[nodiscard]] Status copy_snapshot(const PresentFrames &snapshots);
    /* The regions as the executor takes them, in `out`: false for one that
     * does not lie in the destination or whose rows the buffer layout cannot
     * place. */
    [[nodiscard]] bool   place_regions(AvFrameRegion *out) const;
    void                 forget_lending();

    Host                &host_;
    GuestPageMapping     source_;
    GuestPageMapping     destination_;
    QueuedPresentRequest present_;
    PresentRegion        regions_[QueuedPresentRequest::max_regions];
    uint64_t             refusals_after_;
    /* The destination as a lending's memory: a duplicate of its dma-buf, -1
     * when it is not, and the lending's id, size and the destination's offset
     * in it. */
    int                  lent_fd_;
    uint64_t             lending_;
    uint64_t             lending_size_;
    uint64_t             lent_offset_;
    /* The lendings the lending is watched through, and whether it still
     * exists - read by the engine's thread, which finds it given back when the
     * last holder left between accept and the move. */
    GuestRamLendings    *lendings_;
    LendingLife         *life_;
    uint64_t             oldest_live_;
    Fallback             fallback_;
};

}  // namespace av

#endif  // AV_QUEUED_PRESENT_HPP

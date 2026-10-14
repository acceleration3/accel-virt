/* One present the engine took (QueuedPresentRequest): its surfaces' pages
 * mapped and its regions read out of the request on the main loop, where the
 * request's buffers are, then its pixels moved on the engine's thread through
 * the mappings alone - or, from an image source, out of the present snapshot
 * its key names - and the mappings let go on the main loop again. The
 * tickets refused after it and before the next one taken complete with it, so
 * a slot counts them. A slot of the engine's, used once per ticket. */
#ifndef AV_QUEUED_PRESENT_HPP
#define AV_QUEUED_PRESENT_HPP

#include "core/support/status.hpp"
#include "device/memory/guest_page_mapping.hpp"
#include "proto/dma/present_region.hpp"
#include "proto/virtio_gpu/queued_present_request.hpp"

#include <stdint.h>

namespace av {

class ControlRequest;
class Host;
class PresentFrames;
class RegionMover;

class QueuedPresent {
public:
    explicit QueuedPresent(Host &host);
    QueuedPresent(const QueuedPresent &) = delete;
    QueuedPresent &operator=(const QueuedPresent &) = delete;

    /* Main loop, on a released slot. `present`'s surfaces mapped and its
     * regions read from `request`: invalid_parameter for a list or a region
     * the request does not hold, not_supported for an image source where
     * `snapshots` cannot be read; the mapping's refusal otherwise. Holds
     * nothing on failure. */
    [[nodiscard]] Status accept(ControlRequest &request, const QueuedPresentRequest &present,
                                const PresentFrames &snapshots);
    /* The engine's thread, once accepted: every region moved, in order, from
     * the pages or from the snapshot in `snapshots`, which is let go after.
     * invalid_parameter at the first region outside either surface, the
     * regions before it moved; not_found for a snapshot that never came. */
    [[nodiscard]] Status move(const PresentFrames &snapshots);
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

    GuestPageMapping     source_;
    GuestPageMapping     destination_;
    QueuedPresentRequest present_;
    PresentRegion        regions_[QueuedPresentRequest::max_regions];
    uint64_t             refusals_after_;
};

}  // namespace av

#endif  // AV_QUEUED_PRESENT_HPP

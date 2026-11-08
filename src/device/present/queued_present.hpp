#ifndef AV_QUEUED_PRESENT_HPP
#define AV_QUEUED_PRESENT_HPP

#include "core/support/status.hpp"
#include "device/abi/av_executor.h"
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

    [[nodiscard]] Status accept(ControlRequest &request, const QueuedPresentRequest &present,
                                const PresentFrames &snapshots);
    [[nodiscard]] Status move(const PresentFrames &snapshots);
    void                 release();
    uint64_t             ticket() const;
    void                 follow_with_refusal();
    uint64_t             refusals_after() const;

    bool invariants_hold() const;

private:
    [[nodiscard]] Status map(ControlRequest &request);
    [[nodiscard]] Status read_regions(ControlRequest &request);
    [[nodiscard]] Status move_regions(RegionMover &mover);
    [[nodiscard]] Status move_snapshot(const PresentFrames &snapshots);
    [[nodiscard]] Status move_texture(const PresentFrames &snapshots);
    [[nodiscard]] Status move_view(const AvFrameView &view);
    [[nodiscard]] Status move_surfaces(const PresentFrames &snapshots);

    GuestPageMapping     source_;
    GuestPageMapping     destination_;
    QueuedPresentRequest present_;
    PresentRegion        regions_[QueuedPresentRequest::max_regions];
    uint64_t             refusals_after_;

    friend struct QueuedPresentProbe;
};

}

#endif

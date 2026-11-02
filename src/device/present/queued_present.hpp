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

    [[nodiscard]] Status accept(ControlRequest &request, const QueuedPresentRequest &present,
                                const PresentFrames &snapshots, GuestRamLendings &lendings);
    [[nodiscard]] Status move(const PresentFrames &snapshots);
    Fallback             fallback() const;
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
    void                 lend(ControlRequest &request, GuestRamLendings &lendings);
    [[nodiscard]] Status copy_snapshot(const PresentFrames &snapshots);
    [[nodiscard]] bool   place_regions(AvFrameRegion *out) const;
    void                 forget_lending();

    Host                &host_;
    GuestPageMapping     source_;
    GuestPageMapping     destination_;
    QueuedPresentRequest present_;
    PresentRegion        regions_[QueuedPresentRequest::max_regions];
    uint64_t             refusals_after_;
    int                  lent_fd_;
    uint64_t             lending_;
    uint64_t             lending_size_;
    uint64_t             lent_offset_;
    GuestRamLendings    *lendings_;
    LendingLife         *life_;
    uint64_t             oldest_live_;
    Fallback             fallback_;
};

}

#endif

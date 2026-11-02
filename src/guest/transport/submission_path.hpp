#ifndef AV_SUBMISSION_PATH_HPP
#define AV_SUBMISSION_PATH_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class GuestDoorbell;
class Kmt;
class ReleaseRoute;
class TransportRing;
struct SubmissionPathProbe;

class SubmissionPath {
public:
    SubmissionPath(Kmt &kmt, GuestDoorbell &doorbell, TransportRing &ring);
    SubmissionPath(const SubmissionPath &) = delete;
    SubmissionPath &operator=(const SubmissionPath &) = delete;

    [[nodiscard]] Status submit(uint32_t context, uint64_t dma_address, const uint8_t *descriptor,
                                uint32_t size, uint64_t fence);

    uint64_t through_doorbell() const;
    void     publish_before_kernel();
    uint64_t through_kernel() const;
    void     release_through(ReleaseRoute *route, uint32_t ring);
    bool     releases() const;
    uint64_t through_release() const;
    uint64_t submitted() const;

    bool invariants_hold() const;

private:
    Kmt           &kmt_;
    GuestDoorbell &doorbell_;
    TransportRing &ring_;
    uint64_t       doorbell_submissions_;
    uint64_t       kernel_submissions_;
    bool           publishes_tail_;
    uint64_t       user_submissions_;
    ReleaseRoute  *route_;
    uint32_t       route_ring_;
    uint64_t       release_submissions_;

    friend struct SubmissionPathProbe;
};

}

#endif

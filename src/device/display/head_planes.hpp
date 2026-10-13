/* Each display head's overlay planes as the guest's SCANOUT_PLANE requests
 * left them (docs/protocol.md, "Scanout planes"), and the head's size they were
 * given at: what a head shows when the device composes its frame rather than
 * showing one image. A head needs composing only when it shows more than its
 * base plane filling it - otherwise its one image is shown as it was before
 * planes. Written on the main loop and read from export connections' threads
 * too, so under a lock of its own. */
#ifndef AV_HEAD_PLANES_HPP
#define AV_HEAD_PLANES_HPP

#include "core/support/status.hpp"
#include "device/abi/av_executor.h"
#include "proto/virtio_gpu/scanout_plane_request.hpp"

#include <stdint.h>

namespace av {

class Host;

class HeadPlanes {
public:
    /* DisplayClock::heads. */
    static constexpr uint32_t heads = 16u;
    static constexpr uint32_t layers = ScanoutPlaneRequest::layers;

    explicit HeadPlanes(Host &host);
    ~HeadPlanes();
    HeadPlanes(const HeadPlanes &) = delete;
    HeadPlanes &operator=(const HeadPlanes &) = delete;

    /* At realize: the lock. Until then nothing is kept and nothing is shown. */
    [[nodiscard]] Status initialize();

    /* invalid_parameter for a head past the last. */
    [[nodiscard]] Status set(const ScanoutPlaneRequest &plane);
    /* Whether `head` shows anything but its base plane covering it whole. */
    bool                 composed(uint32_t head) const;
    /* The planes `head` shows, bottom first, into `planes` (`layers` of
     * them), and the head's size; how many planes. */
    uint32_t             planes(uint32_t head, AvScanoutPlane *planes, uint32_t &width, uint32_t &height) const;
    /* A virtio reset: no head shows a plane. */
    void                 clear();

    bool invariants_hold() const;

private:
    bool composed_locked(uint32_t head) const;

    Host          &host_;
    void          *lock_;
    AvScanoutPlane planes_[heads][layers];
    bool           shown_[heads][layers];
    uint32_t       widths_[heads];
    uint32_t       heights_[heads];
};

}  // namespace av

#endif  // AV_HEAD_PLANES_HPP

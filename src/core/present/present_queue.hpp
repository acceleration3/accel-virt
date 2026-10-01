/* The presents this miniport hands the device's present engine
 * (docs/protocol.md, "Present engine"): each is queued under the next ticket,
 * its surfaces' pages and its regions written into a payload of its own that
 * the device reads when it takes the request, and sent without waiting. The
 * device moves them in order off its main loop and says how far it got in the
 * page, and a packet that carries a present's ticket completes only then. A
 * payload is written again once the ticket it last carried has completed, so
 * at most `slots` presents are outstanding; a present that finds its payload
 * still outstanding waits for it. One per powered device. */
#ifndef AV_PRESENT_QUEUE_HPP
#define AV_PRESENT_QUEUE_HPP

#include "core/platform/wait_mutex.hpp"
#include "core/present/present_page_view.hpp"
#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class Platform;
class PresentCopyRequest;
class PresentRegion;
class PresentSurface;
class PresentTicketRecord;
class QueuedPresentRequest;
class SurfacePoint;
class VirtioGpuDevice;

class PresentQueue {
public:
    static constexpr uint32_t slots = 4u;
    /* Room for 16384 memory entries: two 3840x2160 surfaces even when every
     * page of both is a run of its own. */
    static constexpr size_t   payload_bytes = 0x40000u;
    /* How long a present waits for its payload, or for a control slot, before
     * the device is taken as gone. */
    static constexpr uint32_t wait_microseconds = 200000u;

    /* `epoch` tells this queue's tickets from those of the queue before it. */
    PresentQueue(Platform &platform, VirtioGpuDevice &device, uint32_t epoch);
    ~PresentQueue();
    PresentQueue(const PresentQueue &) = delete;
    PresentQueue &operator=(const PresentQueue &) = delete;

    /* PASSIVE: the page and the payloads, and the page registered with the
     * device, which counts from ticket 1. no_memory without contiguous memory;
     * not_supported when the device refuses the page; invalid_state once
     * started; the exchange's failure otherwise. */
    [[nodiscard]] Status start();
    /* PASSIVE. The pixels of `request` - `source`'s (null for a fill) into
     * `destination`'s regions - queued under the next ticket, answered in
     * `ticket`: invalid_state before start; too_large when a payload cannot
     * hold the page lists and the regions; not_found when the device never
     * finished what the payload last carried or never took a control slot
     * back; the send's refusal otherwise. Nothing is queued unless ok. */
    [[nodiscard]] Status queue(const PresentCopyRequest &request, const PresentSurface *source,
                               const PresentSurface &destination, const PresentRegion *regions, uint64_t &ticket);
    /* PASSIVE. The blt `request` states, out of the host surface as the
     * present that published `image` left it, rather than its source's pages:
     * as queue, which names no source pages. */
    [[nodiscard]] Status queue_image(const SurfacePoint &image, const PresentCopyRequest &request,
                                     const PresentSurface &destination, const PresentRegion *regions,
                                     uint64_t &ticket);
    /* Dispatch at most: the last ticket the device finished with; 0 before the
     * first and while the page says nothing this driver accepts. */
    uint64_t             completed() const;
    /* Dispatch at most: whether `record` names a present this queue queued,
     * and so one a packet must wait for. */
    bool                 holds(const PresentTicketRecord &record);
    uint32_t             epoch() const;

    bool invariants_hold() const;

private:
    /* The present under the next ticket: from `source`'s pages, or for an
     * `image` that names a frame from that frame, with no source in `request`. */
    [[nodiscard]] Status enqueue(const SurfacePoint &image, const PresentCopyRequest &request, const PresentSurface *source,
                                 const PresentSurface &destination, const PresentRegion *regions, uint64_t &ticket);
    /* Waits for the payload `ticket` is written into to be done with. */
    [[nodiscard]] Status wait_for_payload(uint64_t ticket);
    /* The payload `ticket` is written into. */
    uint32_t             slot_of(uint64_t ticket) const;
    [[nodiscard]] Status write_payload(uint32_t slot, const PresentSurface *source, const PresentSurface &destination,
                                       const PresentRegion *regions, uint32_t count, size_t &length);
    /* `present`'s fixed part, continued by `length` bytes of payload `slot`:
     * sent, harvesting and waiting while every control slot is busy. */
    [[nodiscard]] Status send(const QueuedPresentRequest &present, uint32_t slot, size_t length);

    Platform        &platform_;
    VirtioGpuDevice &device_;
    WaitMutex        sending_;
    PresentPageView  page_;
    uint8_t         *payloads_[slots];
    uint32_t         epoch_;
    bool             started_;
    /* The last ticket sent: written under `sending_`, read at DISPATCH. */
    uint64_t         queued_;
};

}  // namespace av

#endif  // AV_PRESENT_QUEUE_HPP

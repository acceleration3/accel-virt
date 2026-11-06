#ifndef AV_PRESENT_QUEUE_HPP
#define AV_PRESENT_QUEUE_HPP

#include "core/platform/wait_mutex.hpp"
#include "miniport/present/present_page_view.hpp"
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
struct PresentQueueProbe;

class PresentQueue {
public:
    static constexpr uint32_t slots = 4u;
    static constexpr size_t   payload_bytes = 0x40000u;
    static constexpr uint32_t wait_microseconds = 200000u;

    PresentQueue(Platform &platform, VirtioGpuDevice &device, uint32_t epoch);
    ~PresentQueue();
    PresentQueue(const PresentQueue &) = delete;
    PresentQueue &operator=(const PresentQueue &) = delete;

    [[nodiscard]] Status start();
    [[nodiscard]] Status queue(const PresentCopyRequest &request, const PresentSurface *source,
                               const PresentSurface &destination, const PresentRegion *regions, uint64_t &ticket);
    [[nodiscard]] Status queue_image(const SurfacePoint &image, const PresentCopyRequest &request,
                                     const PresentSurface &destination, const PresentRegion *regions,
                                     uint64_t &ticket);
    uint64_t             completed() const;
    bool                 holds(const PresentTicketRecord &record);
    const PresentPageView &progress() const;
    uint32_t             epoch() const;

    bool invariants_hold() const;

private:
    friend struct PresentQueueProbe;

    [[nodiscard]] Status enqueue(const SurfacePoint &image, const PresentCopyRequest &request, const PresentSurface *source,
                                 const PresentSurface &destination, const PresentRegion *regions, uint64_t &ticket);
    [[nodiscard]] Status wait_for_payload(uint64_t ticket);
    uint32_t             slot_of(uint64_t ticket) const;
    [[nodiscard]] Status write_payload(uint32_t slot, const PresentSurface *source, const PresentSurface &destination,
                                       const PresentRegion *regions, uint32_t count, size_t &length);
    [[nodiscard]] Status send(const QueuedPresentRequest &present, uint32_t slot, size_t length);

    Platform        &platform_;
    VirtioGpuDevice &device_;
    WaitMutex        sending_;
    PresentPageView  page_;
    uint8_t         *payloads_[slots];
    uint32_t         epoch_;
    bool             started_;
    uint64_t         queued_;
};

}

#endif

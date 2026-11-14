#ifndef AV_PRESENT_TICKET_HPP
#define AV_PRESENT_TICKET_HPP

#include "miniport/present/pixel_mover.hpp"
#include "core/support/status.hpp"
#include "proto/resources/surface_pair.hpp"
#include "proto/resources/surface_point.hpp"

#include <stdint.h>

namespace av {

class LogBudget;
class Platform;
class PresentCopyRequest;
class PresentQueue;
class PresentRegion;
class PresentSurface;

class PresentTicket final : public PixelMover {
public:
    PresentTicket(Platform &platform, LogBudget &budget, PresentQueue *queue, PixelMover &fallback,
                  const SurfacePoint &image, const SurfacePair &surfaces);
    PresentTicket(const PresentTicket &) = delete;
    PresentTicket &operator=(const PresentTicket &) = delete;

    [[nodiscard]] Status copy(const PresentSurface &source, const PresentSurface &destination,
                              const PresentRegion *regions, uint32_t count) override;
    [[nodiscard]] Status fill(const PresentSurface &destination, uint32_t colour, const PresentRegion *regions,
                              uint32_t count) override;
    uint64_t             ticket() const;

private:
    bool queued(const PresentCopyRequest &request, const PresentSurface *source, const PresentSurface &destination,
                const PresentRegion *regions);
    void log_refusal(Status status);

    Platform     &platform_;
    LogBudget    &budget_;
    PresentQueue *queue_;
    PixelMover   &fallback_;
    SurfacePoint  image_;
    SurfacePair   surfaces_;
    uint64_t      ticket_;
};

}

#endif

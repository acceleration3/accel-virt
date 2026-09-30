/* One present's pixels handed to the device's present engine (PresentQueue)
 * rather than moved before DdiPresent returns: the ticket the last of them
 * went under is what the present's packet waits for. A blt whose user-mode
 * driver sent a present snapshot instead of reading its buffer back goes from
 * the snapshot. Without a queue, or for a present the queue cannot take, they
 * move through `fallback` at once and the packet waits for nothing - a
 * snapshot's pixels then never arrive, and the present shows what the buffer's
 * pages last held. */
#ifndef AV_PRESENT_TICKET_HPP
#define AV_PRESENT_TICKET_HPP

#include "core/present/pixel_mover.hpp"
#include "core/support/status.hpp"

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
    /* `queue` null: the device has no present engine. `snapshot` the key of
     * the present snapshot a blt comes from; 0 for its source's pages. */
    PresentTicket(Platform &platform, LogBudget &budget, PresentQueue *queue, PixelMover &fallback,
                  uint64_t snapshot);
    PresentTicket(const PresentTicket &) = delete;
    PresentTicket &operator=(const PresentTicket &) = delete;

    [[nodiscard]] Status copy(const PresentSurface &source, const PresentSurface &destination,
                              const PresentRegion *regions, uint32_t count) override;
    [[nodiscard]] Status fill(const PresentSurface &destination, uint32_t colour, const PresentRegion *regions,
                              uint32_t count) override;
    /* The ticket to wait for; 0 when nothing was queued. */
    uint64_t             ticket() const;

private:
    /* Whether the queue took `request`; a refusal is logged while the budget
     * allows, since a present moved at once is one the application waited for. */
    bool queued(const PresentCopyRequest &request, const PresentSurface *source, const PresentSurface &destination,
                const PresentRegion *regions);
    void log_refusal(Status status);

    Platform     &platform_;
    LogBudget    &budget_;
    PresentQueue *queue_;
    PixelMover   &fallback_;
    uint64_t      snapshot_;
    uint64_t      ticket_;
};

}  // namespace av

#endif  // AV_PRESENT_TICKET_HPP

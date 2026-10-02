/* One doorbell a process asked for on a hardware queue (feature 049, US1).
 *
 * Named for the queue because the device side has a Doorbell of its own
 * (src/device/threading/doorbell.hpp): that one is the host's end - an
 * eventfd a store signals - and this one is the guest's, a page of the BAR a
 * process may write.
 *
 * Every method is APC rather than PASSIVE: a doorbell is only ever reached
 * under DoorbellTable's mutex, and holding a fast mutex is APC.
 *
 * Its address is the handle the driver answers and dxgkrnl passes back, as a
 * hardware queue's is. What it holds is the queue it belongs to, the device
 * page that queue's context was bound to, and whether it is connected - which
 * is the whole of what a process needs to know before it stores. */
#ifndef AV_QUEUE_DOORBELL_HPP
#define AV_QUEUE_DOORBELL_HPP

#include "miniport/ddi/dxgkarg_doorbell_records.hpp"
#include "core/platform/platform.hpp"

#include <stdint.h>

namespace av {

class HwQueue;

class QueueDoorbell {
public:
    QueueDoorbell(Platform &platform, HwQueue &queue, uint32_t page);
    QueueDoorbell(const QueueDoorbell &) = delete;
    QueueDoorbell &operator=(const QueueDoorbell &) = delete;

    HwQueue &queue() const;
    uint32_t page() const;
    /* Connected: the process may store, and the driver answers the page's
     * address. A connect on a connected doorbell is the same connect - dxgkrnl
     * reconnects after a suspend without destroying anything. */
    void     connect();
    /* Disconnected: the status a process reads says to submit through
     * D3DKMTSubmitCommand instead. A store it makes anyway wakes the worker,
     * which reads a ring tail that did not move (research.md §10). */
    void     disconnect();
    bool     connected() const;
    /* What a process reads out of its status page. */
    DoorbellStatus status() const;

private:
    Platform &platform_;
    HwQueue  *queue_;
    uint32_t  page_;
    bool      connected_;
};

}  // namespace av

#endif  // AV_QUEUE_DOORBELL_HPP

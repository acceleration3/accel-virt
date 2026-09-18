/* The doorbell one context submits through (feature 049, US1).
 *
 * Three mappings the kernel hands out once: the page a four-byte store lands
 * in, the status a process reads before every store, and where it writes the
 * fence value it has queued. While the status says connected, a submission is
 * those two writes and nothing else - no lock, no allocation, no kernel call
 * (FR-004).
 *
 * The status is read every time, not once: dxgkrnl disconnects a doorbell for
 * suspend, reset, migration and TDR without telling the process, and the page
 * is how it finds out. A value that is no status at all is not a connection -
 * the page is written by something outside this process (constitution V).
 */
#ifndef AV_GUEST_DOORBELL_HPP
#define AV_GUEST_DOORBELL_HPP

#include "core/ddi/dxgkarg_doorbell_records.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class Kmt;
struct GuestDoorbellProbe;

class GuestDoorbell {
public:
    explicit GuestDoorbell(Kmt &kmt);
    ~GuestDoorbell();
    GuestDoorbell(const GuestDoorbell &) = delete;
    GuestDoorbell &operator=(const GuestDoorbell &) = delete;

    /* A hardware queue on `context` and a doorbell on it over the process's
     * `ring` allocation. not_supported when the OS gives this adapter no
     * hardware queue at all, which is an answer and not a failure: the
     * transport keeps submitting through the kernel (research.md §11).
     * invalid_state once set up; nothing is left made on any refusal. */
    [[nodiscard]] Status initialize(uint32_t device, uint32_t context, uint32_t ring);
    /* dxgkrnl connects the doorbell to its page; until then a process reads
     * disconnected and submits the other way. invalid_state without one. */
    [[nodiscard]] Status connect();
    void                 teardown();
    bool                 established() const;

    /* What the status page says now. A doorbell that was never made, or whose
     * page holds a value no status names, is disconnected for good. */
    DoorbellStatus status() const;
    /* Publishes `fence` as the value this process has queued and stores to the
     * doorbell; false when the status says not to, and then nothing was
     * written at all. */
    [[nodiscard]] bool ring(uint64_t fence);

    /* What the status page asked for, once a store was refused: a retry
     * connects the doorbell again, an abort makes a new one (research.md §6).
     * Answers whether it is usable now - a caller that just fell back does not
     * store again for this submission either way. */
    bool               recover();

    uint32_t handle() const;

    /* Either everything is mapped or nothing is, and a doorbell exists only on
     * a queue. */
    bool invariants_hold() const;

private:
    Kmt            *kmt_;
    /* What a doorbell of this context is made from, kept so an abort can make
     * another without the transport being told. */
    uint32_t        device_;
    uint32_t        context_;
    uint32_t        ring_;
    uint32_t        queue_;
    uint32_t        doorbell_;
    /* The queue's progress, as the driver writes it; kept because the kernel
     * hands it out with the queue and a wait will read it (feature 049's
     * native fences are another feature's - research.md §9). */
    void           *progress_;
    volatile uint32_t *page_;
    const volatile uint32_t *status_;
    volatile uint64_t *queued_;

    friend struct GuestDoorbellProbe;
};

}  // namespace av

#endif  // AV_GUEST_DOORBELL_HPP

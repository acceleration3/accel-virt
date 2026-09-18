/* One rvk context as the driver sees it (docs/protocol.md), from setup to
 * teardown: the pages it runs on, the requests that set it up and tear it down,
 * the submissions it publishes and the fence page it reads. The pages are
 * handed to it - a process's allocations in a guest - and stay its caller's. A
 * submission, an answer and the DPC's serve can meet, so each holds the
 * context's spin lock. A device that lies fails the context; a teardown still
 * releases what the device holds, and the pages are only let go once the device
 * let go of them. */
#ifndef AV_GUEST_CONTEXT_HPP
#define AV_GUEST_CONTEXT_HPP

#include "core/platform/platform.hpp"
#include "core/platform/spin_lock.hpp"
#include "core/rvk/context_pages.hpp"
#include "core/rvk/context_request_sequence.hpp"
#include "core/rvk/fence_observer.hpp"
#include "core/rvk/ring_submitter.hpp"
#include "core/support/status.hpp"
#include "proto/submission/fence_progress.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class CapsetInfo;
class ContextSetup;
class RegisterWindow;
class SubmissionDescriptor;
struct GuestContextProbe;

class GuestContext {
public:
    enum class State : uint8_t { idle, setting_up, established, failed, tearing_down, torn_down };

    GuestContext(Platform &platform, RegisterWindow &doorbells);
    GuestContext(const GuestContext &) = delete;
    GuestContext &operator=(const GuestContext &) = delete;

    /* PASSIVE. Begins the setup on `pages`, which must outlive the context's
     * use of them. invalid_state unless idle; invalid_parameter for a setup the
     * capset does not allow or pages not complete. */
    [[nodiscard]] Status set_up(const CapsetInfo &capset, uint32_t context_id, const ContextSetup &setup,
                                const ContextPages &pages);
    /* DISPATCH. The sequence's next request and its answer; the last answer
     * of a setup establishes or fails the context, of a teardown tears it down. */
    [[nodiscard]] Status next_request(uint8_t *bytes, size_t capacity, size_t &length);
    [[nodiscard]] Status accept(const uint8_t *bytes, size_t length);
    /* DISPATCH. The submitter's answer; invalid_state unless established. */
    [[nodiscard]] Status submit(const SubmissionDescriptor &submission);
    /* DISPATCH. A doorbell was connected on this context (feature 049): the
     * process publishes the ring's tail and rings the doorbell itself, so
     * neither the tail nor what has been submitted is the driver's to know
     * until the doorbell goes away again. */
    void                 user_mode_submits(uint64_t *progress);
    void                 driver_submits();
    /* DISPATCH, from the DPC. The observer's answer; invalid_state unless established. */
    [[nodiscard]] Status serve();
    /* DISPATCH. Begins releasing what the device holds, from an established or
     * failed context; invalid_state while a sequence runs. */
    [[nodiscard]] Status tear_down();
    /* DISPATCH. The device reset: it holds nothing of this context any more. */
    void                 abandon();
    /* PASSIVE. Forgets the pages of a torn-down context, returning it to idle;
     * invalid_state while the device may still hold them. */
    [[nodiscard]] Status release();

    State          state() const;
    /* The kernel's views of the pages it was set up on; null when idle. */
    uint8_t       *ring() const;
    const uint8_t *fence_page() const;
    uint64_t       tail() const;
    uint64_t       completed() const;
    bool           faulted() const;
    uint32_t       fault_code() const;

    bool invariants_hold() const;

private:
    void finish_sequence();

    Platform              &platform_;
    SpinLock               lock_;
    ContextPages           pages_;
    ContextRequestSequence sequence_;
    FenceProgress          fences_;
    RingSubmitter          submitter_;
    FenceObserver          observer_;
    /* The hardware queue's progress, which the driver writes what the device
     * has finished into while the process submits for itself; null otherwise. */
    uint64_t              *progress_;
    uint8_t                order_;
    uint32_t               doorbell_;
    State                  state_;

    friend struct GuestContextProbe;
};

}  // namespace av

#endif  // AV_GUEST_CONTEXT_HPP

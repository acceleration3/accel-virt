/* One rvk context as the driver sees it (docs/protocol.md), from setup to
 * teardown: its memory, the requests that set it up and tear it down, the
 * submissions it publishes and the fence page it reads. A submission, an
 * answer and the DPC's serve can meet, so each holds the context's spin lock.
 * A device that lies fails the context; a teardown still releases what the
 * device holds, and the memory is only released once the device let go of it. */
#ifndef AV_GUEST_CONTEXT_HPP
#define AV_GUEST_CONTEXT_HPP

#include "core/platform/platform.hpp"
#include "core/platform/spin_lock.hpp"
#include "core/rvk/context_memory.hpp"
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

    /* PASSIVE. Allocates the context's memory and begins its setup.
     * invalid_state unless idle; invalid_parameter for a setup the capset
     * does not allow; no_memory when the platform has none. */
    [[nodiscard]] Status set_up(const CapsetInfo &capset, uint32_t context_id, const ContextSetup &setup);
    /* DISPATCH. The sequence's next request and its answer; the last answer
     * of a setup establishes or fails the context, of a teardown tears it down. */
    [[nodiscard]] Status next_request(uint8_t *bytes, size_t capacity, size_t &length);
    [[nodiscard]] Status accept(const uint8_t *bytes, size_t length);
    /* DISPATCH. The submitter's answer; invalid_state unless established. */
    [[nodiscard]] Status submit(const SubmissionDescriptor &submission);
    /* DISPATCH, from the DPC. The observer's answer; invalid_state unless established. */
    [[nodiscard]] Status serve();
    /* DISPATCH. Begins releasing what the device holds, from an established or
     * failed context; invalid_state while a sequence runs. */
    [[nodiscard]] Status tear_down();
    /* DISPATCH. The device reset: it holds nothing of this context any more. */
    void                 abandon();
    /* PASSIVE. Frees the memory of a torn-down context; invalid_state while
     * the device may still hold it. */
    [[nodiscard]] Status release();

    State          state() const;
    /* The user-mode side's view: where it writes batches and reads replies. */
    uint8_t       *ring() const;
    uint8_t       *reply() const;
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
    ContextMemory          memory_;
    ContextRequestSequence sequence_;
    FenceProgress          fences_;
    RingSubmitter          submitter_;
    FenceObserver          observer_;
    uint8_t                order_;
    uint32_t               doorbell_;
    State                  state_;

    friend struct GuestContextProbe;
};

}  // namespace av

#endif  // AV_GUEST_CONTEXT_HPP

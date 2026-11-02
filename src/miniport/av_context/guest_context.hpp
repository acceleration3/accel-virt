#ifndef AV_GUEST_CONTEXT_HPP
#define AV_GUEST_CONTEXT_HPP

#include "core/platform/platform.hpp"
#include "core/platform/spin_lock.hpp"
#include "miniport/av_context/context_pages.hpp"
#include "miniport/av_context/context_request_sequence.hpp"
#include "miniport/av_context/fence_observer.hpp"
#include "miniport/av_context/ring_submitter.hpp"
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

    [[nodiscard]] Status set_up(const CapsetInfo &capset, uint32_t context_id, const ContextSetup &setup,
                                const ContextPages &pages);
    [[nodiscard]] Status next_request(uint8_t *bytes, size_t capacity, size_t &length);
    [[nodiscard]] Status accept(const uint8_t *bytes, size_t length);
    [[nodiscard]] Status submit(const SubmissionDescriptor &submission);
    void                 kick();
    void                 user_mode_submits(uint64_t *progress);
    void                 driver_submits();
    bool                 forget_progress(const uint64_t *progress);
    [[nodiscard]] Status serve();
    [[nodiscard]] Status tear_down();
    void                 fail();
    uint64_t             next_begin() const;
    void                 abandon();
    [[nodiscard]] Status release();

    State          state() const;
    uint8_t       *ring() const;
    const uint8_t *fence_page() const;
    uint64_t       tail() const;
    uint64_t       completed() const;
    uint64_t       raised() const;
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
    uint64_t              *progress_;
    uint8_t                order_;
    uint32_t               doorbell_;
    State                  state_;

    friend struct GuestContextProbe;
};

}

#endif

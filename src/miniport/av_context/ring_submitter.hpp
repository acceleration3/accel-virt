#ifndef AV_RING_SUBMITTER_HPP
#define AV_RING_SUBMITTER_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"
#include "proto/context/context_capacity.hpp"
#include "proto/submission/ring_progress.hpp"

#include <stdint.h>

namespace av {

class FenceProgress;
class RegisterWindow;
class SubmissionDescriptor;
struct RingSubmitterProbe;

class RingSubmitter {
public:
    static constexpr uint32_t doorbell_pages = ContextCapacity::contexts;
    static constexpr uint64_t doorbell_page_bytes = ContextCapacity::doorbell_page_bytes;

    RingSubmitter(Platform &platform, RegisterWindow &doorbells, FenceProgress &fences);

    [[nodiscard]] Status attach(uint8_t *ring, uint8_t order, uint32_t doorbell);
    void                 detach();
    [[nodiscard]] Status submit(const SubmissionDescriptor &submission);
    void                 kick();
    void                 user_mode_publishes();
    bool                 attached() const;
    uint64_t             tail() const;
    uint64_t             head() const;
    bool                 failed() const;

    bool invariants_hold() const;

private:
    static constexpr uint64_t tail_offset = 8u;
    static constexpr uint64_t head_offset = 16u;

    [[nodiscard]] Status judge_head(uint64_t tail);
    [[nodiscard]] Status adopt_published_tail(const SubmissionDescriptor &submission, uint64_t &published);

    Platform       &platform_;
    RegisterWindow &doorbells_;
    FenceProgress  &fences_;
    uint8_t        *ring_;
    uint8_t         order_;
    uint32_t        doorbell_;
    uint64_t        tail_;
    bool            shared_tail_;
    uint64_t        published_;
    RingProgress    device_;
    bool            failed_;

    friend struct RingSubmitterProbe;
};

}

#endif

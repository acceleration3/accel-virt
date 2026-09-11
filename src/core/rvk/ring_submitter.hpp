/* Publishes one context's submissions into its ring (docs/protocol.md): each
 * submission's bytes follow the last one's, the bytes the device has not yet
 * consumed stay within the ring, its fence is greater than the last, and then
 * the tail is published and the context's doorbell rung. The device's head is
 * read from the ring and judged against the driver's own history; a head the
 * device could not have written fails the ring for good. Nothing here blocks
 * or allocates: a submission is at most DISPATCH. */
#ifndef AV_RING_SUBMITTER_HPP
#define AV_RING_SUBMITTER_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"
#include "proto/submission/ring_progress.hpp"

#include <stdint.h>

namespace av {

class FenceProgress;
class RegisterWindow;
class SubmissionDescriptor;
struct RingSubmitterProbe;

class RingSubmitter {
public:
    static constexpr uint32_t doorbell_pages = 64u;
    static constexpr uint64_t doorbell_page_bytes = 4096u;

    RingSubmitter(Platform &platform, RegisterWindow &doorbells, FenceProgress &fences);

    /* DISPATCH, when setup completes: the ring's memory, its order, and the context's
     * doorbell page. invalid_parameter for no ring, an order the protocol does
     * not allow or a page past the doorbells; invalid_state once attached. */
    [[nodiscard]] Status attach(uint8_t *ring, uint8_t order, uint32_t doorbell);
    /* DISPATCH. */
    void                 detach();
    /* DISPATCH. ok: published and rung. invalid_parameter for a submission
     * that does not start at the tail, is longer than the ring, or does not
     * follow the last fence; no_space until the device consumes enough;
     * invalid_state when not attached, or for a head the device could not have
     * written, which fails the ring. Refused submissions touch nothing. */
    [[nodiscard]] Status submit(const SubmissionDescriptor &submission);
    bool                 attached() const;
    uint64_t             tail() const;
    uint64_t             head() const;
    bool                 failed() const;

    bool invariants_hold() const;

private:
    static constexpr uint64_t tail_offset = 8u;
    static constexpr uint64_t head_offset = 16u;

    [[nodiscard]] Status judge_head();

    Platform       &platform_;
    RegisterWindow &doorbells_;
    FenceProgress  &fences_;
    uint8_t        *ring_;
    uint8_t         order_;
    uint32_t        doorbell_;
    uint64_t        tail_;
    uint64_t        published_;
    RingProgress    device_;
    bool            failed_;

    friend struct RingSubmitterProbe;
};

}  // namespace av

#endif  // AV_RING_SUBMITTER_HPP

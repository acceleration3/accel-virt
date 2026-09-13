/* The contexts nobody can finish, and whether their owners have been told.
 *
 * A stalled or faulted context is lost alone: dxgkrnl resets the engine it was
 * running on, the other process's context keeps completing, and what this keeps
 * is who still has to be told. A loss is recorded once however many times it is
 * reported to this, and logged once with the value that caused it - a context
 * that fails a hundred times is one line, not a hundred (FR-007). */
#ifndef AV_CONTEXT_LOSS_HPP
#define AV_CONTEXT_LOSS_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class InterruptNotification;

class ContextLoss {
public:
    /* One loss per device is the worst a single transition or reset can
     * produce, and no more devices than the table holds can exist. */
    static constexpr uint32_t max_lost = 16u;

    explicit ContextLoss(Platform &platform);
    ContextLoss(const ContextLoss &) = delete;
    ContextLoss &operator=(const ContextLoss &) = delete;

    /* DISPATCH: the TDR path runs in a DPC. `fence` is the submission the
     * context died on, `why` what killed it. */
    void                 lose(uint32_t context, uint32_t fence, Status why);
    /* Losses whose owner has not been told. */
    uint32_t             pending() const;
    /* DISPATCH: tells dxgkrnl about every pending loss, stopping at the first
     * report it refuses - what is not told stays pending. */
    [[nodiscard]] Status report(InterruptNotification &notification);

private:
    /* Parallel arrays rather than a nested record: one loss is four values, and
     * a struct inside a class is a type nothing else can name. */
    void log(uint32_t index) const;

    Platform &platform_;
    uint32_t  contexts_[max_lost];
    uint32_t  fences_[max_lost];
    Status    whys_[max_lost];
    bool      told_[max_lost];
    uint32_t  count_;
};

}  // namespace av

#endif  // AV_CONTEXT_LOSS_HPP

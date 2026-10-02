/* The batches a ring has run whose GPU work has not finished: each fence with
 * the mark its batch left (AvExecutorOps::gpu_progress), completed in the
 * order the batches ran once the GPU reaches the mark. A fence behind one
 * still held is held too, whatever its own mark, because the guest reads a
 * context's completions as one rising value. */
#ifndef AV_HELD_COMPLETIONS_HPP
#define AV_HELD_COMPLETIONS_HPP

#include <stdint.h>

namespace av {

struct HeldCompletionsProbe;

class HeldCompletions {
public:
    /* More than the guest keeps outstanding (TransportFence::max_outstanding). */
    static constexpr uint32_t capacity = 128u;

    HeldCompletions();
    HeldCompletions(const HeldCompletions &) = delete;
    HeldCompletions &operator=(const HeldCompletions &) = delete;

    /* `fence` completes once the GPU reaches `mark`. False when full. */
    [[nodiscard]] bool hold(uint64_t fence, uint64_t mark);
    /* Lets go of every fence, oldest first, whose mark `reached` passes, and
     * answers the newest of them; 0 when none was. */
    uint64_t           release(uint64_t reached);
    /* Every fence goes, answered as the newest: a faulted context completes
     * nothing more, and a detached one has no GPU work left to wait for. */
    uint64_t           release_all();
    bool               empty() const;

    bool invariants_hold() const;

private:
    uint64_t fences_[capacity];
    uint64_t marks_[capacity];
    uint32_t head_;
    uint32_t count_;

    friend struct HeldCompletionsProbe;
};

}  // namespace av

#endif  // AV_HELD_COMPLETIONS_HPP

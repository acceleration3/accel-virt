/* Where a completion lands when it arrives on a thread the device never gave
 * the host. The host's OpenCL calls back on its own driver thread, which is
 * neither this context's worker nor QEMU's main loop, and everything that
 * thread could usefully touch - the fence page, the handle registry, the decode
 * arena - is the worker's alone. So the foreign thread does two things and
 * nothing else: it sets a flag, and it wakes the worker, the way the doorbell's
 * ioeventfd already wakes it from outside. The worker clears the flag on its
 * own thread and nudges the fence reporter, which is what reaches the guest.
 *
 * The flag is one word, changed with the compiler's atomics, so the foreign
 * thread never blocks and never waits on a lock the worker holds. */
#ifndef AV_CONTEXT_SIGNAL_HPP
#define AV_CONTEXT_SIGNAL_HPP

#include <stdint.h>

namespace av {

class FenceReporter;
class Host;
class Worker;
struct ContextSignalProbe;

class ContextSignal {
public:
    ContextSignal(Host &host, Worker &worker, FenceReporter &fences);
    ContextSignal(const ContextSignal &) = delete;
    ContextSignal &operator=(const ContextSignal &) = delete;

    /* Any thread: something the guest is waiting for has finished. Signals
     * coalesce - the guest is told to ask what completed, never what did. */
    void raise();
    /* Worker: one nudge for everything raised since the last drain, and
     * nothing at all when nothing was raised. */
    void drain();

    bool     pending() const;
    uint64_t raised() const;
    uint64_t delivered() const;

    bool invariants_hold() const;

private:
    Host          &host_;
    Worker        &worker_;
    FenceReporter &fences_;
    /* Written by any thread, cleared by the worker. */
    bool           pending_;
    uint64_t       raised_;
    /* The worker's own, so a plain word. */
    uint64_t       delivered_;

    friend struct ContextSignalProbe;
};

}  // namespace av

#endif  // AV_CONTEXT_SIGNAL_HPP

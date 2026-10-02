/* The signal that wakes a context's worker, raised from a host thread: an
 * OpenCL event's callback on the host driver's thread, a Direct3D 12 fence's
 * eventfd on its own (ClEventSignals, D3d12FenceWake). Bound by the worker
 * when its session begins, cleared when it ends, so a host thread still
 * running for an old session raises nothing rather than another context's
 * pad - the session after it in the same slot binds its own. Read and written
 * atomically: the raising thread is not the worker. */
#ifndef AV_WAKE_SIGNAL_HPP
#define AV_WAKE_SIGNAL_HPP

namespace av {

class WakeSignal {
public:
    WakeSignal();
    WakeSignal(const WakeSignal &) = delete;
    WakeSignal &operator=(const WakeSignal &) = delete;

    /* Worker: what a raise calls, and with what. */
    void bind(void (*signal)(void *context), void *context);
    /* Worker: nothing is raised from now on. */
    void clear();
    bool bound() const;
    /* Any thread: the signal raised. False, and nothing raised, while
     * unbound. */
    bool raise() const;

private:
    void (*signal_)(void *context);
    void *context_;
};

}  // namespace av

#endif  // AV_WAKE_SIGNAL_HPP

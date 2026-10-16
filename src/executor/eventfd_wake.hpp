/* Wakes a session's worker each time an eventfd the host GPU's runtime holds
 * is written: when a fence it was handed reaches a mark.
 *
 * The worker holds a batch's ring fence until the GPU has run what the batch
 * recorded, and a guest waiting on that fence sends nothing that would wake a
 * sleeping worker. Rather than spinning until the GPU catches up, each mark
 * asks the runtime to write one eventfd once its fence reaches it - native
 * DXVK and vkd3d-proton take an eventfd where Windows takes an event HANDLE
 * (tools/dxvk-patches/0009 for DXVK) - and a thread that sleeps in poll() on
 * it raises the session's signal. That thread touches nothing but the eventfd
 * and the signal: the fences and the held batches stay the worker's. One per
 * session, for its Direct3D 11 batches' fence and its Direct3D 12 queues'
 * alike. */
#ifndef AV_EVENTFD_WAKE_HPP
#define AV_EVENTFD_WAKE_HPP

#include "core/trace/function_scope.hpp"
#include "executor/wake_signal.hpp"

#include <pthread.h>
#include <stdint.h>

namespace av {

class EventfdWake {
public:
    EventfdWake();
    ~EventfdWake();
    EventfdWake(const EventfdWake &) = delete;
    EventfdWake &operator=(const EventfdWake &) = delete;

    /* Before the first watch: what a reached mark raises. */
    void bind(void (*signal)(void *context), void *context);
    /* Worker: raise the signal once `fence` - an ID3D11Fence or an
     * ID3D12Fence - reaches `value`. The thread starts with the first; false
     * when it cannot, or the runtime refuses. Unbound, there is no worker to
     * wake and nothing is watched. */
    template <class Fence>
    [[nodiscard]] bool watch(Fence &fence, uint64_t value);
    /* Worker: the thread goes; the next watch starts another on the same
     * eventfd. The eventfd itself stays as long as this does: a fence let go
     * may outlive its session inside the runtime and still write it, and a
     * descriptor closed under it could by then be someone else's. */
    void               stop();
    /* Worker, as its session ends: the thread goes and so does the binding,
     * so nothing watched for this session wakes the next one in its slot,
     * which binds its own. */
    void               unbind();
    /* The thread's own: sleeps until a mark is reached or stop() is asked. */
    void               run();

private:
    /* The descriptor a watched fence writes, the thread started for it:
     * -1 when it cannot start. */
    int  descriptor();
    bool open();
    bool start();

    WakeSignal signal_;
    int        reached_fd_;
    int        stop_fd_;
    pthread_t  thread_;
    bool       running_;
};

/* The fence takes the eventfd as its event HANDLE, a pointer; its HRESULT
 * is a success when it is not negative. */
template <class Fence>
bool EventfdWake::watch(Fence &fence, uint64_t value)
{
    AV_SCOPE("EventfdWake::watch");
    if (!signal_.bound()) return true;
    const int event = descriptor();
    return event > 0 && fence.SetEventOnCompletion(value, reinterpret_cast<void *>(static_cast<intptr_t>(event))) >= 0;
}

}  // namespace av

#endif  // AV_EVENTFD_WAKE_HPP

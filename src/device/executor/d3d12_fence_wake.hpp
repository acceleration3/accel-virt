/* Wakes a context's worker when the host GPU reaches a mark a Direct3D 12
 * queue signalled.
 *
 * The worker holds a batch's ring fence until the GPU has run what the batch
 * executed, and a guest waiting on that fence sends nothing that would wake a
 * sleeping worker. Rather than spinning until the GPU catches up, each mark
 * asks vkd3d-proton to write one eventfd once its fence reaches it - a native
 * build takes an eventfd where Windows takes an event HANDLE - and a thread
 * that sleeps in poll() on it raises the context's signal. That thread touches
 * nothing but the eventfd and the signal: the fences and the held batches stay
 * the worker's. */
#ifndef AV_D3D12_FENCE_WAKE_HPP
#define AV_D3D12_FENCE_WAKE_HPP

#include <pthread.h>
#include <stdint.h>

struct ID3D12Fence;

namespace av {

class D3d12FenceWake {
public:
    D3d12FenceWake();
    ~D3d12FenceWake();
    D3d12FenceWake(const D3d12FenceWake &) = delete;
    D3d12FenceWake &operator=(const D3d12FenceWake &) = delete;

    /* Before the first watch: what a reached mark raises. */
    void bind(void (*signal)(void *context), void *context);
    /* Worker: raise the signal once `fence` reaches `value`. The thread starts
     * with the first; false when it cannot, or vkd3d-proton refuses. Unbound,
     * there is no worker to wake and nothing is watched. */
    [[nodiscard]] bool watch(ID3D12Fence *fence, uint64_t value);
    /* Worker: the thread goes; the next watch starts another. */
    void               stop();
    /* The thread's own: sleeps until a mark is reached or stop() is asked. */
    void               run();

private:
    bool start();

    void (*signal_)(void *context);
    void     *context_;
    int       reached_fd_;
    int       stop_fd_;
    pthread_t thread_;
    bool      running_;
};

}  // namespace av

#endif  // AV_D3D12_FENCE_WAKE_HPP

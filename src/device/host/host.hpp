/* What the device core may ask of QEMU. The core is freestanding: every thread,
 * wakeup, lock and main-loop hop comes through here, so the QEMU glue can create
 * them the way QEMU requires (qemu_thread_create blocks the signals a worker must
 * not take) and the mock can check the threading contract in every test.
 *
 * The contract: virtqueue state and device lifecycle belong to the main loop,
 * under QEMU's big lock; decoding and execution belong to workers and never run
 * on the main loop. Every public method states which side it runs on. */
#ifndef AV_HOST_HPP
#define AV_HOST_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

using HostRoutine = void (*)(void *context);

class Host {
public:
    static constexpr uint32_t uuid_bytes = 16u;

    virtual void log(const char *message) = 0;
    /* A violation is logged in QEMU and recorded by the mock. */
    virtual void require_main_loop(const char *function) = 0;
    virtual void require_worker(const char *function) = 0;

    [[nodiscard]] virtual Status start_thread(HostRoutine entry, void *context, void *&thread) = 0;
    /* Waits for the thread to return from its entry. */
    virtual void join_thread(void *thread) = 0;

    /* An auto-reset event: wait returns once per signal, however many were sent. */
    [[nodiscard]] virtual Status create_event(void *&event) = 0;
    virtual void destroy_event(void *event) = 0;
    virtual void signal_event(void *event) = 0;
    virtual void wait_event(void *event) = 0;

    [[nodiscard]] virtual Status create_mutex(void *&mutex) = 0;
    virtual void destroy_mutex(void *mutex) = 0;
    virtual void lock_mutex(void *mutex) = 0;
    virtual void unlock_mutex(void *mutex) = 0;

    /* Runs `routine` later on the main loop, under the big lock, in call order. */
    virtual void run_on_main_loop(HostRoutine routine, void *context) = 0;

    /* The host's Vulkan physical devices, in enumeration order. */
    virtual uint32_t gpu_count() = 0;
    virtual void     gpu_uuid(uint32_t index, uint8_t *uuid) = 0;

protected:
    Host() = default;
    ~Host() = default;
    Host(const Host &) = delete;
    Host &operator=(const Host &) = delete;
};

}  // namespace av

#endif  // AV_HOST_HPP

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

#include <stddef.h>
#include <stdint.h>

namespace av {

class GpuIdentity;

using HostRoutine = void (*)(void *context);

class Host {
public:
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
    virtual uint32_t    gpu_count() = 0;
    virtual GpuIdentity gpu_identity(uint32_t index) = 0;

    /* Any thread: zeroed memory, or null. */
    virtual void *allocate(size_t bytes) = 0;
    virtual void  release(void *block) = 0;

    /* Main loop. Guest physical memory mapped for the device, valid until
     * unmapped, from any thread: null when nothing is there. `length` shrinks at
     * a region boundary; `direct` is false for a bounce buffer over something
     * that is not guest RAM. */
    virtual void *map_guest(uint64_t address, uint64_t &length, bool &direct) = 0;
    virtual void  unmap_guest(void *pointer, uint64_t length) = 0;

    /* Main loop. A zeroed host-visible block of `size` bytes, a page multiple:
     * its handle, or null. `pointer` is the device's own view of it. */
    virtual void *create_host_memory(uint64_t size, void *&pointer) = 0;
    virtual void  destroy_host_memory(void *memory) = 0;
    /* The size of the region host-visible blocks are placed in (the hostmem BAR). */
    virtual uint64_t hostmem_size() = 0;
    /* Main loop. The block becomes visible at `offset` of the hostmem region. */
    [[nodiscard]] virtual Status place_in_hostmem(void *memory, uint64_t offset) = 0;
    virtual void                 remove_from_hostmem(uint64_t offset, uint64_t size) = 0;

    /* Main loop. A 4-byte store at offset 0 of doorbell page `index` signals
     * `event` without the main loop (a KVM ioeventfd) until disconnected; the
     * event must outlive the connection. */
    virtual void connect_doorbell(void *event, uint32_t index) = 0;
    virtual void disconnect_doorbell(void *event, uint32_t index) = 0;

    /* Main loop. The guest notifiers of every virtqueue, the fence queues
     * included (virtio-pci's set_guest_notifiers): no_memory when the host
     * cannot connect them. */
    [[nodiscard]] virtual Status connect_fence_interrupts() = 0;
    virtual void                 disconnect_fence_interrupts() = 0;
    /* Any thread, while connected: the fence queue of doorbell `line`
     * interrupts the guest on the vector the driver gave it. */
    virtual void                 raise_fence_interrupt(uint32_t line) = 0;

    /* Main loop. A control request's bytes, up to `size` from `offset`: the
     * count copied. */
    virtual size_t read_request(void *request, size_t offset, uint8_t *buffer, size_t size) = 0;
    /* Main loop. The request's one response; the host adds its fence. */
    virtual void   respond(void *request, const uint8_t *bytes, size_t size) = 0;
    /* Main loop. Whether the base virtio-gpu device holds a 2D resource `id`. */
    virtual bool   resource_exists(uint32_t id) = 0;

protected:
    Host() = default;
    ~Host() = default;
    Host(const Host &) = delete;
    Host &operator=(const Host &) = delete;
};

}  // namespace av

#endif  // AV_HOST_HPP

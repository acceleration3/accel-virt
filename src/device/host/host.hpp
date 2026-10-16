/* What the device core may ask of QEMU. The core is freestanding: every thread,
 * wakeup, lock and main-loop hop comes through here, so the QEMU glue can create
 * them the way QEMU requires (qemu_thread_create blocks the signals a worker must
 * not take) and the mock can check the threading contract in every test.
 *
 * The contract: virtqueue state and device lifecycle belong to the main loop,
 * under QEMU's big lock; decoding and execution belong to workers and never run
 * on the main loop. Every public method states which side it runs on.
 *
 * The host is where the device core's memory comes from, as a BlockSource, so
 * the core's growable tables take it as they take a platform: any thread,
 * zeroed, and with no pools or IRQL to honour. */
#ifndef AV_HOST_HPP
#define AV_HOST_HPP

#include "core/platform/block_source.hpp"
#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class GpuIdentity;
class PciAddress;
class ScanoutFramebuffer;
}  // namespace av

struct AvExportedImage;

namespace av {

using HostRoutine = void (*)(void *context);

class Host : public BlockSource {
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
    /* Any thread. Waits for `event` until monotonic_nanoseconds reaches
     * `deadline`: true when it was signalled first, false at the deadline. A
     * deadline already past answers at once, taking a signal that is there. */
    [[nodiscard]] virtual bool wait_event_until(void *event, uint64_t deadline) = 0;

    [[nodiscard]] virtual Status create_mutex(void *&mutex) = 0;
    virtual void destroy_mutex(void *mutex) = 0;
    virtual void lock_mutex(void *mutex) = 0;
    virtual void unlock_mutex(void *mutex) = 0;
    /* Takes the mutex only if it is free; whether it did. */
    virtual bool try_lock_mutex(void *mutex) = 0;

    /* Runs `routine` later on the main loop, under the big lock, in call order. */
    virtual void run_on_main_loop(HostRoutine routine, void *context) = 0;

    /* The host's Vulkan physical devices, in enumeration order. */
    virtual uint32_t    gpu_count() = 0;
    virtual GpuIdentity gpu_identity(uint32_t index) = 0;
    /* Main loop. Where host GPU `index` sits on the host's PCI buses:
     * not_found for a GPU with no PCI function (llvmpipe) or one whose driver
     * does not say (no VK_EXT_pci_bus_info). */
    [[nodiscard]] virtual Status gpu_pci_address(uint32_t index, PciAddress &address) = 0;
    /* Main loop. Up to `capacity` bytes of the file at `path`, their count in
     * `length`: not_found when the host cannot read it. */
    [[nodiscard]] virtual Status read_file(const char *path, char *buffer, size_t capacity, size_t &length) = 0;
    /* Main loop. The names in the directory at `path`, each ended by '\n', as
     * many whole names as `capacity` holds, their bytes in `length`: not_found
     * when the host cannot list it. */
    [[nodiscard]] virtual Status list_directory(const char *path, char *buffer, size_t capacity, size_t &length) = 0;

    /* Any thread: a monotonic count of nanoseconds, for measuring what the
     * device did rather than for telling the time. */
    virtual uint64_t monotonic_nanoseconds() = 0;
    /* Any thread: every store this thread made before the call reaches memory
     * before any it makes after. x86 orders write-back stores so on its own,
     * but not write-combined ones, which wait in the core's buffers. The
     * executor stores into guest-visible memory through host drivers' own
     * mappings, and ANV maps every exported allocation write-combined on a GPU
     * sharing the CPU's last-level cache, a host-cached type included, so the
     * guest saw an answer before the bytes it answered for. */
    virtual void     fence_stores() = 0;

    /* Main loop. Guest physical memory mapped for the device, valid until
     * unmapped, from any thread: null when nothing is there. `length` shrinks at
     * a region boundary; `direct` is false for a bounce buffer over something
     * that is not guest RAM. */
    virtual void *map_guest(uint64_t address, uint64_t &length, bool &direct) = 0;
    virtual void  unmap_guest(void *pointer, uint64_t length) = 0;

    /* Any thread. `bytes` of address space nothing else may be mapped into and
     * nothing may touch until aliased: its `base`, or no_memory. */
    [[nodiscard]] virtual Status reserve_range(uint64_t bytes, void *&base) = 0;
    /* Any thread. The `bytes` of guest RAM at `run` - a direct mapping
     * map_guest answered, whole pages, still mapped - mapped a second time at
     * `at` inside a reserved range, so both addresses are the same pages:
     * not_supported for RAM the host cannot map twice (no file behind it). */
    [[nodiscard]] virtual Status alias_guest_run(void *run, uint64_t bytes, void *at) = 0;
    /* Any thread. The reserved range and every alias inside it. */
    virtual void                 release_range(void *base, uint64_t bytes) = 0;

    /* The size of the region host blobs are placed in (the hostmem BAR). */
    virtual uint64_t hostmem_size() = 0;
    /* Main loop. The `size` bytes of the dma-buf `fd` - host GPU memory the
     * executor allocated - visible at `offset` of the hostmem region. The
     * descriptor stays the caller's. */
    [[nodiscard]] virtual Status place_in_hostmem(int fd, uint64_t size, uint64_t offset) = 0;
    virtual void                 remove_from_hostmem(uint64_t offset, uint64_t size) = 0;

    /* Main loop, once, when the instance's host GPU is resolved: the glue may
     * now create what it needs on that physical device. not_supported when the
     * host cannot take it, which leaves the device unbound. */
    [[nodiscard]] virtual Status bind_gpu(uint32_t index) = 0;

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

    /* Main loop. The wire recording at `path`, emptied if it was there:
     * `file`, or no_memory with the reason logged (feature 065). */
    [[nodiscard]] virtual Status create_recording(const char *path, void *&file) = 0;
    /* Any thread. `size` bytes written to the end of the recording and flushed,
     * so a process that dies next leaves them behind: no_space when the host
     * could not write them all. */
    [[nodiscard]] virtual Status append_recording(void *file, const uint8_t *bytes, size_t size) = 0;
    /* Any thread. The recording back to empty, for a recording that wraps. */
    [[nodiscard]] virtual Status truncate_recording(void *file) = 0;
    virtual void                 close_recording(void *file) = 0;
    /* Main loop. Whether display head `head` exists and its console can show a
     * dma-buf. Asked once per device, when the guest first reads the capset,
     * never at realize: QEMU registers its display backends after every device
     * is realized, and it is the backend that gives a console its GL context,
     * so a realize-time answer is no on every host. */
    virtual bool   display_takes_dmabuf(uint32_t head) = 0;
    /* Main loop. Head `head` shows `frame` out of `blob`, the device's
     * mapping of a pinned guest blob. not_supported when the host would not
     * take it, having changed nothing. */
    [[nodiscard]] virtual Status show_scanout(uint32_t head, const uint8_t *blob, const ScanoutFramebuffer &frame) = 0;
    /* Main loop. The rectangle of head `head` that changed. */
    virtual void   update_scanout(uint32_t head, uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;
    /* Main loop. Head `head` shows nothing. Safe on a head showing nothing. */
    virtual void   hide_scanout(uint32_t head) = 0;
    /* Main loop. Head `head` shows `image`, as export_image gave it:
     * not_supported where the head's console cannot show one, having changed
     * nothing. */
    [[nodiscard]] virtual Status show_image(uint32_t head, const AvExportedImage &image) = 0;

    /* The renderer process (docs/renderer.md). Main loop, all of them. A
     * descriptor handed out is a duplicate, the caller's to close; one handed
     * in stays the caller's. */
    /* The eventfd behind an event create_event made, not duplicated: -1 for a
     * host whose events are not eventfds. */
    virtual int                  event_descriptor(void *event) = 0;
    /* Fence line `line`'s guest notifier while interrupts are connected; -1
     * for a line with none. */
    virtual int                  fence_interrupt_descriptor(uint32_t line) = 0;
    /* Guest RAM as the memfd-backed regions QEMU maps it from: the count, and
     * region `index`'s number, descriptor and size. not_supported for RAM that
     * is not a shared memfd, which a renderer cannot map. */
    virtual uint32_t             guest_region_count() = 0;
    [[nodiscard]] virtual Status guest_region(uint32_t index, uint32_t &region, int &descriptor, uint64_t &size) = 0;
    /* Which region holds all `length` bytes at `pointer`, and where. */
    [[nodiscard]] virtual Status locate_guest(const void *pointer, uint64_t length, uint32_t &region,
                                              uint64_t &offset) = 0;
    virtual void                 close_descriptor(int descriptor) = 0;
    /* The renderer at `path`, started with its socket as descriptor 3; the
     * device is told through av_device_renderer_lost when it dies.
     * not_supported where the host starts none. */
    [[nodiscard]] virtual Status start_renderer(const char *path) = 0;
    /* One datagram and its descriptors: not_found once the renderer is gone. */
    [[nodiscard]] virtual Status send_renderer(const uint8_t *bytes, size_t length, const int *descriptors,
                                               uint32_t count) = 0;
    /* The next datagram, waiting at most `timeout_ms`: invalid_state when none
     * came in time, not_found once the renderer is gone. The descriptors are
     * the caller's. */
    [[nodiscard]] virtual Status receive_renderer(uint8_t *buffer, size_t capacity, size_t &length, int *descriptors,
                                                  uint32_t capacity_descriptors, uint32_t &count,
                                                  uint32_t timeout_ms) = 0;
    /* Ends the renderer at once; its death is reported as any other. */
    virtual void                 kill_renderer() = 0;

    /* The export socket's connections (docs/external-export.md), each a
     * descriptor the glue accepted and the device owns. Any thread, all four. */
    /* The next datagram on `connection`, waiting for it, its length in
     * `length` - past `capacity` for one that did not fit: not_found once the
     * peer has gone or the connection was shut down. */
    [[nodiscard]] virtual Status receive_export(int connection, uint8_t *buffer, size_t capacity, size_t &length) = 0;
    /* One datagram, with `descriptor` beside it unless it is -1, never waiting
     * for room: not_found when the peer has gone or does not read. The
     * descriptor stays the caller's. */
    [[nodiscard]] virtual Status send_export(int connection, const uint8_t *bytes, size_t length, int descriptor) = 0;
    /* A receive on `connection`, now or later, answers not_found. */
    virtual void                 shutdown_export(int connection) = 0;
    /* A descriptor the export path owns closed: a connection, a dma-buf or a
     * sync_file. */
    virtual void                 close_export(int descriptor) = 0;
    /* One of the export path's own for a descriptor the executor keeps; -1
     * when there is none to make. */
    virtual int                  duplicate_export(int descriptor) = 0;

protected:
    Host() = default;
    ~Host() = default;
};

}  // namespace av

#endif  // AV_HOST_HPP

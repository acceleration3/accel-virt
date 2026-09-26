/* What the device core asks of the process it runs in, answered by the
 * renderer process (docs/renderer.md). Its control thread is the "main loop":
 * the one thread that takes QEMU's messages, and the one routines posted with
 * run_on_main_loop run on. Guest memory is what QEMU sent as regions; a
 * worker's event waits on the doorbell QEMU registered too, because the guest
 * rings a KVM ioeventfd this process never made; a fence interrupt is a write
 * to the guest notifier QEMU passed. What only QEMU has - the display, the
 * hostmem BAR, the GPU catalog and its probes, the guest's RAM blocks and the
 * renderer process itself - is not_supported here. */
#ifndef AV_POSIX_RENDERER_HOST_HPP
#define AV_POSIX_RENDERER_HOST_HPP

#include "device/host/host.hpp"
#include "device/renderer/renderer_resources.hpp"
#include "device/shim/posix_executor_module.hpp"

#include <pthread.h>
#include <stdint.h>

namespace av {

class DoorbellOffers;
class GuestMemoryTable;

class PosixRendererHost final : public Host, public RendererResources {
public:
    /* One per doorbell page. */
    static constexpr uint32_t max_interrupt_lines = 64u;

    PosixRendererHost(GuestMemoryTable &memory, DoorbellOffers &doorbells);
    ~PosixRendererHost();
    PosixRendererHost(const PosixRendererHost &) = delete;
    PosixRendererHost &operator=(const PosixRendererHost &) = delete;

    /* The calling thread is the main loop from here on. no_memory when the
     * descriptor routines are posted through cannot be made. */
    [[nodiscard]] Status become_main_loop();
    /* The descriptor that is readable while posted routines wait. */
    int                  main_loop_wakeup() const;
    /* Main loop: every routine posted so far, in order. */
    void                 run_main_loop_work();
    [[nodiscard]] Status map_region(int descriptor, uint64_t size, void *&base) override;
    void                 unmap_region(void *base, uint64_t size) override;
    void                 close_descriptor(int descriptor) override;
    /* The memfd the next create_host_memory maps; this host closes it with the
     * memory, or at the next offer if nothing took it. */
    void                 offer_host_memory(int descriptor) override;
    void                 take_interrupt_lines(const int *descriptors, uint32_t count) override;
    void                 drop_interrupt_lines() override;
    [[nodiscard]] Status load_executor(const char *path, uint32_t length, const RvkExecutorOps *&ops) override;

    void log(const char *message) override;
    void require_main_loop(const char *function) override;
    void require_worker(const char *function) override;

    [[nodiscard]] Status start_thread(HostRoutine entry, void *context, void *&thread) override;
    void                 join_thread(void *thread) override;

    [[nodiscard]] Status create_event(void *&event) override;
    void                 destroy_event(void *event) override;
    void                 signal_event(void *event) override;
    void                 wait_event(void *event) override;

    [[nodiscard]] Status create_mutex(void *&mutex) override;
    void                 destroy_mutex(void *mutex) override;
    void                 lock_mutex(void *mutex) override;
    void                 unlock_mutex(void *mutex) override;
    bool                 try_lock_mutex(void *mutex) override;

    void run_on_main_loop(HostRoutine routine, void *context) override;

    uint32_t             gpu_count() override;
    GpuIdentity          gpu_identity(uint32_t index) override;
    [[nodiscard]] Status gpu_pci_address(uint32_t index, PciAddress &address) override;
    [[nodiscard]] Status read_file(const char *path, char *buffer, size_t capacity, size_t &length) override;

    uint64_t monotonic_nanoseconds() override;

    void *allocate(size_t bytes) override;
    void  release(void *block) override;

    void                *map_guest(uint64_t address, uint64_t &length, bool &direct) override;
    void                 unmap_guest(void *pointer, uint64_t length) override;
    [[nodiscard]] Status reserve_range(uint64_t bytes, void *&base) override;
    [[nodiscard]] Status alias_guest_run(void *run, uint64_t bytes, void *at) override;
    void                 release_range(void *base, uint64_t bytes) override;
    [[nodiscard]] Status dmabuf_over_guest(void *const *runs, const uint64_t *sizes, uint32_t count,
                                           int &fd) override;
    void                 close_dmabuf(int fd) override;

    void                *create_host_memory(uint64_t size, void *&pointer) override;
    void                 destroy_host_memory(void *memory) override;
    uint64_t             hostmem_size() override;
    [[nodiscard]] Status place_in_hostmem(void *memory, uint64_t offset) override;
    void                 remove_from_hostmem(uint64_t offset, uint64_t size) override;
    [[nodiscard]] Status read_hostmem(uint64_t offset, uint8_t *buffer, uint64_t size) override;

    [[nodiscard]] Status bind_gpu(uint32_t index) override;
    [[nodiscard]] void  *export_gpu_memory(uint64_t size, void *&pointer, int32_t &host_error) override;
    void                 release_gpu_memory(void *memory) override;
    [[nodiscard]] void  *import_guest_memory(void *pointer, uint64_t size, int32_t &host_error) override;
    void                 release_guest_memory(void *memory) override;
    [[nodiscard]] Status read_imported_memory(void *memory, uint64_t offset, uint8_t *buffer,
                                              uint64_t size) override;

    void connect_doorbell(void *event, uint32_t index) override;
    void disconnect_doorbell(void *event, uint32_t index) override;

    [[nodiscard]] Status connect_fence_interrupts() override;
    void                 disconnect_fence_interrupts() override;
    void                 raise_fence_interrupt(uint32_t line) override;

    size_t read_request(void *request, size_t offset, uint8_t *buffer, size_t size) override;
    void   respond(void *request, const uint8_t *bytes, size_t size) override;
    bool   resource_exists(uint32_t id) override;

    [[nodiscard]] Status create_recording(const char *path, void *&file) override;
    [[nodiscard]] Status append_recording(void *file, const uint8_t *bytes, size_t size) override;
    [[nodiscard]] Status truncate_recording(void *file) override;
    void                 close_recording(void *file) override;

    bool                 display_takes_dmabuf(uint32_t head) override;
    [[nodiscard]] Status show_scanout(uint32_t head, int fd, const ScanoutFramebuffer &frame) override;
    void                 update_scanout(uint32_t head, uint32_t x, uint32_t y, uint32_t width,
                                        uint32_t height) override;
    void                 hide_scanout(uint32_t head) override;

    int                  event_descriptor(void *event) override;
    int                  fence_interrupt_descriptor(uint32_t line) override;
    uint32_t             guest_region_count() override;
    [[nodiscard]] Status guest_region(uint32_t index, uint32_t &region, int &descriptor, uint64_t &size) override;
    [[nodiscard]] Status locate_guest(const void *pointer, uint64_t length, uint32_t &region,
                                      uint64_t &offset) override;
    int                  host_memory_descriptor(void *memory) override;
    [[nodiscard]] Status start_renderer(const char *path) override;
    [[nodiscard]] Status send_renderer(const uint8_t *bytes, size_t length, const int *descriptors,
                                       uint32_t count) override;
    [[nodiscard]] Status receive_renderer(uint8_t *buffer, size_t capacity, size_t &length, int *descriptors,
                                          uint32_t capacity_descriptors, uint32_t &count,
                                          uint32_t timeout_ms) override;
    void                 kill_renderer() override;

private:
    struct Posted {
        HostRoutine routine_;
        void       *context_;
        Posted     *next_;
    };

    bool on_main_loop() const;
    void close_offered_memory();

    static constexpr uint32_t path_capacity = 4096u;

    GuestMemoryTable   &memory_;
    DoorbellOffers     &doorbells_;
    PosixExecutorModule executor_;
    pthread_t         main_loop_;
    bool              main_loop_known_;
    int               wakeup_;
    pthread_mutex_t   posted_lock_;
    Posted           *posted_first_;
    Posted           *posted_last_;
    int               offered_memory_;
    int               interrupt_lines_[max_interrupt_lines];
    bool              interrupts_connected_;
};

}  // namespace av

#endif  // AV_POSIX_RENDERER_HOST_HPP

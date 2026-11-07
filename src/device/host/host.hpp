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
}

struct AvExportedImage;

namespace av {

using HostRoutine = void (*)(void *context);

class Host : public BlockSource {
public:
    virtual void log(const char *message) = 0;
    virtual void require_main_loop(const char *function) = 0;
    virtual void require_worker(const char *function) = 0;

    [[nodiscard]] virtual Status start_thread(HostRoutine entry, void *context, void *&thread) = 0;
    virtual void join_thread(void *thread) = 0;

    [[nodiscard]] virtual Status create_event(void *&event) = 0;
    virtual void destroy_event(void *event) = 0;
    virtual void signal_event(void *event) = 0;
    virtual void wait_event(void *event) = 0;
    [[nodiscard]] virtual bool wait_event_until(void *event, uint64_t deadline) = 0;

    [[nodiscard]] virtual Status create_mutex(void *&mutex) = 0;
    virtual void destroy_mutex(void *mutex) = 0;
    virtual void lock_mutex(void *mutex) = 0;
    virtual void unlock_mutex(void *mutex) = 0;
    virtual bool try_lock_mutex(void *mutex) = 0;

    virtual void run_on_main_loop(HostRoutine routine, void *context) = 0;

    virtual uint32_t    gpu_count() = 0;
    virtual GpuIdentity gpu_identity(uint32_t index) = 0;
    [[nodiscard]] virtual Status gpu_pci_address(uint32_t index, PciAddress &address) = 0;
    [[nodiscard]] virtual Status read_file(const char *path, char *buffer, size_t capacity, size_t &length) = 0;
    [[nodiscard]] virtual Status list_directory(const char *path, char *buffer, size_t capacity, size_t &length) = 0;

    virtual uint64_t monotonic_nanoseconds() = 0;
    virtual void     fence_stores() = 0;

    virtual void *map_guest(uint64_t address, uint64_t &length, bool &direct) = 0;
    virtual void  unmap_guest(void *pointer, uint64_t length) = 0;

    [[nodiscard]] virtual Status reserve_range(uint64_t bytes, void *&base) = 0;
    [[nodiscard]] virtual Status alias_guest_run(void *run, uint64_t bytes, void *at) = 0;
    virtual void                 release_range(void *base, uint64_t bytes) = 0;
    [[nodiscard]] virtual Status overlay_guest_run(void *run, uint64_t bytes, int fd, uint64_t offset) = 0;
    [[nodiscard]] virtual Status restore_guest_run(void *run, uint64_t bytes) = 0;

    virtual uint64_t guest_memory_bytes() = 0;
    virtual uint64_t hostmem_size() = 0;
    [[nodiscard]] virtual Status place_in_hostmem(int fd, uint64_t size, uint64_t offset) = 0;
    virtual void                 remove_from_hostmem(uint64_t offset, uint64_t size) = 0;

    [[nodiscard]] virtual Status bind_gpu(uint32_t index) = 0;

    virtual void connect_doorbell(void *event, uint32_t index) = 0;
    virtual void disconnect_doorbell(void *event, uint32_t index) = 0;

    [[nodiscard]] virtual Status connect_fence_interrupts() = 0;
    virtual void                 disconnect_fence_interrupts() = 0;
    virtual void                 raise_fence_interrupt(uint32_t line) = 0;

    virtual size_t read_request(void *request, size_t offset, uint8_t *buffer, size_t size) = 0;
    virtual void   respond(void *request, const uint8_t *bytes, size_t size) = 0;
    virtual bool   resource_exists(uint32_t id) = 0;

    [[nodiscard]] virtual Status create_recording(const char *path, void *&file) = 0;
    [[nodiscard]] virtual Status append_recording(void *file, const uint8_t *bytes, size_t size) = 0;
    [[nodiscard]] virtual Status truncate_recording(void *file) = 0;
    virtual void                 close_recording(void *file) = 0;
    virtual bool   display_takes_dmabuf(uint32_t head) = 0;
    [[nodiscard]] virtual Status show_scanout(uint32_t head, const uint8_t *blob, const ScanoutFramebuffer &frame) = 0;
    virtual void   update_scanout(uint32_t head, uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;
    virtual void   hide_scanout(uint32_t head) = 0;
    [[nodiscard]] virtual Status show_image(uint32_t head, const AvExportedImage &image) = 0;

    virtual int                  event_descriptor(void *event) = 0;
    virtual int                  fence_interrupt_descriptor(uint32_t line) = 0;
    virtual uint32_t             guest_region_count() = 0;
    [[nodiscard]] virtual Status guest_region(uint32_t index, uint32_t &region, int &descriptor, uint64_t &size) = 0;
    [[nodiscard]] virtual Status locate_guest(const void *pointer, uint64_t length, uint32_t &region,
                                              uint64_t &offset) = 0;
    virtual void                 close_descriptor(int descriptor) = 0;
    [[nodiscard]] virtual Status start_renderer(const char *path) = 0;
    [[nodiscard]] virtual Status send_renderer(const uint8_t *bytes, size_t length, const int *descriptors,
                                               uint32_t count) = 0;
    [[nodiscard]] virtual Status receive_renderer(uint8_t *buffer, size_t capacity, size_t &length, int *descriptors,
                                                  uint32_t capacity_descriptors, uint32_t &count,
                                                  uint32_t timeout_ms) = 0;
    virtual void                 kill_renderer() = 0;

    [[nodiscard]] virtual Status receive_export(int connection, uint8_t *buffer, size_t capacity, size_t &length) = 0;
    [[nodiscard]] virtual Status send_export(int connection, const uint8_t *bytes, size_t length, int descriptor) = 0;
    virtual void                 shutdown_export(int connection) = 0;
    virtual void                 close_export(int descriptor) = 0;
    virtual int                  duplicate_export(int descriptor) = 0;

protected:
    Host() = default;
    ~Host() = default;
};

}

#endif

/* The host QEMU provides: every Host call forwarded to the glue's operations
 * table, with the threading contract checked against QEMU's big lock. */
#ifndef AV_TABLE_HOST_HPP
#define AV_TABLE_HOST_HPP

#include "device/abi/av_device.h"
#include "device/host/host.hpp"

namespace av {

class TableHost final : public Host {
public:
    explicit TableHost(const AvHostOps &ops);

    void     log(const char *message) override;
    void     require_main_loop(const char *function) override;
    void     require_worker(const char *function) override;
    Status   start_thread(HostRoutine entry, void *context, void *&thread) override;
    void     join_thread(void *thread) override;
    Status   create_event(void *&event) override;
    void     destroy_event(void *event) override;
    void     signal_event(void *event) override;
    void     wait_event(void *event) override;
    bool     wait_event_until(void *event, uint64_t deadline) override;
    Status   create_mutex(void *&mutex) override;
    void     destroy_mutex(void *mutex) override;
    void     lock_mutex(void *mutex) override;
    void     unlock_mutex(void *mutex) override;
    bool     try_lock_mutex(void *mutex) override;
    void     run_on_main_loop(HostRoutine routine, void *context) override;
    uint32_t gpu_count() override;
    GpuIdentity gpu_identity(uint32_t index) override;
    Status   gpu_pci_address(uint32_t index, PciAddress &address) override;
    Status   read_file(const char *path, char *buffer, size_t capacity, size_t &length) override;
    Status   list_directory(const char *path, char *buffer, size_t capacity, size_t &length) override;
    void    *allocate(size_t bytes) override;
    void     release(void *block) override;
    void    *map_guest(uint64_t address, uint64_t &length, bool &direct) override;
    void     unmap_guest(void *pointer, uint64_t length) override;
    Status   reserve_range(uint64_t bytes, void *&base) override;
    Status   alias_guest_run(void *run, uint64_t bytes, void *at) override;
    void     release_range(void *base, uint64_t bytes) override;
    uint64_t hostmem_size() override;
    Status   place_in_hostmem(int fd, uint64_t size, uint64_t offset) override;
    void     remove_from_hostmem(uint64_t offset, uint64_t size) override;
    uint64_t monotonic_nanoseconds() override;
    void     fence_stores() override;
    Status   bind_gpu(uint32_t index) override;
    void     connect_doorbell(void *event, uint32_t index) override;
    void     disconnect_doorbell(void *event, uint32_t index) override;
    Status   connect_fence_interrupts() override;
    void     disconnect_fence_interrupts() override;
    void     raise_fence_interrupt(uint32_t line) override;
    size_t   read_request(void *request, size_t offset, uint8_t *buffer, size_t size) override;
    void     respond(void *request, const uint8_t *bytes, size_t size) override;
    bool     resource_exists(uint32_t id) override;
    Status   create_recording(const char *path, void *&file) override;
    Status   append_recording(void *file, const uint8_t *bytes, size_t size) override;
    Status   truncate_recording(void *file) override;
    void     close_recording(void *file) override;
    bool     display_takes_dmabuf(uint32_t head) override;
    Status   show_scanout(uint32_t head, const uint8_t *blob, const ScanoutFramebuffer &frame) override;
    void     update_scanout(uint32_t head, uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
    void     hide_scanout(uint32_t head) override;
    Status   show_image(uint32_t head, const AvExportedImage &image) override;
    int      event_descriptor(void *event) override;
    int      fence_interrupt_descriptor(uint32_t line) override;
    uint32_t guest_region_count() override;
    Status   guest_region(uint32_t index, uint32_t &region, int &descriptor, uint64_t &size) override;
    Status   locate_guest(const void *pointer, uint64_t length, uint32_t &region, uint64_t &offset) override;
    void     close_descriptor(int descriptor) override;
    Status   start_renderer(const char *path) override;
    Status   send_renderer(const uint8_t *bytes, size_t length, const int *descriptors, uint32_t count) override;
    Status   receive_renderer(uint8_t *buffer, size_t capacity, size_t &length, int *descriptors,
                              uint32_t capacity_descriptors, uint32_t &count, uint32_t timeout_ms) override;
    void     kill_renderer() override;
    Status   receive_export(int connection, uint8_t *buffer, size_t capacity, size_t &length) override;
    Status   send_export(int connection, const uint8_t *bytes, size_t length, int descriptor) override;
    void     shutdown_export(int connection) override;
    void     close_export(int descriptor) override;
    int      duplicate_export(int descriptor) override;

private:
    void violated(const char *rule, const char *function);

    const AvHostOps &ops_;
};

}  // namespace av

#endif  // AV_TABLE_HOST_HPP

/* The host QEMU provides: every Host call forwarded to the glue's operations
 * table, with the threading contract checked against QEMU's big lock. */
#ifndef AV_TABLE_HOST_HPP
#define AV_TABLE_HOST_HPP

#include "device/abi/rvk_device.h"
#include "device/host/host.hpp"

namespace av {

class TableHost final : public Host {
public:
    explicit TableHost(const RvkHostOps &ops);

    void     log(const char *message) override;
    void     require_main_loop(const char *function) override;
    void     require_worker(const char *function) override;
    Status   start_thread(HostRoutine entry, void *context, void *&thread) override;
    void     join_thread(void *thread) override;
    Status   create_event(void *&event) override;
    void     destroy_event(void *event) override;
    void     signal_event(void *event) override;
    void     wait_event(void *event) override;
    Status   create_mutex(void *&mutex) override;
    void     destroy_mutex(void *mutex) override;
    void     lock_mutex(void *mutex) override;
    void     unlock_mutex(void *mutex) override;
    void     run_on_main_loop(HostRoutine routine, void *context) override;
    uint32_t gpu_count() override;
    GpuIdentity gpu_identity(uint32_t index) override;
    void    *allocate(size_t bytes) override;
    void     release(void *block) override;
    void    *map_guest(uint64_t address, uint64_t &length, bool &direct) override;
    void     unmap_guest(void *pointer, uint64_t length) override;
    Status   reserve_range(uint64_t bytes, void *&base) override;
    Status   alias_guest_run(void *run, uint64_t bytes, void *at) override;
    void     release_range(void *base, uint64_t bytes) override;
    void    *create_host_memory(uint64_t size, void *&pointer) override;
    void     destroy_host_memory(void *memory) override;
    uint64_t hostmem_size() override;
    Status   place_in_hostmem(void *memory, uint64_t offset) override;
    void     remove_from_hostmem(uint64_t offset, uint64_t size) override;
    uint64_t monotonic_nanoseconds() override;
    Status   read_hostmem(uint64_t offset, uint8_t *buffer, uint64_t size) override;
    Status   bind_gpu(uint32_t index) override;
    void    *export_gpu_memory(uint64_t size, void *&pointer, int32_t &host_error) override;
    void     release_gpu_memory(void *memory) override;
    void    *import_guest_memory(void *pointer, uint64_t size, int32_t &host_error) override;
    void     release_guest_memory(void *memory) override;
    Status   read_imported_memory(void *memory, uint64_t offset, uint8_t *buffer, uint64_t size) override;
    void     connect_doorbell(void *event, uint32_t index) override;
    void     disconnect_doorbell(void *event, uint32_t index) override;
    Status   connect_fence_interrupts() override;
    void     disconnect_fence_interrupts() override;
    void     raise_fence_interrupt(uint32_t line) override;
    size_t   read_request(void *request, size_t offset, uint8_t *buffer, size_t size) override;
    void     respond(void *request, const uint8_t *bytes, size_t size) override;
    bool     resource_exists(uint32_t id) override;

private:
    void violated(const char *rule, const char *function);

    const RvkHostOps &ops_;
};

}  // namespace av

#endif  // AV_TABLE_HOST_HPP

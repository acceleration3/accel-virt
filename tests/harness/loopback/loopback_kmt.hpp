/* A D3DKMT the in-process transport runs against the loopback device core
 * (feature 043, L4): allocations are guest memory the device reads by physical
 * address, and a virtual context is set up by publishing its ring, fence and
 * reply as guest blobs and sending CTX_CREATE and the setup over the device's
 * control channel - the miniport's job, done in a shim. A submission rings the
 * context's doorbell; a wait blocks on its fence-queue interrupt. It is the
 * ClientTransport's `Kmt` so KmtTransport, and the generated client above it,
 * reach the real device core in one process. */
#ifndef AV_LOOPBACK_KMT_HPP
#define AV_LOOPBACK_KMT_HPP

#include "guest/kmt/kmt.hpp"
#include "harness/loopback/loopback_device.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

namespace av {

class LoopbackKmt final : public Kmt {
public:
    static constexpr std::uint32_t max_allocations = 32u;
    static constexpr std::uint32_t max_contexts    = 8u;
    static constexpr std::uint32_t first_resource  = 0x00100000u;

    LoopbackKmt(MockPlatform &platform, LoopbackDevice &device);

    Status   enumerate_adapters(std::uint32_t *adapters, std::uint32_t capacity, std::uint32_t &count) override;
    Status   query_identity(std::uint32_t adapter, PciIdentity &identity) override;
    Status   query_driver_version(std::uint32_t adapter, std::uint32_t &version) override;
    Status   close_adapter(std::uint32_t adapter) override;
    Status   create_device(std::uint32_t adapter, std::uint32_t &device) override;
    Status   destroy_device(std::uint32_t device) override;
    Status   create_allocation(std::uint32_t device, const std::uint8_t *descriptor, std::uint32_t size,
                               std::uint32_t &allocation) override;
    Status   destroy_allocation(std::uint32_t device, std::uint32_t allocation) override;
    Status   lock(std::uint32_t device, std::uint32_t allocation, void *&view) override;
    Status   unlock(std::uint32_t device, std::uint32_t allocation) override;
    Status   create_paging_queue(std::uint32_t device, std::uint32_t &queue, std::uint32_t &fence_object) override;
    Status   destroy_paging_queue(std::uint32_t queue) override;
    Status   make_resident(std::uint32_t queue, std::uint32_t allocation, std::uint64_t &paging_fence) override;
    Status   map_gpu_virtual_address(std::uint32_t queue, std::uint32_t allocation, std::uint64_t pages,
                                     std::uint64_t &address, std::uint64_t &paging_fence) override;
    Status   free_gpu_virtual_address(std::uint32_t adapter, std::uint64_t address, std::uint64_t pages) override;
    Status   wait_for_fence(std::uint32_t device, std::uint32_t fence_object, std::uint64_t fence) override;
    Status   create_context(std::uint32_t device, std::uint32_t &context) override;
    Status   create_context_virtual(std::uint32_t device, const std::uint8_t *private_data, std::uint32_t size,
                                    std::uint32_t &context) override;
    Status   destroy_context(std::uint32_t context) override;
    Status   submit(std::uint32_t context, std::uint64_t commands, std::uint64_t length) override;
    Status   wait_for_context(std::uint32_t device, std::uint32_t context) override;
    Status   wait_for_vertical_blank(std::uint32_t adapter, std::uint32_t device, std::uint32_t source) override;
    std::uint64_t performance_counter(std::uint64_t &frequency) const override;
    std::uint32_t last_code() const override;

private:
    static constexpr std::uint64_t page_bytes = 4096u;

    struct Allocation {
        std::uint32_t handle;
        std::uint8_t *view;
        std::uint64_t physical;
        std::uint64_t bytes;
    };
    struct Context {
        std::uint32_t handle;
        std::uint32_t doorbell;
        std::uint32_t blobs[3];
        std::uint32_t seen;   /* fence-line raises seen before the pending submit */
    };

    Allocation *find_allocation(std::uint32_t handle);
    Context    *find_context(std::uint32_t handle);
    /* One control request through the device; true when it answered OK_NODATA. */
    bool        control(const std::uint8_t *request, std::size_t length);
    bool        publish(std::uint32_t resource_id, const Allocation &allocation);

    MockPlatform  &platform_;
    LoopbackDevice &device_;
    Allocation     allocations_[max_allocations];
    Context        contexts_[max_contexts];
    std::uint32_t  allocation_count_;
    std::uint32_t  context_count_;
    std::uint32_t  next_handle_;
    std::uint32_t  next_resource_;
    std::uint32_t  next_doorbell_;
};

}  // namespace av

#endif  // AV_LOOPBACK_KMT_HPP

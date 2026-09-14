/* Kmt over gdi32's D3DKMT functions: each call fills the kernel's structure,
 * keeps the NTSTATUS, and answers core's types. Waits go through one event
 * with a timeout, so a fence the driver never reaches fails the step instead
 * of hanging the guest. Translation only, like src/km. */
#ifndef AV_GDI_KMT_HPP
#define AV_GDI_KMT_HPP

#include "core/support/status.hpp"
#include "guest/kmt/kmt.hpp"

#include <stdint.h>

namespace av {

class GdiKmt final : public Kmt {
public:
    static constexpr uint32_t wait_milliseconds = 10000u;

    GdiKmt();
    ~GdiKmt();
    GdiKmt(const GdiKmt &) = delete;
    GdiKmt &operator=(const GdiKmt &) = delete;

    /* The event every wait uses. */
    [[nodiscard]] Status initialize();

    Status   enumerate_adapters(uint32_t *adapters, uint32_t capacity, uint32_t &count) override;
    Status   query_identity(uint32_t adapter, PciIdentity &identity) override;
    Status   query_driver_version(uint32_t adapter, uint32_t &version) override;
    Status   close_adapter(uint32_t adapter) override;
    Status   create_device(uint32_t adapter, uint32_t &device) override;
    Status   destroy_device(uint32_t device) override;
    Status   create_allocation(uint32_t device, const uint8_t *descriptor, uint32_t size,
                               uint32_t &allocation) override;
    Status   destroy_allocation(uint32_t device, uint32_t allocation) override;
    Status   lock(uint32_t device, uint32_t allocation, void *&view) override;
    Status   unlock(uint32_t device, uint32_t allocation) override;
    Status   create_paging_queue(uint32_t device, uint32_t &queue, uint32_t &fence_object) override;
    Status   destroy_paging_queue(uint32_t queue) override;
    Status   make_resident(uint32_t queue, uint32_t allocation, uint64_t &paging_fence) override;
    Status   map_gpu_virtual_address(uint32_t queue, uint32_t allocation, uint64_t pages, uint64_t &address,
                                     uint64_t &paging_fence) override;
    Status   free_gpu_virtual_address(uint32_t adapter, uint64_t address, uint64_t pages) override;
    Status   wait_for_fence(uint32_t device, uint32_t fence_object, uint64_t fence) override;
    Status   create_context(uint32_t device, uint32_t &context) override;
    Status   create_context_virtual(uint32_t device, const uint8_t *private_data, uint32_t size,
                                    uint32_t &context) override;
    Status   destroy_context(uint32_t context) override;
    Status   submit(uint32_t context, uint64_t commands, uint64_t length, const uint8_t *private_data,
                    uint32_t private_size) override;
    Status   wait_for_context(uint32_t device, uint32_t context) override;
    Status   wait_for_vertical_blank(uint32_t adapter, uint32_t device, uint32_t source) override;
    uint64_t performance_counter(uint64_t &frequency) const override;
    uint32_t last_code() const override;

private:
    Status answer(int32_t code);
    Status await_event();

    void    *event_;
    uint32_t last_code_;
};

}  // namespace av

#endif  // AV_GDI_KMT_HPP

/* One PCI display device, from DxgkDdiAddDevice to DxgkDdiRemoveDevice. */
#ifndef AV_ADAPTER_HPP
#define AV_ADAPTER_HPP

#include "platform.hpp"

#include <stdint.h>

namespace av {

class Adapter {
public:
    Adapter(Platform &platform, void *physical_device_object);
    Adapter(const Adapter &) = delete;
    Adapter &operator=(const Adapter &) = delete;

    void *physical_device_object() const;

    /* DDI answers, as NTSTATUS. Filled in by later phases of feature 006. */
    int32_t start_device(void *start_info, void *dxgkrnl_interface, uint32_t *sources,
                         uint32_t *children);
    int32_t stop_device();
    int32_t dispatch_io_request(uint32_t source, void *request);
    uint8_t interrupt_routine(uint32_t message);
    void    dpc_routine();
    int32_t query_child_relations(void *relations, uint32_t size);
    int32_t query_child_status(void *status, uint8_t non_destructive);
    int32_t query_device_descriptor(uint32_t child, void *descriptor);
    int32_t set_power_state(uint32_t device, int32_t power_state, int32_t action);
    int32_t notify_acpi_event(int32_t type, uint32_t event, void *argument, uint32_t *acpi_flags);
    void    reset_device();
    int32_t query_interface(void *query);
    int32_t link_device(void *physical_device_object, void *linked);

private:
    Platform &platform_;
    void     *physical_device_object_;
};

}  // namespace av

#endif  // AV_ADAPTER_HPP

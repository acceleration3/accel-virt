/* The DDI entry points registered in the miniport's DdiTable. Constitution III's
 * ABI exception: extern "C", one statement each, forwarding to the adapter
 * behind the context or, for the context-less DDIs, to the bound root.
 * Signatures mirror dispmprt.h at DXGKDDI_INTERFACE_VERSION_WDDM3_2: ULONG is
 * uint32_t, BOOLEAN and UCHAR uint8_t, NTSTATUS and enums int32_t. */
#ifndef AV_DDI_ENTRY_POINTS_HPP
#define AV_DDI_ENTRY_POINTS_HPP

#include "../miniport_binding.hpp"
#include "dxgk_child_descriptor_record.hpp"
#include "dxgk_child_status_record.hpp"
#include "dxgkrnl_interface_record.hpp"
#include "linked_device_record.hpp"

#include <stdint.h>

namespace av {

extern MiniportBinding miniport_binding;

}  // namespace av

extern "C" {

int32_t AvDdiAddDevice(void *physical_device_object, void **context);
int32_t AvDdiStartDevice(void *context, void *start_info, const av::DxgkrnlInterfaceRecord *dxgkrnl_interface,
                         uint32_t *sources, uint32_t *children);
int32_t AvDdiStopDevice(void *context);
int32_t AvDdiRemoveDevice(void *context);
int32_t AvDdiDispatchIoRequest(void *context, uint32_t source, void *request);
uint8_t AvDdiInterruptRoutine(void *context, uint32_t message);
void    AvDdiDpcRoutine(void *context);
int32_t AvDdiQueryChildRelations(void *context, av::DxgkChildDescriptorRecord *relations, uint32_t size);
int32_t AvDdiQueryChildStatus(void *context, av::DxgkChildStatusRecord *status, uint8_t non_destructive);
int32_t AvDdiQueryDeviceDescriptor(void *context, uint32_t child, void *descriptor);
int32_t AvDdiSetPowerState(void *context, uint32_t device, int32_t power_state, int32_t action);
int32_t AvDdiNotifyAcpiEvent(void *context, int32_t type, uint32_t event, void *argument,
                             uint32_t *acpi_flags);
void    AvDdiResetDevice(void *context);
void    AvDdiUnload(void);
int32_t AvDdiQueryInterface(void *context, void *query);
void    AvDdiControlEtwLogging(uint8_t enable, uint32_t flags, uint8_t level);
int32_t AvDdiLinkDevice(void *physical_device_object, void *context, av::LinkedDeviceRecord *linked);

}

#endif  // AV_DDI_ENTRY_POINTS_HPP

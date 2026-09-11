/* The C entry points into the device core: the RvkDevice ABI the QEMU glue
 * calls (rvk_device.h), and the routines the host calls back. Each one line,
 * forwarding to the object behind its pointer. */
#ifndef AV_DEVICE_ENTRY_POINTS_HPP
#define AV_DEVICE_ENTRY_POINTS_HPP

#include "device/abi/rvk_device.h"

namespace av {

class DeviceModule;

/* The module's devices, in static storage: see DeviceModule. */
extern constinit DeviceModule device_module;

}  // namespace av

extern "C" {

void RvkWorkerRun(void *worker);
void RvkRingAttach(void *ring);

}

#endif  // AV_DEVICE_ENTRY_POINTS_HPP

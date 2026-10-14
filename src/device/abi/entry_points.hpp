/* The C entry points into the device core: the AvDevice ABI the QEMU glue
 * calls (av_device.h), and the routines the host calls back. Each one line,
 * forwarding to the object behind its pointer. */
#ifndef AV_DEVICE_ENTRY_POINTS_HPP
#define AV_DEVICE_ENTRY_POINTS_HPP

#include "device/abi/av_device.h"

namespace av {

class DeviceModule;

/* The module's devices, in static storage: see DeviceModule. */
extern constinit DeviceModule device_module;

}  // namespace av

extern "C" {

void AvWorkerRun(void *worker);
void AvRingAttach(void *ring);
void AvDisplayClockRun(void *clock);
void AvDevicePulseRun(void *pulse);
void AvDisplayFramesShow(void *device);
void AvPresentEngineRun(void *engine);
void AvExportConnectionServe(void *connection);

}

#endif  // AV_DEVICE_ENTRY_POINTS_HPP

#ifndef AV_DEVICE_ENTRY_POINTS_HPP
#define AV_DEVICE_ENTRY_POINTS_HPP

#include "device/abi/av_device.h"

namespace av {

class DeviceModule;

extern constinit DeviceModule device_module;

}

extern "C" {

void AvWorkerRun(void *worker);
void AvRingAttach(void *ring);
void AvDisplayClockRun(void *clock);
void AvDevicePulseRun(void *pulse);
void AvDisplayShowRun(void *show);
void AvDisplayFramesExport(void *device);
void AvDisplayImagesShow(void *device);
void AvDisplayFramesShow(void *device);
void AvPresentEngineRun(void *engine);
void AvExportConnectionServe(void *connection);

}

#endif

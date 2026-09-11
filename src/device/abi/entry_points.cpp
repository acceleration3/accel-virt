#include "device/abi/entry_points.hpp"

#include "device/module/device_instance.hpp"
#include "device/module/device_module.hpp"
#include "device/threading/worker.hpp"

namespace av {

constinit DeviceModule device_module;

}  // namespace av

extern "C" {

__attribute__((visibility("default")))
RvkDevice *rvk_device_create(const RvkHostOps *ops, const char *host_gpu, int base_blobs, char *error,
                             size_t error_size)
{ return reinterpret_cast<RvkDevice *>(av::device_module.create(ops, host_gpu, base_blobs != 0, error, error_size)); }

__attribute__((visibility("default")))
int rvk_device_control(RvkDevice *device, void *request, uint32_t type)
{ return reinterpret_cast<av::DeviceInstance *>(device)->device().control(request, type) ? 1 : 0; }

__attribute__((visibility("default")))
uint32_t rvk_device_capsets(RvkDevice *device)
{ return reinterpret_cast<av::DeviceInstance *>(device)->device().capsets(); }

__attribute__((visibility("default")))
void rvk_device_reset(RvkDevice *device)
{ reinterpret_cast<av::DeviceInstance *>(device)->device().reset(); }

__attribute__((visibility("default")))
int rvk_device_destroy(RvkDevice *device)
{ return av::device_module.destroy(reinterpret_cast<av::DeviceInstance *>(device)) != av::Status::ok; }

void RvkWorkerRun(void *worker)
{ static_cast<av::Worker *>(worker)->run(); }

}

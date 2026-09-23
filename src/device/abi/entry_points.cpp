#include "device/abi/entry_points.hpp"

#include "device/module/device_instance.hpp"
#include "device/module/device_module.hpp"
#include "device/ring/ring_consumer.hpp"
#include "device/shim/device_trace.hpp"
#include "device/threading/worker.hpp"

namespace av {

constinit DeviceModule device_module;
/* The module's trace services: its table, crash file and state socket. */
constinit DeviceTraceRoot device_trace;

}  // namespace av

extern "C" {

/* The fatal signals' handler: sigaction hands it no context. */
static void rvk_device_crashed(int signal, siginfo_t *info, void *context)
{ av::device_trace.crashed(signal, info, context); }

__attribute__((visibility("default")))
void rvk_device_trace_start(RvkMainLoopScheduler schedule)
{ av::device_trace.start(av::device_module, schedule, &rvk_device_crashed); }

__attribute__((visibility("default")))
RvkDevice *rvk_device_create(const RvkHostOps *ops, const RvkExecutorOps *executor, const char *host_gpu,
                             int base_blobs, const RvkRecordingOptions *recording, char *error,
                             size_t error_size)
{ return reinterpret_cast<RvkDevice *>(av::device_module.create(ops, executor, host_gpu, base_blobs != 0, recording, error, error_size)); }

__attribute__((visibility("default")))
int rvk_device_control(RvkDevice *device, void *request, uint32_t type)
{ return reinterpret_cast<av::DeviceInstance *>(device)->device().control(request, type) ? 1 : 0; }

__attribute__((visibility("default")))
int rvk_device_export_image(RvkDevice *device, uint64_t key, RvkExportedImage *image)
{ return image != nullptr && reinterpret_cast<av::DeviceInstance *>(device)->device().export_image(key, *image) ? 1 : 0; }

int rvk_device_read_image(RvkDevice *device, uint64_t key, void *pixels, uint32_t stride, uint32_t width, uint32_t height)
{ return reinterpret_cast<av::DeviceInstance *>(device)->device().read_image(key, pixels, stride, width, height) ? 1 : 0; }

__attribute__((visibility("default")))
uint32_t rvk_device_capsets(RvkDevice *device)
{ return reinterpret_cast<av::DeviceInstance *>(device)->device().capsets(); }

__attribute__((visibility("default")))
int rvk_device_pci_identity(RvkDevice *device, RvkPciIdentity *identity)
{ return reinterpret_cast<av::DeviceInstance *>(device)->device().pci_identity(*identity) ? 1 : 0; }

__attribute__((visibility("default")))
void rvk_device_status(RvkDevice *device, uint8_t status)
{ reinterpret_cast<av::DeviceInstance *>(device)->device().status(status); }

__attribute__((visibility("default")))
void rvk_device_reset(RvkDevice *device)
{ reinterpret_cast<av::DeviceInstance *>(device)->device().reset(); }

__attribute__((visibility("default")))
int rvk_device_destroy(RvkDevice *device)
{ return av::device_module.destroy(reinterpret_cast<av::DeviceInstance *>(device)) != av::Status::ok; }

void RvkWorkerRun(void *worker)
{ static_cast<av::Worker *>(worker)->run(); }

void RvkRingAttach(void *ring)
{ static_cast<av::RingConsumer *>(ring)->attach(); }

}

#include "device/abi/entry_points.hpp"

#include "device/module/device_instance.hpp"
#include "device/display/display_clock.hpp"
#include "device/display/display_show.hpp"
#include "device/fence/device_pulse.hpp"
#include "device/export/export_connection.hpp"
#include "device/present/present_engine.hpp"
#include "device/module/device_module.hpp"
#include "device/module/virtio_gpu_av.hpp"
#include "device/context/ring_consumer.hpp"
#include "device/shim/device_trace.hpp"
#include "device/threading/worker.hpp"

namespace av {

constinit DeviceModule device_module;
constinit DeviceTraceRoot device_trace;

}

extern "C" {
static void av_device_crashed(int signal, siginfo_t *info, void *context)
{ av::device_trace.crashed(signal, info, context); }

__attribute__((visibility("default")))
void av_device_trace_start(AvMainLoopScheduler schedule)
{ av::device_trace.start(av::device_module, schedule, &av_device_crashed); }

__attribute__((visibility("default")))
AvDevice *av_device_create(const AvHostOps *ops, const AvExecutorOps *executor, const char *host_gpu,
                             int base_blobs, const AvRecordingOptions *recording, const char *renderer,
                             const char *executor_path, uint32_t vram_reserve_mib, uint32_t host_ram_reserve_mib,
                             char *error, size_t error_size)
{ return reinterpret_cast<AvDevice *>(av::device_module.create(ops, executor, host_gpu, base_blobs != 0, recording, renderer, executor_path, vram_reserve_mib, host_ram_reserve_mib, error, error_size)); }

__attribute__((visibility("default")))
int av_device_control(AvDevice *device, void *request, uint32_t type)
{ return reinterpret_cast<av::DeviceInstance *>(device)->device().control(request, type) ? 1 : 0; }

__attribute__((visibility("default")))
int av_device_export_image(AvDevice *device, uint64_t key, AvExportedImage *image)
{ return image != nullptr && reinterpret_cast<av::DeviceInstance *>(device)->device().export_image(key, *image) ? 1 : 0; }

__attribute__((visibility("default")))
int av_device_export_head(AvDevice *device, uint32_t head, uint64_t key, AvExportedImage *image)
{ return image != nullptr && reinterpret_cast<av::DeviceInstance *>(device)->device().export_head(head, key, *image) ? 1 : 0; }

int av_device_read_image(AvDevice *device, uint64_t key, void *pixels, uint32_t stride, uint32_t width, uint32_t height)
{ return reinterpret_cast<av::DeviceInstance *>(device)->device().read_image(key, pixels, stride, width, height) ? 1 : 0; }

__attribute__((visibility("default")))
uint32_t av_device_capsets(AvDevice *device)
{ return reinterpret_cast<av::DeviceInstance *>(device)->device().capsets(); }

__attribute__((visibility("default")))
int av_device_pci_identity(AvDevice *device, AvPciIdentity *identity)
{ return reinterpret_cast<av::DeviceInstance *>(device)->device().pci_identity(*identity) ? 1 : 0; }

__attribute__((visibility("default")))
int av_device_set_modes(AvDevice *device, const char *modes, uint32_t *width, uint32_t *height)
{ return reinterpret_cast<av::DeviceInstance *>(device)->device().offer_modes(modes, width, height); }

__attribute__((visibility("default")))
int av_device_set_refresh(AvDevice *device, uint32_t millihz)
{ return reinterpret_cast<av::DeviceInstance *>(device)->device().offer_refresh(millihz) ? 1 : -1; }

__attribute__((visibility("default")))
void av_device_status(AvDevice *device, uint8_t status)
{ reinterpret_cast<av::DeviceInstance *>(device)->device().status(status); }

__attribute__((visibility("default")))
int av_device_post_frame(AvDevice *device, uint32_t head, uint32_t frame, uint64_t key)
{ return reinterpret_cast<av::DeviceInstance *>(device)->device().post_frame(head, frame, key) ? 1 : 0; }

__attribute__((visibility("default")))
void av_device_reset(AvDevice *device)
{ reinterpret_cast<av::DeviceInstance *>(device)->device().reset(); }

__attribute__((visibility("default")))
void av_device_renderer_lost(AvDevice *device)
{ reinterpret_cast<av::DeviceInstance *>(device)->device().renderer_lost(); }

__attribute__((visibility("default")))
int av_device_export_accepted(AvDevice *device, int connection, int same_user)
{ return reinterpret_cast<av::DeviceInstance *>(device)->device().export_accepted(connection, same_user != 0) ? 0 : 1; }

__attribute__((visibility("default")))
int av_device_destroy(AvDevice *device)
{ return av::device_module.destroy(reinterpret_cast<av::DeviceInstance *>(device)) != av::Status::ok; }

void AvWorkerRun(void *worker)
{ static_cast<av::Worker *>(worker)->run(); }

void AvRingAttach(void *ring)
{ static_cast<av::RingConsumer *>(ring)->attach(); }

void AvDisplayClockRun(void *clock)
{ static_cast<av::DisplayClock *>(clock)->run(); }

void AvDevicePulseRun(void *pulse)
{ static_cast<av::DevicePulse *>(pulse)->run(); }

void AvDisplayShowRun(void *show)
{ static_cast<av::DisplayShow *>(show)->run(); }

void AvDisplayFramesExport(void *device)
{ static_cast<av::VirtioGpuAv *>(device)->export_latched_frames(); }

void AvDisplayImagesShow(void *device)
{ static_cast<av::VirtioGpuAv *>(device)->show_exported_images(); }

void AvDisplayFramesShow(void *device)
{ static_cast<av::VirtioGpuAv *>(device)->show_latched_frames(); }

void AvPresentEngineRun(void *engine)
{ static_cast<av::PresentEngine *>(engine)->run(); }

void AvExportConnectionServe(void *connection)
{ static_cast<av::ExportConnection *>(connection)->serve(); }

}

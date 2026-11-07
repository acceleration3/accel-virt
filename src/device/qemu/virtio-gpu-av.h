/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef HW_VIRTIO_GPU_AV_H
#define HW_VIRTIO_GPU_AV_H

#include "qemu/iov.h"
#include <gmodule.h>
#include "hw/virtio/virtio-gpu.h"
#include <av_device.h>
#include <vulkan/vulkan.h>

#define TYPE_VIRTIO_GPU_AV "virtio-gpu-av-device"
OBJECT_DECLARE_SIMPLE_TYPE(VirtIOGPUAv, VIRTIO_GPU_AV)

#define VIRTIO_GPU_AV_MAX_GPUS 16
#define VIRTIO_GPU_AV_MAX_REGIONS 64

typedef struct VirtIOGPUAvRegion {
    void *host;
    uint64_t fd_offset;
    uint64_t used;
    bool mappable;
} VirtIOGPUAvRegion;

typedef struct VirtIOGPUAvRenderer {
    pid_t pid;
    int pidfd;
    int socket;
    guint hangup;
    uint32_t region_count;
    VirtIOGPUAvRegion regions[VIRTIO_GPU_AV_MAX_REGIONS];
} VirtIOGPUAvRenderer;

struct VirtIOGPUAv {
    VirtIOGPU parent_obj;

    char *host_gpu;
    char *executor;
    GModule *executor_library;
    char *renderer;
    VirtIOGPUAvRenderer process;
    char *record;
    uint64_t record_limit;
    uint32_t vram_reserve;
    uint32_t host_ram_reserve;
    char *record_kinds;
    bool record_wrap;
    bool display;
    char *modes;
    uint32_t refresh;
    char *export_socket;
    int export_listener;
    AvHostOps host;
    uint32_t gpu_count;
    AvGpuProperties gpus[VIRTIO_GPU_AV_MAX_GPUS];
    VkInstance instance;
    VkPhysicalDevice physical[VIRTIO_GPU_AV_MAX_GPUS];
    VkPhysicalDevice bound;
    AvDevice *core;
    MemoryRegion hostmem_background;
    void *hostmem_mmap;
    MemoryRegion doorbells;
    VirtQueue *fence_queues[AV_DEVICE_INTERRUPT_LINES];
    bool scanouts[VIRTIO_GPU_MAX_SCANOUTS];
    QemuDmaBuf *exported[VIRTIO_GPU_MAX_SCANOUTS][2];
    int32_t exported_fd[VIRTIO_GPU_MAX_SCANOUTS][2];
    int32_t showing[VIRTIO_GPU_MAX_SCANOUTS];
};

void virtio_gpu_av_host_init(VirtIOGPUAv *r);
void virtio_gpu_av_host_finish(VirtIOGPUAv *r);
int virtio_gpu_av_pci_identity(VirtIOGPUAv *r, AvPciIdentity *identity);

void virtio_gpu_av_renderer_ops(AvHostOps *ops);
void virtio_gpu_av_renderer_init(VirtIOGPUAv *r);
void virtio_gpu_av_renderer_finish(VirtIOGPUAv *r);

void virtio_gpu_av_export_init(VirtIOGPUAv *r);
bool virtio_gpu_av_export_listen(VirtIOGPUAv *r, Error **errp);
void virtio_gpu_av_export_finish(VirtIOGPUAv *r);

bool virtio_gpu_av_show_image(VirtIOGPUAv *r, uint32_t head, const AvExportedImage *image);
void virtio_gpu_av_show_surface(VirtIOGPUAv *r, uint32_t head);

#endif

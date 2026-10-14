/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * virtio-gpu-av: accel-virt's virtio-gpu device. A virtio-gpu subclass whose
 * behaviour lives in the device core (libav_device.a) behind av_device.h;
 * this glue only translates between QEMU and that ABI.
 *
 * Installed by accel-virt's tools/build_qemu.sh as include/hw/virtio/virtio-gpu-av.h.
 */
#ifndef HW_VIRTIO_GPU_AV_H
#define HW_VIRTIO_GPU_AV_H

#include "qemu/iov.h"
#include <gmodule.h>
#include "hw/virtio/virtio-gpu.h"
#include <av_device.h>
#include <vulkan/vulkan.h>

#define TYPE_VIRTIO_GPU_AV "virtio-gpu-av-device"
OBJECT_DECLARE_SIMPLE_TYPE(VirtIOGPUAv, VIRTIO_GPU_AV)

/* Host GPUs remembered from the Vulkan loader at realize. */
#define VIRTIO_GPU_AV_MAX_GPUS 16
/* RAMBlocks a renderer may be handed as guest regions. */
#define VIRTIO_GPU_AV_MAX_REGIONS 64

/* A RAMBlock as guest_region_count found it: remembered by its host address,
 * not the block, which RCU may free once the list changes. */
typedef struct VirtIOGPUAvRegion {
    void *host;
    uint64_t fd_offset;
    uint64_t used;
    bool mappable;
} VirtIOGPUAvRegion;

/* The av-renderer the contexts run in (docs/renderer.md): -1 descriptors
 * while none runs. */
typedef struct VirtIOGPUAvRenderer {
    pid_t pid;
    int pidfd;
    int socket;
    /* The socket's hangup watch, 0 when none. */
    guint hangup;
    uint32_t region_count;
    VirtIOGPUAvRegion regions[VIRTIO_GPU_AV_MAX_REGIONS];
} VirtIOGPUAvRenderer;

struct VirtIOGPUAv {
    VirtIOGPU parent_obj;

    char *host_gpu;
    /* The executor library's path (av_executor.h); none: no capset. */
    char *executor;
    GModule *executor_library;
    /* av-renderer's path: set, the contexts run in that process, which loads
     * the executor library itself, and QEMU does not (docs/renderer.md). */
    char *renderer;
    VirtIOGPUAvRenderer process;
    /* The wire recording this device writes (docs/protocol.md, feature 065);
     * no path is recording off. */
    char *record;
    uint64_t record_limit;
    char *record_kinds;
    bool record_wrap;
    bool display;
    /* The modes the display offers, "WxH,WxH" with the first preferred; none
     * is the host's monitors'. Settable before realize and while the VM runs. */
    char *modes;
    /* The unix socket a process outside QEMU asks for shared surfaces on
     * (docs/external-export.md); none: no socket. The listening descriptor,
     * -1 while none listens. */
    char *export_socket;
    int export_listener;
    AvHostOps host;
    uint32_t gpu_count;
    AvGpuProperties gpus[VIRTIO_GPU_AV_MAX_GPUS];
    /* The Vulkan loader, kept from the catalog read at realize: the physical
     * devices it enumerated and the one this instance was bound to. A host
     * with no loader leaves every handle null. */
    VkInstance instance;
    VkPhysicalDevice physical[VIRTIO_GPU_AV_MAX_GPUS];
    VkPhysicalDevice bound;
    AvDevice *core;
    /* The hostmem BAR's backing: private anonymous memory that the
     * executor's dma-bufs are placed over with MAP_FIXED, as virtio-gpu-gl
     * does for virgl (docs/host-memory.md). */
    MemoryRegion hostmem_background;
    void *hostmem_mmap;
    /* One page per context; a store to a page is an ioeventfd the core
     * connects to the context's worker. The PCI wrapper maps it as BAR 0. */
    MemoryRegion doorbells;
    /* Queue FIRST + n carries doorbell n's fence interrupts on its vector,
     * and queue FIRST + DISPLAY_LINE the display clock's. */
    VirtQueue *fence_queues[AV_DEVICE_INTERRUPT_LINES];
    /* Whether each head shows a display surface over a guest blob. */
    bool scanouts[VIRTIO_GPU_MAX_SCANOUTS];
    /* A GL display's view of the executor's exported images, one per head and
     * slot, kept so the display imports each image once rather than a frame;
     * `showing` is the slot on screen, or -1 while the 2D surface is. */
    QemuDmaBuf *exported[VIRTIO_GPU_MAX_SCANOUTS][2];
    int32_t exported_fd[VIRTIO_GPU_MAX_SCANOUTS][2];
    int32_t showing[VIRTIO_GPU_MAX_SCANOUTS];
};

/* Fills r->host with QEMU's threads, events, locks, main loop, memory and
 * control queue, and the GPU catalog with the host's Vulkan physical devices. */
void virtio_gpu_av_host_init(VirtIOGPUAv *r);
/* Releases the logical device and the Vulkan instance the catalog was read
 * with; safe on a device that never realized. */
void virtio_gpu_av_host_finish(VirtIOGPUAv *r);
/* av_device_pci_identity for the PCI wrapper, which is a module of its own
 * and must not link a second copy of the core to ask it. */
int virtio_gpu_av_pci_identity(VirtIOGPUAv *r, AvPciIdentity *identity);

/* The renderer's operations in `ops`: its process, its socket and the guest
 * regions it maps (virtio-gpu-av-renderer.c). */
void virtio_gpu_av_renderer_ops(AvHostOps *ops);
/* No renderer running; before anything else touches r->process. */
void virtio_gpu_av_renderer_init(VirtIOGPUAv *r);
/* Kills and reaps a renderer still running, without telling the core, which
 * is gone by then. */
void virtio_gpu_av_renderer_finish(VirtIOGPUAv *r);

/* The export socket (docs/external-export.md): nothing listening, before
 * anything else touches it; listening at r->export_socket, accepting each
 * connection for the core, or an error; and no longer listening, the socket
 * file gone. */
void virtio_gpu_av_export_init(VirtIOGPUAv *r);
bool virtio_gpu_av_export_listen(VirtIOGPUAv *r, Error **errp);
void virtio_gpu_av_export_finish(VirtIOGPUAv *r);

/* Head `head` shows `image` straight from the executor's dma-buf; false
 * where the head's display has no GL. */
bool virtio_gpu_av_show_image(VirtIOGPUAv *r, uint32_t head, const AvExportedImage *image);
/* Head `head` goes back to its 2D surface, which new pixels were written to. */
void virtio_gpu_av_show_surface(VirtIOGPUAv *r, uint32_t head);

#endif

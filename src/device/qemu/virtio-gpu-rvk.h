/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * virtio-gpu-rvk: accel-virt's virtio-gpu device. A virtio-gpu subclass whose
 * behaviour lives in the device core (librvk_device.a) behind rvk_device.h;
 * this glue only translates between QEMU and that ABI.
 *
 * Installed by accel-virt's tools/build_qemu.sh as include/hw/virtio/virtio-gpu-rvk.h.
 */
#ifndef HW_VIRTIO_GPU_RVK_H
#define HW_VIRTIO_GPU_RVK_H

#include "qemu/iov.h"
#include <gmodule.h>
#include "hw/virtio/virtio-gpu.h"
#include <rvk_device.h>
#include <vulkan/vulkan.h>

#define TYPE_VIRTIO_GPU_RVK "virtio-gpu-rvk-device"
OBJECT_DECLARE_SIMPLE_TYPE(VirtIOGPURvk, VIRTIO_GPU_RVK)

/* Host GPUs remembered from the Vulkan loader at realize. */
#define VIRTIO_GPU_RVK_MAX_GPUS 16
/* RAMBlocks a renderer may be handed as guest regions. */
#define VIRTIO_GPU_RVK_MAX_REGIONS 64

/* A RAMBlock as guest_region_count found it: remembered by its host address,
 * not the block, which RCU may free once the list changes. */
typedef struct VirtIOGPURvkRegion {
    void *host;
    uint64_t fd_offset;
    uint64_t used;
    bool mappable;
} VirtIOGPURvkRegion;

/* The rvk-renderer the contexts run in (docs/renderer.md): -1 descriptors
 * while none runs. */
typedef struct VirtIOGPURvkRenderer {
    pid_t pid;
    int pidfd;
    int socket;
    /* The socket's hangup watch, 0 when none. */
    guint hangup;
    uint32_t region_count;
    VirtIOGPURvkRegion regions[VIRTIO_GPU_RVK_MAX_REGIONS];
} VirtIOGPURvkRenderer;

struct VirtIOGPURvk {
    VirtIOGPU parent_obj;

    char *host_gpu;
    /* The executor library's path (rvk_executor.h); none: no capset. */
    char *executor;
    GModule *executor_library;
    /* rvk-renderer's path: set, the contexts run in that process, which loads
     * the executor library itself, and QEMU does not (docs/renderer.md). */
    char *renderer;
    VirtIOGPURvkRenderer process;
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
    RvkHostOps host;
    uint32_t gpu_count;
    RvkGpuProperties gpus[VIRTIO_GPU_RVK_MAX_GPUS];
    /* The Vulkan loader, kept from the catalog read at realize: the physical
     * devices it enumerated, and the logical device on the one this instance
     * was bound to, which the feasibility probes allocate and import through
     * (feature 039). A host with no loader leaves every handle null and every
     * probe refused. */
    VkInstance instance;
    VkPhysicalDevice physical[VIRTIO_GPU_RVK_MAX_GPUS];
    VkPhysicalDevice bound;
    VkDevice gpu;
    /* VK_EXT_external_memory_host's alignment, 0 when the device lacks it. */
    uint64_t import_alignment;
    RvkDevice *core;
    /* The hostmem BAR's backing: private anonymous memory that blobs are
     * placed over with MAP_FIXED, as virtio-gpu-gl does for virgl. */
    MemoryRegion hostmem_background;
    void *hostmem_mmap;
    /* One page per context; a store to a page is an ioeventfd the core
     * connects to the context's worker. The PCI wrapper maps it as BAR 0. */
    MemoryRegion doorbells;
    /* Queue FIRST + n carries doorbell n's fence interrupts on its vector. */
    VirtQueue *fence_queues[RVK_DEVICE_DOORBELLS];
    /* The dma-buf each head is showing, which QEMU's display holds an import
     * of until it is told otherwise; NULL for a head showing nothing. */
    QemuDmaBuf *scanouts[VIRTIO_GPU_MAX_SCANOUTS];
    /* A GL display's view of the executor's exported images, one per head and
     * slot, kept so the display imports each image once rather than a frame;
     * `showing` is the slot on screen, or -1 while the 2D surface is. */
    QemuDmaBuf *exported[VIRTIO_GPU_MAX_SCANOUTS][2];
    int32_t exported_fd[VIRTIO_GPU_MAX_SCANOUTS][2];
    int32_t showing[VIRTIO_GPU_MAX_SCANOUTS];
};

/* Fills r->host with QEMU's threads, events, locks, main loop, memory and
 * control queue, and the GPU catalog with the host's Vulkan physical devices. */
void virtio_gpu_rvk_host_init(VirtIOGPURvk *r);
/* Releases the logical device and the Vulkan instance the catalog was read
 * with; safe on a device that never realized. */
void virtio_gpu_rvk_host_finish(VirtIOGPURvk *r);
/* rvk_device_pci_identity for the PCI wrapper, which is a module of its own
 * and must not link a second copy of the core to ask it. */
int virtio_gpu_rvk_pci_identity(VirtIOGPURvk *r, RvkPciIdentity *identity);

/* The renderer's operations in `ops`: its process, its socket and the guest
 * regions it maps (virtio-gpu-rvk-renderer.c). */
void virtio_gpu_rvk_renderer_ops(RvkHostOps *ops);
/* No renderer running; before anything else touches r->process. */
void virtio_gpu_rvk_renderer_init(VirtIOGPURvk *r);
/* Kills and reaps a renderer still running, without telling the core, which
 * is gone by then. */
void virtio_gpu_rvk_renderer_finish(VirtIOGPURvk *r);

/* Head `head` shows `image` straight from the executor's dma-buf; false
 * where the head's display has no GL. */
bool virtio_gpu_rvk_show_image(VirtIOGPURvk *r, uint32_t head, const RvkExportedImage *image);
/* Head `head` goes back to its 2D surface, which new pixels were written to. */
void virtio_gpu_rvk_show_surface(VirtIOGPURvk *r, uint32_t head);

#endif

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

struct VirtIOGPURvk {
    VirtIOGPU parent_obj;

    char *host_gpu;
    /* The executor library's path (rvk_executor.h); none: no capset. */
    char *executor;
    GModule *executor_library;
    /* The wire recording this device writes (docs/protocol.md, feature 065);
     * no path is recording off. */
    char *record;
    uint64_t record_limit;
    char *record_kinds;
    bool record_wrap;
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

#endif

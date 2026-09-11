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
#include "hw/virtio/virtio-gpu.h"
#include <rvk_device.h>

#define TYPE_VIRTIO_GPU_RVK "virtio-gpu-rvk-device"
OBJECT_DECLARE_SIMPLE_TYPE(VirtIOGPURvk, VIRTIO_GPU_RVK)

/* Host GPUs remembered from the Vulkan loader at realize. */
#define VIRTIO_GPU_RVK_MAX_GPUS 16

struct VirtIOGPURvk {
    VirtIOGPU parent_obj;

    char *host_gpu;
    RvkHostOps host;
    uint32_t gpu_count;
    RvkGpuProperties gpus[VIRTIO_GPU_RVK_MAX_GPUS];
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
};

/* Fills r->host with QEMU's threads, events, locks, main loop, memory and
 * control queue, and the GPU catalog with the host's Vulkan physical devices. */
void virtio_gpu_rvk_host_init(VirtIOGPURvk *r);

#endif

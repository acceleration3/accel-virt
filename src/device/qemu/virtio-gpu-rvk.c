/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * virtio-gpu-rvk-device: the base virtio-gpu device handles the 2D protocol;
 * realize, reset, finalize and every control request reach the device core
 * through rvk_device.h first. Beyond stock virtio-gpu, only what the core says
 * it implements is offered: context init, when it has a capset, the doorbell
 * pages the PCI wrapper maps, and a fence queue per doorbell whose guest
 * notifier the core's workers set to interrupt the guest.
 *
 * Installed by accel-virt's tools/build_qemu.sh as hw/display/virtio-gpu-rvk.c.
 */
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/module.h"
#include "qemu/iov.h"
#include "qemu/mmap-alloc.h"
#include "hw/core/qdev-properties.h"
#include "hw/virtio/virtio.h"
#include "hw/virtio/virtio-bus.h"
#include "hw/virtio/virtio-gpu-bswap.h"
#include "hw/virtio/virtio-gpu-rvk.h"

static void (*virtio_gpu_rvk_parent_unrealize)(DeviceState *qdev);

/* Fence queues carry no requests: a kick does nothing. */
static void virtio_gpu_rvk_fence_kick(VirtIODevice *vdev, VirtQueue *vq)
{
}

static void virtio_gpu_rvk_realize(DeviceState *qdev, Error **errp)
{
    ERRP_GUARD();
    VirtIOGPURvk *r = VIRTIO_GPU_RVK(qdev);
    VirtIOGPUBase *b = VIRTIO_GPU_BASE(qdev);
    VirtIOGPU *g = VIRTIO_GPU(qdev);
    const RvkExecutorOps *executor = NULL;
    char error[256];

    /* Before the core: its capset reports host-visible memory from the region. */
    if (virtio_gpu_hostmem_enabled(b->conf)) {
        void *map = qemu_ram_mmap(-1, b->conf.hostmem, qemu_real_host_page_size(), 0, 0);

        if (map == MAP_FAILED) {
            error_setg_errno(errp, errno, "virtio-gpu-rvk: the hostmem region could not be reserved");
            return;
        }
        r->hostmem_mmap = map;
        memory_region_init_ram_ptr(&r->hostmem_background, NULL, "virtio-gpu-rvk-hostmem-background",
                                   b->conf.hostmem, map);
        memory_region_add_subregion(&b->hostmem, 0, &r->hostmem_background);
    }
    if (r->executor && r->executor[0]) {
        RvkExecutorOpsFunction ops = NULL;

        r->executor_library = g_module_open(r->executor, G_MODULE_BIND_LOCAL);
        if (!r->executor_library) {
            error_setg(errp, "virtio-gpu-rvk: executor '%s' could not be loaded: %s", r->executor,
                       g_module_error());
            return;
        }
        if (!g_module_symbol(r->executor_library, RVK_EXECUTOR_OPS_SYMBOL, (gpointer *)&ops) || !ops) {
            error_setg(errp, "virtio-gpu-rvk: executor '%s' exports no %s", r->executor, RVK_EXECUTOR_OPS_SYMBOL);
            return;
        }
        executor = ops();
    }
    virtio_gpu_rvk_host_init(r);
    r->core = rvk_device_create(&r->host, executor, r->host_gpu, virtio_gpu_blob_enabled(g->parent_obj.conf),
                                error, sizeof(error));
    if (!r->core) {
        error_setg(errp, "%s", error);
        return;
    }
    b->virtio_config.num_capsets = cpu_to_le32(rvk_device_capsets(r->core));
    if (rvk_device_capsets(r->core)) {
        b->conf.flags |= 1 << VIRTIO_GPU_FLAG_CONTEXT_INIT_ENABLED;
    }
    virtio_gpu_device_realize(qdev, errp);
    if (*errp) {
        return;
    }
    for (uint32_t i = 0; i < RVK_DEVICE_DOORBELLS; i++) {
        r->fence_queues[i] = virtio_add_queue(VIRTIO_DEVICE(qdev), 2, virtio_gpu_rvk_fence_kick);
    }
}

/* The workers are joined and the fence interrupts released before the queues go. */
static void virtio_gpu_rvk_unrealize(DeviceState *qdev)
{
    VirtIOGPURvk *r = VIRTIO_GPU_RVK(qdev);

    rvk_device_reset(r->core);
    for (uint32_t i = 0; i < RVK_DEVICE_DOORBELLS; i++) {
        virtio_del_queue(VIRTIO_DEVICE(qdev), RVK_DEVICE_FENCE_QUEUE_FIRST + i);
    }
    virtio_gpu_rvk_parent_unrealize(qdev);
}

static int virtio_gpu_rvk_set_status(VirtIODevice *vdev, uint8_t status)
{
    VirtIOGPURvk *r = VIRTIO_GPU_RVK(vdev);

    rvk_device_status(r->core, status);
    return 0;
}

/* The core answers the requests it owns; the base class answers the rest. */
static void virtio_gpu_rvk_process_cmd(VirtIOGPU *g, struct virtio_gpu_ctrl_command *cmd)
{
    VirtIOGPURvk *r = VIRTIO_GPU_RVK(g);
    struct virtio_gpu_ctrl_hdr header;

    if (iov_to_buf(cmd->elem.out_sg, cmd->elem.out_num, 0, &header, sizeof(header)) == sizeof(header)) {
        virtio_gpu_ctrl_hdr_bswap(&header);
        /* virtio_gpu_ctrl_response copies fence, flags and context from here. */
        cmd->cmd_hdr = header;
        if (rvk_device_control(r->core, cmd, header.type)) {
            return;
        }
    }
    virtio_gpu_simple_process_cmd(g, cmd);
}

static void virtio_gpu_rvk_reset(VirtIODevice *vdev)
{
    VirtIOGPURvk *r = VIRTIO_GPU_RVK(vdev);

    rvk_device_reset(r->core);
    virtio_gpu_reset(vdev);
}

/* After unrealize, or after a realize that failed. The hostmem reservation
 * stays mapped, as virtio-gpu-gl's does: the region may outlive the device.
 * The executor library is closed after the core, which calls into it. */
static void virtio_gpu_rvk_finalize(Object *obj)
{
    VirtIOGPURvk *r = VIRTIO_GPU_RVK(obj);

    (void)rvk_device_destroy(r->core);
    if (r->executor_library) {
        g_module_close(r->executor_library);
    }
}

/* Stores that are not a connected doorbell's ioeventfd land here and do nothing. */
static uint64_t virtio_gpu_rvk_doorbell_read(void *opaque, hwaddr offset, unsigned size)
{
    return 0;
}

static void virtio_gpu_rvk_doorbell_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
}

static const MemoryRegionOps virtio_gpu_rvk_doorbell_ops = {
    .read = virtio_gpu_rvk_doorbell_read,
    .write = virtio_gpu_rvk_doorbell_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void virtio_gpu_rvk_instance_init(Object *obj)
{
    VirtIOGPURvk *r = VIRTIO_GPU_RVK(obj);

    memory_region_init_io(&r->doorbells, obj, &virtio_gpu_rvk_doorbell_ops, r, "virtio-gpu-rvk-doorbells",
                          (uint64_t)RVK_DEVICE_DOORBELLS * RVK_DEVICE_DOORBELL_PAGE);
}

static const Property virtio_gpu_rvk_properties[] = {
    DEFINE_PROP_STRING("host-gpu", VirtIOGPURvk, host_gpu),
    DEFINE_PROP_STRING("executor", VirtIOGPURvk, executor),
};

static void virtio_gpu_rvk_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    VirtioDeviceClass *vdc = VIRTIO_DEVICE_CLASS(klass);
    VirtIOGPUClass *vgc = VIRTIO_GPU_CLASS(klass);

    vdc->realize = virtio_gpu_rvk_realize;
    virtio_gpu_rvk_parent_unrealize = vdc->unrealize;
    vdc->unrealize = virtio_gpu_rvk_unrealize;
    vdc->set_status = virtio_gpu_rvk_set_status;
    vgc->process_cmd = virtio_gpu_rvk_process_cmd;
    vdc->reset = virtio_gpu_rvk_reset;
    device_class_set_props(dc, virtio_gpu_rvk_properties);
}

static const TypeInfo virtio_gpu_rvk_info[] = {
    {
        .name = TYPE_VIRTIO_GPU_RVK,
        .parent = TYPE_VIRTIO_GPU,
        .instance_size = sizeof(VirtIOGPURvk),
        .instance_init = virtio_gpu_rvk_instance_init,
        .instance_finalize = virtio_gpu_rvk_finalize,
        .class_init = virtio_gpu_rvk_class_init,
    },
};

DEFINE_TYPES(virtio_gpu_rvk_info)

module_obj(TYPE_VIRTIO_GPU_RVK);
module_kconfig(VIRTIO_GPU);
module_dep("hw-display-virtio-gpu");

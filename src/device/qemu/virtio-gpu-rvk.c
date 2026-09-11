/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * virtio-gpu-rvk-device: the base virtio-gpu device handles the 2D protocol;
 * realize, reset, finalize and every control request reach the device core
 * through rvk_device.h first. No feature beyond stock virtio-gpu is advertised
 * until the core implements it.
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
#include "hw/virtio/virtio-gpu-bswap.h"
#include "hw/virtio/virtio-gpu-rvk.h"

static void virtio_gpu_rvk_realize(DeviceState *qdev, Error **errp)
{
    VirtIOGPURvk *r = VIRTIO_GPU_RVK(qdev);
    VirtIOGPUBase *b = VIRTIO_GPU_BASE(qdev);
    VirtIOGPU *g = VIRTIO_GPU(qdev);
    char error[256];

    virtio_gpu_rvk_host_init(r);
    r->core = rvk_device_create(&r->host, r->host_gpu, virtio_gpu_blob_enabled(g->parent_obj.conf),
                                error, sizeof(error));
    if (!r->core) {
        error_setg(errp, "%s", error);
        return;
    }
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
    virtio_gpu_device_realize(qdev, errp);
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
 * stays mapped, as virtio-gpu-gl's does: the region may outlive the device. */
static void virtio_gpu_rvk_finalize(Object *obj)
{
    VirtIOGPURvk *r = VIRTIO_GPU_RVK(obj);

    (void)rvk_device_destroy(r->core);
}

static const Property virtio_gpu_rvk_properties[] = {
    DEFINE_PROP_STRING("host-gpu", VirtIOGPURvk, host_gpu),
};

static void virtio_gpu_rvk_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    VirtioDeviceClass *vdc = VIRTIO_DEVICE_CLASS(klass);
    VirtIOGPUClass *vgc = VIRTIO_GPU_CLASS(klass);

    vdc->realize = virtio_gpu_rvk_realize;
    vgc->process_cmd = virtio_gpu_rvk_process_cmd;
    vdc->reset = virtio_gpu_rvk_reset;
    device_class_set_props(dc, virtio_gpu_rvk_properties);
}

static const TypeInfo virtio_gpu_rvk_info[] = {
    {
        .name = TYPE_VIRTIO_GPU_RVK,
        .parent = TYPE_VIRTIO_GPU,
        .instance_size = sizeof(VirtIOGPURvk),
        .instance_finalize = virtio_gpu_rvk_finalize,
        .class_init = virtio_gpu_rvk_class_init,
    },
};

DEFINE_TYPES(virtio_gpu_rvk_info)

module_obj(TYPE_VIRTIO_GPU_RVK);
module_kconfig(VIRTIO_GPU);
module_dep("hw-display-virtio-gpu");

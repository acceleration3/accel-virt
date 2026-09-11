/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * virtio-gpu-rvk-device: the base virtio-gpu device handles the 2D protocol;
 * realize, reset and finalize reach the device core through rvk_device.h. No
 * feature beyond stock virtio-gpu is advertised until the core implements it.
 *
 * Installed by accel-virt's tools/build_qemu.sh as hw/display/virtio-gpu-rvk.c.
 */
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/module.h"
#include "hw/core/qdev-properties.h"
#include "hw/virtio/virtio.h"
#include "hw/virtio/virtio-gpu-rvk.h"

static void virtio_gpu_rvk_realize(DeviceState *qdev, Error **errp)
{
    VirtIOGPURvk *r = VIRTIO_GPU_RVK(qdev);
    char error[256];

    virtio_gpu_rvk_host_init(r);
    r->core = rvk_device_create(&r->host, r->host_gpu, error, sizeof(error));
    if (!r->core) {
        error_setg(errp, "%s", error);
        return;
    }
    virtio_gpu_device_realize(qdev, errp);
}

static void virtio_gpu_rvk_reset(VirtIODevice *vdev)
{
    VirtIOGPURvk *r = VIRTIO_GPU_RVK(vdev);

    rvk_device_reset(r->core);
    virtio_gpu_reset(vdev);
}

/* After unrealize, or after a realize that failed. */
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

    vdc->realize = virtio_gpu_rvk_realize;
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

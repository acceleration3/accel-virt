/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/module.h"
#include "hw/pci/pci.h"
#include "hw/core/qdev-properties.h"
#include "hw/virtio/virtio.h"
#include "hw/virtio/virtio-bus.h"
#include "hw/virtio/virtio-gpu-pci.h"
#include "hw/virtio/virtio-pci.h"
#include "hw/virtio/virtio-gpu-av.h"
#include "qom/object.h"

#define TYPE_VIRTIO_GPU_AV_PCI "virtio-gpu-av-pci"
OBJECT_DECLARE_SIMPLE_TYPE(VirtIOGPUAvPCI, VIRTIO_GPU_AV_PCI)

#define VIRTIO_GPU_AV_PCI_SUBSYSTEM_ID 0x5256

struct VirtIOGPUAvPCI {
    VirtIOGPUPCIBase parent_obj;

    VirtIOGPUAv vdev;
};

static void (*virtio_gpu_av_pci_parent_realize)(VirtIOPCIProxy *proxy, Error **errp);

static void virtio_gpu_av_pci_realize(VirtIOPCIProxy *proxy, Error **errp)
{
    ERRP_GUARD();
    VirtIOGPUAvPCI *dev = VIRTIO_GPU_AV_PCI(proxy);
    AvPciIdentity host;
    uint8_t *config = proxy->pci_dev.config;

    pci_register_bar(&proxy->pci_dev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &dev->vdev.doorbells);
    virtio_pci_add_shm_cap(proxy, 0, 0, memory_region_size(&dev->vdev.doorbells), AV_DEVICE_DOORBELL_SHM_ID);
    virtio_gpu_av_pci_parent_realize(proxy, errp);
    if (*errp) {
        return;
    }
    if (!virtio_gpu_av_pci_identity(&dev->vdev, &host)) {
        return;
    }
    pci_config_set_vendor_id(config, host.vendor_id);
    pci_config_set_device_id(config, host.device_id);
    pci_config_set_revision(config, host.revision);
}

static void virtio_gpu_av_pci_initfn(Object *obj)
{
    VirtIOGPUAvPCI *dev = VIRTIO_GPU_AV_PCI(obj);

    virtio_instance_init_common(obj, &dev->vdev, sizeof(dev->vdev), TYPE_VIRTIO_GPU_AV);
    VIRTIO_GPU_PCI_BASE(obj)->vgpu = VIRTIO_GPU_BASE(&dev->vdev);
    VIRTIO_PCI(obj)->flags |= VIRTIO_PCI_FLAG_USE_IOEVENTFD;
}

static void virtio_gpu_av_pci_class_init(ObjectClass *klass, const void *data)
{
    PCIDeviceClass *pcidev_k = PCI_DEVICE_CLASS(klass);
    VirtioPCIClass *k = VIRTIO_PCI_CLASS(klass);

    virtio_gpu_av_pci_parent_realize = k->realize;
    k->realize = virtio_gpu_av_pci_realize;

    pcidev_k->subsystem_vendor_id = PCI_SUBVENDOR_ID_REDHAT_QUMRANET;
    pcidev_k->subsystem_id = VIRTIO_GPU_AV_PCI_SUBSYSTEM_ID;
}

static const TypeInfo virtio_gpu_av_pci_info[] = {
    {
        .name = TYPE_VIRTIO_GPU_AV_PCI,
        .parent = TYPE_VIRTIO_GPU_PCI_BASE,
        .instance_size = sizeof(VirtIOGPUAvPCI),
        .instance_init = virtio_gpu_av_pci_initfn,
        .class_init = virtio_gpu_av_pci_class_init,
        .interfaces = (const InterfaceInfo[]) {
            { INTERFACE_PCIE_DEVICE },
            { },
        },
    },
};

DEFINE_TYPES(virtio_gpu_av_pci_info)

module_obj(TYPE_VIRTIO_GPU_AV_PCI);
module_kconfig(VIRTIO_PCI);
module_dep("hw-display-virtio-gpu-pci");
module_dep("hw-display-virtio-gpu-av");

/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * virtio-gpu-rvk-pci: the PCI function the guest's rvkgpu.sys binds to - a
 * virtio-gpu (1af4:1050) with subsystem 1af4:5256, "RV", whose doorbell pages
 * are BAR 0, located by a shared memory capability.
 *
 * Installed by accel-virt's tools/build_qemu.sh as hw/display/virtio-gpu-pci-rvk.c.
 */
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/module.h"
#include "hw/pci/pci.h"
#include "hw/core/qdev-properties.h"
#include "hw/virtio/virtio.h"
#include "hw/virtio/virtio-bus.h"
#include "hw/virtio/virtio-gpu-pci.h"
#include "hw/virtio/virtio-pci.h"
#include "hw/virtio/virtio-gpu-rvk.h"
#include "qom/object.h"

#define TYPE_VIRTIO_GPU_RVK_PCI "virtio-gpu-rvk-pci"
OBJECT_DECLARE_SIMPLE_TYPE(VirtIOGPURvkPCI, VIRTIO_GPU_RVK_PCI)

#define VIRTIO_GPU_RVK_PCI_SUBSYSTEM_ID 0x5256

struct VirtIOGPURvkPCI {
    VirtIOGPUPCIBase parent_obj;

    VirtIOGPURvk vdev;
};

static void (*virtio_gpu_rvk_pci_parent_realize)(VirtIOPCIProxy *proxy, Error **errp);

/* BAR 0 is free: virtio-gpu-pci forces virtio 1, so there is no legacy I/O BAR. */
static void virtio_gpu_rvk_pci_realize(VirtIOPCIProxy *proxy, Error **errp)
{
    VirtIOGPURvkPCI *dev = VIRTIO_GPU_RVK_PCI(proxy);

    pci_register_bar(&proxy->pci_dev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &dev->vdev.doorbells);
    virtio_pci_add_shm_cap(proxy, 0, 0, memory_region_size(&dev->vdev.doorbells), RVK_DEVICE_DOORBELL_SHM_ID);
    virtio_gpu_rvk_pci_parent_realize(proxy, errp);
}

static void virtio_gpu_rvk_pci_initfn(Object *obj)
{
    VirtIOGPURvkPCI *dev = VIRTIO_GPU_RVK_PCI(obj);

    virtio_instance_init_common(obj, &dev->vdev, sizeof(dev->vdev), TYPE_VIRTIO_GPU_RVK);
    VIRTIO_GPU_PCI_BASE(obj)->vgpu = VIRTIO_GPU_BASE(&dev->vdev);
}

static void virtio_gpu_rvk_pci_class_init(ObjectClass *klass, const void *data)
{
    PCIDeviceClass *pcidev_k = PCI_DEVICE_CLASS(klass);
    VirtioPCIClass *k = VIRTIO_PCI_CLASS(klass);

    virtio_gpu_rvk_pci_parent_realize = k->realize;
    k->realize = virtio_gpu_rvk_pci_realize;

    pcidev_k->subsystem_vendor_id = PCI_SUBVENDOR_ID_REDHAT_QUMRANET;
    pcidev_k->subsystem_id = VIRTIO_GPU_RVK_PCI_SUBSYSTEM_ID;
}

static const TypeInfo virtio_gpu_rvk_pci_info[] = {
    {
        .name = TYPE_VIRTIO_GPU_RVK_PCI,
        .parent = TYPE_VIRTIO_GPU_PCI_BASE,
        .instance_size = sizeof(VirtIOGPURvkPCI),
        .instance_init = virtio_gpu_rvk_pci_initfn,
        .class_init = virtio_gpu_rvk_pci_class_init,
        .interfaces = (const InterfaceInfo[]) {
            { INTERFACE_PCIE_DEVICE },
            { },
        },
    },
};

DEFINE_TYPES(virtio_gpu_rvk_pci_info)

module_obj(TYPE_VIRTIO_GPU_RVK_PCI);
module_kconfig(VIRTIO_PCI);
module_dep("hw-display-virtio-gpu-pci");

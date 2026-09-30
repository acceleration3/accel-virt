/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * virtio-gpu-rvk-pci: the PCI function the guest's rvkgpu.sys binds to - a
 * virtio-gpu with subsystem 1af4:5256, "RV", whose doorbell pages are BAR 0,
 * located by a shared memory capability. Its vendor, device and revision are
 * the bound host GPU's, or virtio-gpu's own 1af4:1050 when that GPU has no PCI
 * function to name.
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

/* BAR 0 is free: virtio-gpu-pci forces virtio 1, so there is no legacy I/O BAR.
 *
 * The vendor, device and revision are the bound host GPU's once the parent has
 * realized - virtio-pci writes its own when the virtio device is plugged, which
 * is inside that realize - so a Windows guest names the adapter after the host
 * GPU in Device Manager, WMI and DXGI. The subsystem stays 1af4:5256: it is how
 * rvkgpu.inf and the guest's own programs tell this function from any other,
 * and the driver finds everything else through the virtio capabilities, never
 * the IDs. */
static void virtio_gpu_rvk_pci_realize(VirtIOPCIProxy *proxy, Error **errp)
{
    ERRP_GUARD();
    VirtIOGPURvkPCI *dev = VIRTIO_GPU_RVK_PCI(proxy);
    RvkPciIdentity host;
    uint8_t *config = proxy->pci_dev.config;

    pci_register_bar(&proxy->pci_dev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &dev->vdev.doorbells);
    virtio_pci_add_shm_cap(proxy, 0, 0, memory_region_size(&dev->vdev.doorbells), RVK_DEVICE_DOORBELL_SHM_ID);
    virtio_gpu_rvk_pci_parent_realize(proxy, errp);
    if (*errp) {
        return;
    }
    if (!virtio_gpu_rvk_pci_identity(&dev->vdev, &host)) {
        return;
    }
    pci_config_set_vendor_id(config, host.vendor_id);
    pci_config_set_device_id(config, host.device_id);
    pci_config_set_revision(config, host.revision);
}

/* ioeventfd on by default, as virtio-blk-pci and virtio-net-pci have it;
 * virtio-gpu-pci leaves it off. Off, a queue kick is an MMIO exit QEMU handles
 * on the vCPU under the BQL, and the main loop holds the BQL while it draws
 * the display: the vCPU waited milliseconds in the kick, and the display DPC
 * it was running - and every interrupt aimed at that vCPU - with it. Set here
 * rather than as a property default because the property is the base's, and
 * after its default so that ioeventfd=off still wins. */
static void virtio_gpu_rvk_pci_initfn(Object *obj)
{
    VirtIOGPURvkPCI *dev = VIRTIO_GPU_RVK_PCI(obj);

    virtio_instance_init_common(obj, &dev->vdev, sizeof(dev->vdev), TYPE_VIRTIO_GPU_RVK);
    VIRTIO_GPU_PCI_BASE(obj)->vgpu = VIRTIO_GPU_BASE(&dev->vdev);
    VIRTIO_PCI(obj)->flags |= VIRTIO_PCI_FLAG_USE_IOEVENTFD;
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
module_dep("hw-display-virtio-gpu-rvk");

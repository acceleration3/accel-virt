/* The PCI identity an adapter reports to user mode: vendor and device, and the
 * subsystem pair that tells virtio-gpu-av-pci from stock virtio-gpu-pci. */
#ifndef AV_PCI_IDENTITY_HPP
#define AV_PCI_IDENTITY_HPP

#include <stdint.h>

namespace av {

class PciIdentity {
public:
    PciIdentity();
    PciIdentity(uint32_t vendor, uint32_t device, uint32_t subsystem_vendor, uint32_t subsystem);

    bool same_as(const PciIdentity &other) const;
    /* Whether `other` carries the same subsystem pair, whatever its vendor and
     * device. virtio-gpu-av-pci presents its host GPU's vendor and device,
     * which differ from host to host, so its subsystem 1af4:5256 is what
     * names it. */
    bool same_subsystem_as(const PciIdentity &other) const;
    /* What DXGI and DXCore report as the adapter's VendorId and DeviceId
     * (feature 055): the same two values the identity was matched on. */
    uint32_t vendor() const;
    uint32_t device() const;

private:
    uint32_t vendor_;
    uint32_t device_;
    uint32_t subsystem_vendor_;
    uint32_t subsystem_;
};

}  // namespace av

#endif  // AV_PCI_IDENTITY_HPP

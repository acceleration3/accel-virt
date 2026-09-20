/* The PCI identity an adapter reports to user mode: vendor and device, and the
 * subsystem pair that tells virtio-gpu-rvk-pci from stock virtio-gpu-pci. */
#ifndef AV_PCI_IDENTITY_HPP
#define AV_PCI_IDENTITY_HPP

#include <stdint.h>

namespace av {

class PciIdentity {
public:
    PciIdentity();
    PciIdentity(uint32_t vendor, uint32_t device, uint32_t subsystem_vendor, uint32_t subsystem);

    bool same_as(const PciIdentity &other) const;
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

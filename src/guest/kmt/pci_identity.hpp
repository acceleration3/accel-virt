/* The PCI identity an adapter reports to user mode: vendor and device, and the
 * subsystem pair that tells virtio-gpu-av-pci from stock virtio-gpu-pci. */
#ifndef AV_PCI_IDENTITY_HPP
#define AV_PCI_IDENTITY_HPP

#include <stdint.h>

namespace av {

class TextBuffer;

class PciIdentity {
public:
    /* The PCI vendors the user-mode drivers name: Red Hat's, which is the
     * virtio function avgpu drives, and the two whose vendor libraries
     * (docs/vendor-libraries.md) and OpenCL extensions answer for the host
     * GPU avgpu presents. */
    static constexpr uint32_t virtio_vendor = 0x1AF4u;
    static constexpr uint32_t amd_vendor = 0x1002u;
    static constexpr uint32_t nvidia_vendor = 0x10DEu;

    PciIdentity();
    PciIdentity(uint32_t vendor, uint32_t device, uint32_t subsystem_vendor, uint32_t subsystem,
                uint32_t revision = 0u);

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
    uint32_t subsystem_vendor() const;
    uint32_t subsystem() const;
    /* The revision, which identifies no adapter (same_as leaves it out) but
     * names the silicon a vendor library's hardware ID ends in. */
    uint32_t revision() const;
    /* "VEN_1002&DEV_13C0&SUBSYS_52561AF4&REV_C9": the hardware ID Windows gives
     * the device, after its bus's "PCI\\". */
    void     append_hardware_id(TextBuffer &text) const;

private:
    uint32_t vendor_;
    uint32_t device_;
    uint32_t subsystem_vendor_;
    uint32_t subsystem_;
    uint32_t revision_;
};

}  // namespace av

#endif  // AV_PCI_IDENTITY_HPP

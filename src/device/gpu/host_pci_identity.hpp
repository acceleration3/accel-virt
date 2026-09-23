/* The vendor, device and revision of the PCI function behind the host GPU a
 * device instance is bound to, read from sysfs, which virtio-gpu-rvk-pci
 * presents as its own. A Windows guest reads an adapter's IDs from its PCI
 * configuration space and nowhere else - dxgkrnl has no query a driver could
 * answer them through - so this is what makes Device Manager, WMI and DXGI
 * name the host's GPU. A GPU with no PCI function (llvmpipe), or one sysfs
 * does not describe, has nothing to present, and the function keeps
 * virtio-gpu's own IDs. */
#ifndef AV_HOST_PCI_IDENTITY_HPP
#define AV_HOST_PCI_IDENTITY_HPP

#include "device/abi/rvk_device.h"

#include <stddef.h>
#include <stdint.h>

namespace av {

class Host;
class PciAddress;

class HostPciIdentity {
public:
    explicit HostPciIdentity(Host &host);
    HostPciIdentity(const HostPciIdentity &) = delete;
    HostPciIdentity &operator=(const HostPciIdentity &) = delete;

    /* Main loop. The identity of host GPU `gpu_index`'s function, replacing
     * whatever was read before; logged when there is one. */
    void read(uint32_t gpu_index);
    /* Fills `identity` when there is one to present; false, leaving it
     * untouched, when there is not. */
    bool presented(RvkPciIdentity &identity) const;

private:
    /* sysfs writes each of these as "0x%04x\n" or "0x%02x\n". */
    static constexpr size_t   attribute_bytes = 16u;
    static constexpr uint32_t id_limit        = 0xFFFFu;
    static constexpr uint32_t revision_limit  = 0xFFu;

    bool read_attribute(const PciAddress &address, const char *attribute, uint32_t limit, uint32_t &value);
    /* "0x" and at least one hex digit, then only whitespace, at most `limit`. */
    bool parse(const char *text, size_t length, uint32_t limit, uint32_t &value) const;
    void report(const PciAddress &address);

    Host    &host_;
    uint32_t vendor_id_;
    uint32_t device_id_;
    uint32_t revision_;
    bool     present_;
};

}  // namespace av

#endif  // AV_HOST_PCI_IDENTITY_HPP

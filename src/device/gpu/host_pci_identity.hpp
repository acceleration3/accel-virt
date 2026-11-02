#ifndef AV_HOST_PCI_IDENTITY_HPP
#define AV_HOST_PCI_IDENTITY_HPP

#include "device/abi/av_device.h"

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

    void read(uint32_t gpu_index);
    bool presented(AvPciIdentity &identity) const;

private:
    static constexpr size_t   attribute_bytes = 16u;
    static constexpr uint32_t id_limit        = 0xFFFFu;
    static constexpr uint32_t revision_limit  = 0xFFu;

    bool read_attribute(const PciAddress &address, const char *attribute, uint32_t limit, uint32_t &value);
    bool parse(const char *text, size_t length, uint32_t limit, uint32_t &value) const;
    void report(const PciAddress &address);

    Host    &host_;
    uint32_t vendor_id_;
    uint32_t device_id_;
    uint32_t revision_;
    bool     present_;
};

}

#endif

#ifndef AV_PCI_IDENTITY_HPP
#define AV_PCI_IDENTITY_HPP

#include <stdint.h>

namespace av {

class TextBuffer;

class PciIdentity {
public:
    static constexpr uint32_t virtio_vendor = 0x1AF4u;
    static constexpr uint32_t amd_vendor = 0x1002u;
    static constexpr uint32_t nvidia_vendor = 0x10DEu;

    PciIdentity();
    PciIdentity(uint32_t vendor, uint32_t device, uint32_t subsystem_vendor, uint32_t subsystem,
                uint32_t revision = 0u);

    bool same_as(const PciIdentity &other) const;
    bool same_subsystem_as(const PciIdentity &other) const;
    uint32_t vendor() const;
    uint32_t device() const;
    uint32_t subsystem_vendor() const;
    uint32_t subsystem() const;
    uint32_t revision() const;
    void     append_hardware_id(TextBuffer &text) const;

private:
    uint32_t vendor_;
    uint32_t device_;
    uint32_t subsystem_vendor_;
    uint32_t subsystem_;
    uint32_t revision_;
};

}

#endif

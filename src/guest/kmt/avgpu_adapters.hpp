/* Every avgpu adapter in the machine, held open while this lives: what each
 * user-mode driver that shows the guest's GPUs builds its devices from, so
 * they all show the same adapters in the same order.
 *
 * An adapter is avgpu's by its PCI subsystem 1af4:5256 alone:
 * virtio-gpu-av-pci presents its host GPU's vendor and device
 * (docs/protocol.md), so it reads as 1002:7550 on one host and 10de:2684 on
 * the next. The one the primary display is on comes first, as DXGI lists it
 * first and a Direct3D application gets it by default; the rest follow in the
 * kernel's order, which changes from boot to boot. An ICD that shows one
 * device shows that one: an ICD on the integrated GPU while the application's
 * Direct3D runs on the discrete one is slower, and Affinity's canvas,
 * computing on one and drawing on the other, drew stale tiles. */
#ifndef AV_AVGPU_ADAPTERS_HPP
#define AV_AVGPU_ADAPTERS_HPP

#include "core/support/status.hpp"
#include "guest/kmt/pci_identity.hpp"

#include <stdint.h>

namespace av {

class Kmt;

class AvgpuAdapters {
public:
    static constexpr uint32_t max_adapters = 16u;
    /* avgpu's PCI identity: virtio-gpu's vendor and device, and the
     * subsystem only this device presents. */
    static constexpr uint32_t virtio_vendor = 0x1AF4u;
    static constexpr uint32_t virtio_gpu_device = 0x1050u;
    static constexpr uint32_t av_subsystem = 0x5256u;

    explicit AvgpuAdapters(Kmt &kmt);
    ~AvgpuAdapters();
    AvgpuAdapters(const AvgpuAdapters &) = delete;
    AvgpuAdapters &operator=(const AvgpuAdapters &) = delete;

    /* Lets go of what an earlier learn held and asks Windows again; every
     * adapter that is not avgpu's is closed at once. The kernel's refusal to
     * enumerate is the answer, with nothing held. */
    [[nodiscard]] Status learn();
    uint32_t             count() const;
    /* The `index`th adapter's handle; 0 past count() or once taken. */
    uint32_t             adapter(uint32_t index) const;
    /* Its LUID, packed as Kmt::adapter_luid packs it; 0 where Windows gave
     * none. */
    uint64_t             luid(uint32_t index) const;
    const PciIdentity   &identity(uint32_t index) const;
    /* Whether the primary display is on it: only ever the first. */
    bool                 primary(uint32_t index) const;
    /* The index of the adapter whose LUID is `luid`, or count() when none
     * is - and for a `luid` of 0, which names no adapter. */
    uint32_t             index_of(uint64_t luid) const;
    /* Hands the `index`th adapter's handle to the caller, who closes it;
     * 0 past count() or when already taken. */
    uint32_t             take(uint32_t index);
    /* Closes every handle not taken. */
    void                 close();

    bool invariants_hold() const;

private:
    void keep_or_close(uint32_t adapter);
    void put_primary_first(uint64_t primary_luid);

    Kmt        &kmt_;
    uint32_t    adapters_[max_adapters];
    uint64_t    luids_[max_adapters];
    PciIdentity identities_[max_adapters];
    /* What identity() answers past the end: matches no adapter. */
    PciIdentity none_;
    uint32_t    count_;
    bool        primary_;
};

}  // namespace av

#endif  // AV_AVGPU_ADAPTERS_HPP

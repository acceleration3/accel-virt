#ifndef AV_AVGPU_ADAPTERS_HPP
#define AV_AVGPU_ADAPTERS_HPP

#include "core/support/status.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "proto/context/av_device_count.hpp"

#include <stdint.h>

namespace av {

class Kmt;
struct AvgpuAdaptersProbe;

class AvgpuAdapters {
public:
    static constexpr uint32_t max_adapters = most_av_devices;
    static constexpr uint32_t virtio_gpu_device = 0x1050u;
    static constexpr uint32_t av_subsystem = 0x5256u;

    explicit AvgpuAdapters(Kmt &kmt);
    ~AvgpuAdapters();
    AvgpuAdapters(const AvgpuAdapters &) = delete;
    AvgpuAdapters &operator=(const AvgpuAdapters &) = delete;

    [[nodiscard]] Status learn();
    uint32_t             count() const;
    uint32_t             adapter(uint32_t index) const;
    uint64_t             luid(uint32_t index) const;
    uint32_t             luids(uint64_t (&into)[max_adapters]) const;
    const PciIdentity   &identity(uint32_t index) const;
    bool                 primary(uint32_t index) const;
    uint32_t             index_of(uint64_t luid) const;
    uint32_t             take(uint32_t index);
    void                 close();

    bool invariants_hold() const;

private:
    void keep_or_close(uint32_t adapter);
    void put_primary_first(uint64_t primary_luid);

    Kmt        &kmt_;
    uint32_t    adapters_[max_adapters];
    uint64_t    luids_[max_adapters];
    PciIdentity identities_[max_adapters];
    PciIdentity none_;
    uint32_t    count_;
    bool        primary_;

    friend struct AvgpuAdaptersProbe;
};

}

#endif

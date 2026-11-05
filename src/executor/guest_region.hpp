#ifndef AV_GUEST_REGION_HPP
#define AV_GUEST_REGION_HPP

#include "proto/wire/guest_bytes.hpp"

#include <stdint.h>

namespace av {

class BlobRole;
class SourceMemory;

class GuestRegion {
public:
    explicit GuestRegion(GuestBytes &bytes);
    GuestRegion(const GuestRegion &) = delete;
    GuestRegion &operator=(const GuestRegion &) = delete;

    [[nodiscard]] bool adopt(SourceMemory &memory, const BlobRole &role, uint64_t &handle);
    void               abandon(const SourceMemory &memory);
    bool               is(const SourceMemory &memory) const;
    bool               held() const;
    uint64_t           base_of(const SourceMemory &memory) const;
    SourceMemory      *memory_at(uint64_t offset) const;

    bool invariants_hold() const;

private:
    friend struct GuestRegionProbe;

    GuestBytes   &bytes_;
    SourceMemory *memories_[GuestBytes::slot_count];
};

}

#endif

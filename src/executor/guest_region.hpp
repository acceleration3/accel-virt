#ifndef AV_GUEST_REGION_HPP
#define AV_GUEST_REGION_HPP

#include <stdint.h>

namespace av {

class GuestBytes;
class SourceMemory;

class GuestRegion {
public:
    explicit GuestRegion(GuestBytes &bytes);
    GuestRegion(const GuestRegion &) = delete;
    GuestRegion &operator=(const GuestRegion &) = delete;

    [[nodiscard]] bool adopt(const SourceMemory &memory, uint64_t &handle);
    void               abandon(const SourceMemory &memory);
    bool               is(const SourceMemory &memory) const;
    bool               held() const;

    bool invariants_hold() const;

private:
    friend struct GuestRegionProbe;

    GuestBytes &bytes_;
};

}

#endif

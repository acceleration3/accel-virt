#ifndef AV_HOST_VISIBLE_SEGMENT_HPP
#define AV_HOST_VISIBLE_SEGMENT_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

struct HostVisibleSegmentProbe;

class CapsetInfo;
class PciBaseAddresses;
class VirtioPciLayout;

class HostVisibleSegment {
public:
    static constexpr uint64_t page_bytes = 0x1000u;

    explicit HostVisibleSegment(Platform &platform);
    HostVisibleSegment(const HostVisibleSegment &) = delete;
    HostVisibleSegment &operator=(const HostVisibleSegment &) = delete;

    void                 discover(const PciBaseAddresses &bars, const VirtioPciLayout &layout);
    void                 offer(const CapsetInfo &capset);
    void                 forget();

    bool                 offered() const;
    uint64_t             base() const;
    uint64_t             size() const;

    bool invariants_hold() const;

private:
    static constexpr uint8_t  host_visible_region = 1u;

    friend struct HostVisibleSegmentProbe;

    Platform &platform_;
    uint64_t  base_;
    uint64_t  size_;
    bool      offered_;
};

}

#endif

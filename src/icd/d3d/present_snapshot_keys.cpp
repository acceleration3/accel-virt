#include "icd/d3d/present_snapshot_keys.hpp"

#include "core/trace/function_scope.hpp"

#include <x86intrin.h>

namespace av::d3d {

__attribute__((target("rdrnd"))) PresentSnapshotKeys::PresentSnapshotKeys() : base_(0u), issued_(0u)
{
    AV_SCOPE("PresentSnapshotKeys::PresentSnapshotKeys");
    unsigned int low = 0u;
    unsigned int high = 0u;
    bool         drawn = false;
    for (uint32_t attempt = 0u; attempt < attempts && !drawn; attempt++)
        drawn = _rdrand32_step(&low) != 0 && _rdrand32_step(&high) != 0;
    base_ = drawn ? (uint64_t{high} << 32u) | low : __rdtsc() * 0x9E3779B97F4A7C15ull;
}

uint64_t PresentSnapshotKeys::next()
{
    AV_SCOPE("PresentSnapshotKeys::next");
    uint64_t key = base_ + ++issued_;
    if (key == 0u) key = base_ + ++issued_;
    return key;
}

}  // namespace av::d3d

/* The LUID D3DKMT gave an adapter, which the loader matches physical devices
 * against and VkPhysicalDeviceIDProperties reports.
 *
 * Integration point: Kmt gains adapter_luid(adapter, luid) in T021 (another
 * branch), and this seam is then deleted in favour of it - the spelling is the
 * same so the change is the type of one constructor parameter. A LUID is carried
 * as the 64-bit value whose little-endian bytes are the LUID structure's
 * (LowPart, then HighPart), which is what deviceLUID holds and how a LUID
 * passed by value crosses the x64 calling convention. */
#ifndef AV_ICD_ADAPTER_LUIDS_HPP
#define AV_ICD_ADAPTER_LUIDS_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av::icd {

class AdapterLuids {
public:
    AdapterLuids(const AdapterLuids &) = delete;
    AdapterLuids &operator=(const AdapterLuids &) = delete;

    [[nodiscard]] virtual Status adapter_luid(uint32_t adapter, uint64_t &luid) = 0;

protected:
    AdapterLuids() = default;
    ~AdapterLuids() = default;
};

}  // namespace av::icd

#endif  // AV_ICD_ADAPTER_LUIDS_HPP

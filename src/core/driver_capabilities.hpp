/* Every capability the adapter reports to dxgkrnl, decided in one place.
 * Constitution VI: a capability is set here only by the feature that
 * implements it, and DXGADAPTER::Initialize's rules (tests/harness
 * FakeDxgadapter) reject a claim whose DDIs are not registered. */
#ifndef AV_DRIVER_CAPABILITIES_HPP
#define AV_DRIVER_CAPABILITIES_HPP

#include "platform.hpp"
#include "status.hpp"

#include <stdint.h>

namespace av {

class AdapterInfoQuery;

class DriverCapabilities {
public:
    explicit DriverCapabilities(Platform &platform);
    DriverCapabilities(const DriverCapabilities &) = delete;
    DriverCapabilities &operator=(const DriverCapabilities &) = delete;

    /* DXGKQAITYPE_DRIVERCAPS: the whole DXGK_DRIVERCAPS, every unclaimed field zero. */
    [[nodiscard]] Status report_driver_caps(const AdapterInfoQuery &query) const;
    /* DXGKQAITYPE_WDDMDEVICECAPS, which dxgkrnl may ask before StartDevice. */
    [[nodiscard]] Status report_wddm_device_caps(const AdapterInfoQuery &query) const;

private:
    static constexpr int32_t wddm_1_0 = 0x1000;   /* DXGKDDI_WDDMv1 */
    /* The virtio device addresses all of guest physical memory. */
    static constexpr int64_t any_address = -1;
    static constexpr uint32_t interface_version_bytes = 4u;   /* DXGK_WDDMDEVICECAPSIN */
    /* HardwareCursor draws every pointer type up to its 64x64 image. */
    static constexpr uint32_t pointer_caps = 0x7u;   /* Monochrome | Color | MaskedColor */

    Platform &platform_;
    int32_t   wddm_version_;
    int64_t   highest_acceptable_address_;
};

}  // namespace av

#endif  // AV_DRIVER_CAPABILITIES_HPP

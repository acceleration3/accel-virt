/* DXGKARGCB_QUERYFEATURESUPPORT (d3dkmddi.h:10184), what `DxgkCbQueryFeatureSupport`
 * takes: the feature a driver implements, and whether this OS allows it to be
 * turned on. A DDI record; src/km/ddi_record_layout.cpp proves each offset. */
#ifndef AV_DXGKARGCB_QUERY_FEATURE_SUPPORT_RECORD_HPP
#define AV_DXGKARGCB_QUERY_FEATURE_SUPPORT_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargcbQueryFeatureSupportRecord {
    void    *DeviceHandle;
    uint32_t FeatureId;
    uint32_t DriverSupportState;
    uint8_t  Enabled;
};

/* DXGK_FEATURE_ID: the driver category is 0, so a driver feature's id is its
 * own number (d3dukmdt.h:2204). */
enum : uint32_t {
    feature_hwsch = 0u,
    feature_user_mode_submission = 4u,
};

/* DXGK_FEATURE_SUPPORT_* (d3dkmdt.h:2153): what the driver says of its own
 * implementation, which the OS weighs against its policy. */
enum : uint32_t {
    feature_support_always_off = 0u,
    feature_support_experimental = 1u,
    feature_support_stable = 2u,
    feature_support_always_on = 3u,
};

}  // namespace av

#endif  // AV_DXGKARGCB_QUERY_FEATURE_SUPPORT_RECORD_HPP

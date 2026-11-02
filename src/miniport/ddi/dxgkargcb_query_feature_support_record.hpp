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

enum : uint32_t {
    feature_hwsch = 0u,
    feature_user_mode_submission = 4u,
    feature_native_fence = 37u,
};

enum : uint32_t {
    feature_support_always_off = 0u,
    feature_support_experimental = 1u,
    feature_support_stable = 2u,
    feature_support_always_on = 3u,
};

}

#endif

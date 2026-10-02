/* DXGK_WDDMDEVICECAPS, answered for DXGKQAITYPE_WDDMDEVICECAPS. A DDI record;
 * src/km/ddi_record_layout.cpp proves its layout. */
#ifndef AV_DXGK_WDDM_DEVICE_CAPS_RECORD_HPP
#define AV_DXGK_WDDM_DEVICE_CAPS_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkWddmDeviceCapsRecord {
    int32_t WDDMVersion;
};

}  // namespace av

#endif  // AV_DXGK_WDDM_DEVICE_CAPS_RECORD_HPP

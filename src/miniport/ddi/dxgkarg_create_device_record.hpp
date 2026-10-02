/* DXGKARG_CREATEDEVICE, as DxgkDdiCreateDevice receives and answers it. A DDI
 * record: Flags is the DXGK_CREATEDEVICEFLAGS Value, widened by the pInfo
 * union. src/km/ddi_record_layout.cpp proves each offset. */
#ifndef AV_DXGKARG_CREATE_DEVICE_RECORD_HPP
#define AV_DXGKARG_CREATE_DEVICE_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargCreateDeviceRecord {
    void     *hDevice;
    uint32_t Flags;
    uint32_t FlagsPadding;
    uint32_t Pasid;
    uint32_t PasidPadding;
    void     *hKmdProcess;
};

}  // namespace av

#endif  // AV_DXGKARG_CREATE_DEVICE_RECORD_HPP

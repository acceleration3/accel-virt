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

}

#endif

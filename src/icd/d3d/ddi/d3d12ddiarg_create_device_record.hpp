#ifndef AV_D3D_D3D12DDIARG_CREATE_DEVICE_RECORD_HPP
#define AV_D3D_D3D12DDIARG_CREATE_DEVICE_RECORD_HPP

#include <stdint.h>

namespace av::d3d {

struct D3d12ddiargCreateDeviceRecord {
    void       *hRTDevice;
    uint32_t    Interface;
    uint32_t    Version;
    const void *pKTCallbacks;
    void       *hDrvDevice;
    const void *p12UMCallbacks;
    uint32_t    Flags;
    void       *pReserveRanges;
    uint32_t    NumReserveRanges;
};

}

#endif

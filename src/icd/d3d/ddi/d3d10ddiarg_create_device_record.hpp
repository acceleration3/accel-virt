#ifndef AV_D3D_D3D10DDIARG_CREATE_DEVICE_RECORD_HPP
#define AV_D3D_D3D10DDIARG_CREATE_DEVICE_RECORD_HPP

#include "icd/d3d/ddi/dxgi_ddi_base_args_record.hpp"

#include <stdint.h>

namespace av::d3d {

struct D3d10ddiargCreateDeviceRecord {
    void                 *hRTDevice;
    uint32_t              Interface;
    uint32_t              Version;
    const void           *pKTCallbacks;
    void                 *pDeviceFuncs;
    void                 *hDrvDevice;
    DxgiDdiBaseArgsRecord DXGIBaseDDI;
    void                 *hRTCoreLayer;
    const void           *pUMCallbacks;
    uint32_t              Flags;
    void                 *ppfnRetrieveSubObject;
};

}

#endif

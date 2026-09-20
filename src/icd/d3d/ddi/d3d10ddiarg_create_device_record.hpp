/* D3D10DDIARG_CREATEDEVICE (d3d10umddi.h:7118), what the D3D10/11 runtime
 * hands pfnCreateDevice. Three of its members are unions of one pointer each -
 * the device-functions table, the core-layer callbacks, and (inside
 * DXGIBaseDDI) the DXGI functions table - and which arm is live is the
 * Interface the runtime asked with, so each is one pointer here.
 *
 * hDrvDevice is the block the runtime allocated at the size
 * pfnCalcPrivateDeviceSize answered: the driver writes a DeviceCookie into it
 * and constructs nothing there. A DDI record. */
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
    uint32_t              Padding;
    void                 *ppfnRetrieveSubObject;
};

}  // namespace av::d3d

#endif  // AV_D3D_D3D10DDIARG_CREATE_DEVICE_RECORD_HPP

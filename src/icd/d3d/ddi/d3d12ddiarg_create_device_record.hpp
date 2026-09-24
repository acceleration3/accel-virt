/* D3D12DDIARG_CREATEDEVICE_0109 (d3d12umddi.h:13618), what D3D12Core hands
 * pfnCreateDevice at the interface this driver answers (D3D12DDI_SUPPORTED_0110).
 * The core-layer callbacks are a union of pointers to the table's revisions,
 * one pointer here; the driver reads the 0050 revision, which holds the queue
 * callbacks it creates contexts through.
 *
 * hDrvDevice is the block the runtime allocated at the size
 * pfnCalcPrivateDeviceSize answered, as for Direct3D 11. A DDI record. */
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
    uint32_t    Padding;
    void       *pReserveRanges;
    uint32_t    NumReserveRanges;
    uint32_t    Padding2;
};

}  // namespace av::d3d

#endif  // AV_D3D_D3D12DDIARG_CREATE_DEVICE_RECORD_HPP

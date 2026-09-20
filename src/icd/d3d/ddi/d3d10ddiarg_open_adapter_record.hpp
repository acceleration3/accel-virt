/* D3D10DDIARG_OPENADAPTER (d3d10umddi.h:7637), what d3d11.dll hands
 * OpenAdapter10_2. hAdapter and the function table pAdapterFuncs points at are
 * the driver's to write; everything else is the runtime's. The handle types are
 * single-void* structures (D3D10DDI_H / D3D10DDI_HRT, d3d10umddi.h:142, 245),
 * so they are carried as void*. A DDI record. */
#ifndef AV_D3D_D3D10DDIARG_OPEN_ADAPTER_RECORD_HPP
#define AV_D3D_D3D10DDIARG_OPEN_ADAPTER_RECORD_HPP

#include <stdint.h>

namespace av::d3d {

struct D3d10ddiargOpenAdapterRecord {
    void       *hRTAdapter;
    void       *hAdapter;
    uint32_t    Interface;
    uint32_t    Version;
    const void *pAdapterCallbacks;
    void       *pAdapterFuncs;
};

}  // namespace av::d3d

#endif  // AV_D3D_D3D10DDIARG_OPEN_ADAPTER_RECORD_HPP

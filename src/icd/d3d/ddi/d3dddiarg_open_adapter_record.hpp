/* D3DDDIARG_OPENADAPTER (d3dumddi.h:4656), what d3d9.dll's OpenAdapter call
 * hands the driver: the runtime's adapter handle in hAdapter, replaced by the
 * driver's own on the way out. A DDI record. */
#ifndef AV_D3D_D3DDDIARG_OPEN_ADAPTER_RECORD_HPP
#define AV_D3D_D3DDDIARG_OPEN_ADAPTER_RECORD_HPP

#include <stdint.h>

namespace av::d3d {

struct D3dddiargOpenAdapterRecord {
    void       *hAdapter;
    uint32_t    Interface;
    uint32_t    Version;
    const void *pAdapterCallbacks;
    void       *pAdapterFuncs;
    uint32_t    DriverVersion;
};

}  // namespace av::d3d

#endif  // AV_D3D_D3DDDIARG_OPEN_ADAPTER_RECORD_HPP

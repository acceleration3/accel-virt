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

}

#endif

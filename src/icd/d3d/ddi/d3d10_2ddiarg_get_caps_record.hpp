#ifndef AV_D3D_D3D10_2DDIARG_GET_CAPS_RECORD_HPP
#define AV_D3D_D3D10_2DDIARG_GET_CAPS_RECORD_HPP

#include <stdint.h>

namespace av::d3d {

struct D3d102ddiargGetCapsRecord {
    uint32_t Type;
    void    *pInfo;
    void    *pData;
    uint32_t DataSize;
};

}

#endif

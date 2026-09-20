/* D3D10_2DDIARG_GETCAPS (d3d10umddi.h) and, field for field,
 * D3D12DDIARG_GETCAPS (d3d12umddi.h:2611). pData and DataSize are the
 * runtime's buffer and its length: nothing past DataSize is ever written.
 * A DDI record. */
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

}  // namespace av::d3d

#endif  // AV_D3D_D3D10_2DDIARG_GET_CAPS_RECORD_HPP

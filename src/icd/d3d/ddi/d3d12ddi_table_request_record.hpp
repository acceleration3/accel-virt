/* D3D12DDI_TABLE_REQUEST (d3d12umddi.h:2517), one row of what
 * pfnGetOptionalDDITables answers. A DDI record. */
#ifndef AV_D3D_D3D12DDI_TABLE_REQUEST_RECORD_HPP
#define AV_D3D_D3D12DDI_TABLE_REQUEST_RECORD_HPP

#include <stdint.h>

namespace av::d3d {

struct D3d12ddiTableRequestRecord {
    uint32_t tableType;
    uint32_t numTables;
};

}  // namespace av::d3d

#endif  // AV_D3D_D3D12DDI_TABLE_REQUEST_RECORD_HPP

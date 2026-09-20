/* D3D10DDIARG_CALCPRIVATEDEVICESIZE (d3d10umddi.h:7181) and, field for field,
 * D3D12DDIARG_CALCPRIVATEDEVICESIZE (d3d12umddi.h:2597): the runtime asks how
 * much of its own memory the driver wants for a device. A DDI record. */
#ifndef AV_D3D_D3D10DDIARG_CALC_PRIVATE_DEVICE_SIZE_RECORD_HPP
#define AV_D3D_D3D10DDIARG_CALC_PRIVATE_DEVICE_SIZE_RECORD_HPP

#include <stdint.h>

namespace av::d3d {

struct D3d10ddiargCalcPrivateDeviceSizeRecord {
    uint32_t Interface;
    uint32_t Version;
    uint32_t Flags;
};

}  // namespace av::d3d

#endif  // AV_D3D_D3D10DDIARG_CALC_PRIVATE_DEVICE_SIZE_RECORD_HPP

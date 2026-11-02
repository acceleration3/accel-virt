#ifndef AV_D3D_D3D10DDIARG_CALC_PRIVATE_DEVICE_SIZE_RECORD_HPP
#define AV_D3D_D3D10DDIARG_CALC_PRIVATE_DEVICE_SIZE_RECORD_HPP

#include <stdint.h>

namespace av::d3d {

struct D3d10ddiargCalcPrivateDeviceSizeRecord {
    uint32_t Interface;
    uint32_t Version;
    uint32_t Flags;
};

}

#endif

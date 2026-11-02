#ifndef AV_DXGK_DEVICE_DESCRIPTOR_RECORD_HPP
#define AV_DXGK_DEVICE_DESCRIPTOR_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkDeviceDescriptorRecord {
    uint32_t DescriptorOffset;
    uint32_t DescriptorLength;
    uint8_t  *DescriptorBuffer;
};

}

#endif

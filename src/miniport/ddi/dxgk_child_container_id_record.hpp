#ifndef AV_DXGK_CHILD_CONTAINER_ID_RECORD_HPP
#define AV_DXGK_CHILD_CONTAINER_ID_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkChildContainerIdRecord {
    uint8_t  ContainerId[16];
    uint64_t EldInfoPortId;
    uint16_t EldInfoManufacturerName;
    uint16_t EldInfoProductCode;
    uint32_t EldInfoPadding;
};

}

#endif

/* DXGK_CHILD_CONTAINER_ID, as DxgkDdiGetChildContainerId fills it. A DDI
 * record: the GUID is its sixteen bytes and EldInfo's fields are prefixed with
 * its name; src/km/ddi_record_layout.cpp proves each offset. */
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

}  // namespace av

#endif  // AV_DXGK_CHILD_CONTAINER_ID_RECORD_HPP

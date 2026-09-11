/* DXGK_NODEMETADATA (DXGKARG_GETNODEMETADATA), as DxgkDdiGetNodeMetadata fills
 * it. A DDI record: FriendlyName is UTF-16, and d3dkmdt.h declares it under
 * pack(1), so there is no tail padding; src/km/ddi_record_layout.cpp proves
 * each offset. */
#ifndef AV_DXGK_NODE_METADATA_RECORD_HPP
#define AV_DXGK_NODE_METADATA_RECORD_HPP

#include <stdint.h>

namespace av {

#pragma pack(push, 1)
struct DxgkNodeMetadataRecord {
    int32_t  EngineType;
    uint16_t FriendlyName[32];
    uint32_t Flags;
    uint8_t  GpuMmuSupported;
    uint8_t  IoMmuSupported;
};
#pragma pack(pop)

}  // namespace av

#endif  // AV_DXGK_NODE_METADATA_RECORD_HPP

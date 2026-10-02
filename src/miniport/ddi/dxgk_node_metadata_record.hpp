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

/* DXGK_NODEMETADATA_FLAGS (d3dkmdt.h:1970): what this node supports, and how
 * deep a hardware queue on it may be. Only the two this driver implements are
 * named - a node reporting more would be a capability nobody wrote. */
enum : uint32_t {
    node_context_scheduling_supported = 0x1u,
    node_user_mode_submission = 0x8u,
    node_in_flight_hw_queue_buffers_shift = 16u,
};
#pragma pack(pop)

}  // namespace av

#endif  // AV_DXGK_NODE_METADATA_RECORD_HPP

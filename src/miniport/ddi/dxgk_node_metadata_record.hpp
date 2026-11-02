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

enum : uint32_t {
    node_context_scheduling_supported = 0x1u,
    node_user_mode_submission = 0x8u,
    node_in_flight_hw_queue_buffers_shift = 16u,
};
#pragma pack(pop)

}

#endif

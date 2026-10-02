/* DXGKARG_RESUMEHWENGINE, as DxgkDdiResumeHwEngine receives it. A DDI record;
 * src/km/ddi_record_layout.cpp proves each offset. */
#ifndef AV_DXGKARG_RESUME_HW_ENGINE_RECORD_HPP
#define AV_DXGKARG_RESUME_HW_ENGINE_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargResumeHwEngineRecord {
    uint32_t NodeOrdinal;
    uint32_t EngineOrdinal;
};

}  // namespace av

#endif  // AV_DXGKARG_RESUME_HW_ENGINE_RECORD_HPP

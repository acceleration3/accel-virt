/* DXGKARG_CREATEPROCESS, as DxgkDdiCreateProcess receives and answers it. A DDI
 * record: Flags is DXGK_CREATEPROCESSFLAGS' UINT; src/km/ddi_record_layout.cpp
 * proves each offset. */
#ifndef AV_DXGKARG_CREATE_PROCESS_RECORD_HPP
#define AV_DXGKARG_CREATE_PROCESS_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargCreateProcessRecord {
    void           *hDxgkProcess;
    void           *hKmdProcess;
    uint32_t       Flags;
    uint32_t       NumPasid;
    const uint32_t *pPasid;
    void           *hKmdVmWorkerProcess;
    uint32_t       ProcessNameLength;
    uint32_t       ProcessNameLengthPadding;
    const uint16_t *pProcessName;
};

}  // namespace av

#endif  // AV_DXGKARG_CREATE_PROCESS_RECORD_HPP

/* DXGKARGCB_RELEASEHANDLEDATA, the argument of DxgkCbReleaseHandleData. A DDI
 * record passed by value, so its size is part of the ABI;
 * src/km/ddi_record_layout.cpp proves each offset. */
#ifndef AV_DXGKARGCB_RELEASE_HANDLE_DATA_RECORD_HPP
#define AV_DXGKARGCB_RELEASE_HANDLE_DATA_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargcbReleaseHandleDataRecord {
    void   *ReleaseHandle;
    int32_t Type;
    int32_t TypePadding;
};

}  // namespace av

#endif  // AV_DXGKARGCB_RELEASE_HANDLE_DATA_RECORD_HPP

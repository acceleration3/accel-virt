#ifndef AV_DXGKARGCB_RELEASE_HANDLE_DATA_RECORD_HPP
#define AV_DXGKARGCB_RELEASE_HANDLE_DATA_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargcbReleaseHandleDataRecord {
    void   *ReleaseHandle;
    int32_t Type;
    int32_t TypePadding;
};

}

#endif

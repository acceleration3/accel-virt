#ifndef AV_DXGKARGCB_GET_HANDLE_DATA_RECORD_HPP
#define AV_DXGKARGCB_GET_HANDLE_DATA_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargcbGetHandleDataRecord {
    uint32_t hObject;
    int32_t  Type;
    uint32_t Flags;
};

}

#endif

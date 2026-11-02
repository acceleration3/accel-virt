#ifndef AV_DXGKARG_ESCAPE_RECORD_HPP
#define AV_DXGKARG_ESCAPE_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargEscapeRecord {
    void    *hDevice;
    uint32_t Flags;
    uint32_t FlagsPadding;
    void    *pPrivateDriverData;
    uint32_t PrivateDriverDataSize;
    uint32_t PrivateDriverDataSizePadding;
    void    *hContext;
    void    *hKmdProcessHandle;
};

}

#endif

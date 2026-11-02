#ifndef AV_DXGK_PTE_RECORD_HPP
#define AV_DXGK_PTE_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkPteRecord {
    uint64_t Flags;
    uint64_t PageAddress;
};

}

#endif

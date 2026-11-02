#ifndef AV_LINKED_DEVICE_RECORD_HPP
#define AV_LINKED_DEVICE_RECORD_HPP

#include <stdint.h>

namespace av {

struct LinkedDeviceRecord {
    uint32_t ChainUid;
    uint32_t NumberOfLinksInChain;
    uint8_t  LeadLink;
    uint8_t  Padding[3];
};

}

#endif

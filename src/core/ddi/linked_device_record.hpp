/* LINKED_DEVICE, as DxgkDdiLinkDevice fills it. A DDI record;
 * src/km/ddi_record_layout.cpp proves each offset. */
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

}  // namespace av

#endif  // AV_LINKED_DEVICE_RECORD_HPP

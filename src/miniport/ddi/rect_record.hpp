/* RECT, as the present DDIs carry rectangles. A DDI record: fields keep the
 * SDK's lowercase names; src/km/ddi_record_layout.cpp proves each offset. */
#ifndef AV_RECT_RECORD_HPP
#define AV_RECT_RECORD_HPP

#include <stdint.h>

namespace av {

struct RectRecord {
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
};

}  // namespace av

#endif  // AV_RECT_RECORD_HPP

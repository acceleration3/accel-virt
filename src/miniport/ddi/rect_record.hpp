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

}

#endif

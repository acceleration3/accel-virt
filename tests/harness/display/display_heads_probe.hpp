#ifndef AV_DISPLAY_HEADS_PROBE_HPP
#define AV_DISPLAY_HEADS_PROBE_HPP

#include "device/display/display_heads.hpp"

#include <stdint.h>

namespace av {

struct DisplayHeadsProbe {
    explicit DisplayHeadsProbe(DisplayHeads &target);

    void renumber_head(uint32_t head, uint32_t number);
    void unpicture_head(uint32_t head);

    DisplayHeads &heads;
};

}

#endif

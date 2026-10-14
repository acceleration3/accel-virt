/* Reaches into DisplayHeads so invariant tests can corrupt it. Declared a
 * friend by DisplayHeads; nothing outside the tests uses it. */
#ifndef AV_DISPLAY_HEADS_PROBE_HPP
#define AV_DISPLAY_HEADS_PROBE_HPP

#include "device/display/display_heads.hpp"

#include <stdint.h>

namespace av {

struct DisplayHeadsProbe {
    explicit DisplayHeadsProbe(DisplayHeads &target);

    /* A binding that thinks it is another head than the slot it is in. */
    void renumber_head(uint32_t head, uint32_t number);
    /* A binding that shows with no framebuffer: one head out of step. */
    void unpicture_head(uint32_t head);

    DisplayHeads &heads;
};

}  // namespace av

#endif  // AV_DISPLAY_HEADS_PROBE_HPP

/* Reaches into DisplayHeads so invariant tests can corrupt it. Declared a
 * friend by DisplayHeads; nothing outside the tests uses it. */
#ifndef AV_DISPLAY_HEADS_PROBE_HPP
#define AV_DISPLAY_HEADS_PROBE_HPP

#include "device/display/display_heads.hpp"

#include <stdint.h>

namespace av {

struct DisplayHeadsProbe {
    explicit DisplayHeadsProbe(DisplayHeads &target);

    /* The display's answer, unasked again: a head showing something on a
     * device that never agreed to show one is the state the invariant exists
     * to catch. */
    void forget_the_display();
    void remember_the_display();
    /* A binding that thinks it is another head than the slot it is in. */
    void renumber_head(uint32_t head, uint32_t number);

    DisplayHeads &heads;
};

}  // namespace av

#endif  // AV_DISPLAY_HEADS_PROBE_HPP

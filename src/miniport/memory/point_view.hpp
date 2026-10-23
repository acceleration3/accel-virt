/* What a present reads of the points named for its source allocation, taken
 * under the device table's lock (DeviceTable::peek_point). */
#ifndef AV_POINT_VIEW_HPP
#define AV_POINT_VIEW_HPP

#include <stdint.h>

namespace av {

struct PointView {
    /* A point was ever named for the allocation: its presents are points'. */
    bool     pointed = false;
    /* A point is waiting for this present; `point` is the newest, 0 for the
     * process saying this present reads the allocation's pages. */
    bool     queued = false;
    uint32_t point = 0u;
    /* The allocation is pointed and this present has none, the first time. */
    bool     first_starvation = false;
};

}  // namespace av

#endif  // AV_POINT_VIEW_HPP

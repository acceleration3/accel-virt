#include "framework.h"

namespace samples {

Sample *make_triangle();
Sample *make_pushconstants();
Sample *make_gears();
Sample *make_texture();

namespace {
// Made at the first call: the guest build has no C runtime to run static
// constructors, so a sample object at namespace scope would have no vtable.
Sample  *all[64];
uint32_t made = 0;
}  // namespace

Sample **registry(uint32_t &count)
{
    if (made == 0) {
        Sample *(*const makers[])() = {make_triangle, make_pushconstants, make_gears, make_texture};
        for (auto maker : makers) all[made++] = maker();
    }
    count = made;
    return all;
}

}  // namespace samples

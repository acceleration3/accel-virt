#include "framework.h"

namespace samples {

void register_compute(Sample **all, uint32_t &made);
void register_linear(Sample **all, uint32_t &made);
void register_image(Sample **all, uint32_t &made);
void register_transfer(Sample **all, uint32_t &made);

namespace {
Sample  *all[64];
uint32_t made = 0;
}

Sample **registry(uint32_t &count)
{
    if (made == 0) {
        register_compute(all, made);
        register_linear(all, made);
        register_image(all, made);
        register_transfer(all, made);
    }
    count = made;
    return all;
}

}

#include "harness/display/display_heads_probe.hpp"

#include "device/display/scanout_binding.hpp"

namespace av {

DisplayHeadsProbe::DisplayHeadsProbe(DisplayHeads &target) : heads(target) {}

void DisplayHeadsProbe::forget_the_display() { heads.display_ = DisplayHeads::Display::refuses_dmabuf; }

void DisplayHeadsProbe::remember_the_display() { heads.display_ = DisplayHeads::Display::takes_dmabuf; }

void DisplayHeadsProbe::renumber_head(uint32_t head, uint32_t number)
{
    heads.heads_[head].object().head_ = number;
}

}  // namespace av

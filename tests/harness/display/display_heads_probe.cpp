#include "harness/display/display_heads_probe.hpp"

#include "device/display/scanout_binding.hpp"

namespace av {

DisplayHeadsProbe::DisplayHeadsProbe(DisplayHeads &target) : heads(target) {}

void DisplayHeadsProbe::unpicture_head(uint32_t head) { heads.heads_[head].object().framebuffer_.forget(); }

void DisplayHeadsProbe::renumber_head(uint32_t head, uint32_t number)
{
    heads.heads_[head].object().head_ = number;
}

}  // namespace av

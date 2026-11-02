#include "harness/display/scanout_binding_probe.hpp"

namespace av {

ScanoutBindingProbe::ScanoutBindingProbe(ScanoutBinding &target) : binding(target) {}

void ScanoutBindingProbe::set_shown(bool shown) { binding.shown_ = shown; }

void ScanoutBindingProbe::set_blob(uint32_t blob) { binding.blob_ = blob; }

void ScanoutBindingProbe::forget_framebuffer() { binding.framebuffer_.forget(); }

}

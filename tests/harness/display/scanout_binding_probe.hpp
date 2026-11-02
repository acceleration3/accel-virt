#ifndef AV_SCANOUT_BINDING_PROBE_HPP
#define AV_SCANOUT_BINDING_PROBE_HPP

#include "device/display/scanout_binding.hpp"

#include <stdint.h>

namespace av {

struct ScanoutBindingProbe {
    explicit ScanoutBindingProbe(ScanoutBinding &target);

    void set_shown(bool shown);
    void set_blob(uint32_t blob);
    void forget_framebuffer();

    ScanoutBinding &binding;
};

}

#endif

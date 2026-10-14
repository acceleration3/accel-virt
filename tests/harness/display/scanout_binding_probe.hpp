/* Reaches into a ScanoutBinding so invariant tests can corrupt it: each of the
 * two things a head keeps in step with showing - a framebuffer, and the blob
 * it names - can be broken on its own here, and on its own it fails. Declared
 * a friend by ScanoutBinding; nothing outside the tests uses it. */
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

}  // namespace av

#endif  // AV_SCANOUT_BINDING_PROBE_HPP

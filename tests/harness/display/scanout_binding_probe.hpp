/* Reaches into a ScanoutBinding so invariant tests can corrupt it: each of the
 * three things a head keeps in step - that it shows a framebuffer, that it
 * names a blob, and that it holds the descriptor it is showing - can be broken
 * on its own here, and on its own it fails. Declared a friend by
 * ScanoutBinding; nothing outside the tests uses it. */
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
    /* The descriptor the head is showing becomes the spare: a head holding the
     * wrong one of its two slots. */
    void swap_slots();

    ScanoutBinding &binding;
};

}  // namespace av

#endif  // AV_SCANOUT_BINDING_PROBE_HPP

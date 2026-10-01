/* The pointer the OpenGL ICD's entry points and hooks reach its root through
 * (RootBinding), defined beside the hooks in gl_hooks.cpp. */
#ifndef AV_GLICD_GL_ICD_BINDING_HPP
#define AV_GLICD_GL_ICD_BINDING_HPP

#include "icd/common/root_binding.hpp"

namespace av::glicd {

class GlIcdRoot;

using GlIcdBinding = icd::RootBinding<GlIcdRoot>;

extern GlIcdBinding gl_icd_binding;

}  // namespace av::glicd

#endif  // AV_GLICD_GL_ICD_BINDING_HPP

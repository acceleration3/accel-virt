#ifndef AV_GLICD_GL_ICD_BINDING_HPP
#define AV_GLICD_GL_ICD_BINDING_HPP

#include "icd/common/root_binding.hpp"

namespace av::glicd {

class GlIcdRoot;

using GlIcdBinding = icd::RootBinding<GlIcdRoot>;

extern GlIcdBinding gl_icd_binding;

}

#endif

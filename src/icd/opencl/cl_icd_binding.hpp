#ifndef AV_CLICD_CL_ICD_BINDING_HPP
#define AV_CLICD_CL_ICD_BINDING_HPP

#include "icd/common/root_binding.hpp"

namespace av::clicd {

class ClIcdRoot;

using ClIcdBinding = icd::RootBinding<ClIcdRoot>;

extern ClIcdBinding cl_icd_binding;

}

#endif

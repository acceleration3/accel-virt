/* The pointer the OpenCL ICD's entry points reach its root through
 * (RootBinding), defined beside the hooks in cl_hooks.cpp. */
#ifndef AV_CLICD_CL_ICD_BINDING_HPP
#define AV_CLICD_CL_ICD_BINDING_HPP

#include "icd/common/root_binding.hpp"

namespace av::clicd {

class ClIcdRoot;

using ClIcdBinding = icd::RootBinding<ClIcdRoot>;

extern ClIcdBinding cl_icd_binding;

}  // namespace av::clicd

#endif  // AV_CLICD_CL_ICD_BINDING_HPP

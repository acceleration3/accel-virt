/* The pointer the Vulkan ICD's entry points and hooks reach its root through
 * (RootBinding), defined beside the hooks in icd_hooks.cpp. */
#ifndef AV_ICD_BINDING_HPP
#define AV_ICD_BINDING_HPP

#include "icd/common/root_binding.hpp"

namespace av::icd {

class IcdRoot;

using IcdBinding = RootBinding<IcdRoot>;

extern IcdBinding icd_binding;

}  // namespace av::icd

#endif  // AV_ICD_BINDING_HPP

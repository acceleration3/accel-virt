#ifndef AV_ICD_BINDING_HPP
#define AV_ICD_BINDING_HPP

#include "icd/common/root_binding.hpp"

namespace av::icd {

class IcdRoot;

using IcdBinding = RootBinding<IcdRoot>;

extern IcdBinding icd_binding;

}

#endif

#include "icd/vulkan/icd_binding.hpp"

namespace av::icd {

void     IcdBinding::bind(IcdRoot &root) { root_ = &root; }
void     IcdBinding::unbind() { root_ = nullptr; }
bool     IcdBinding::bound() const { return root_ != nullptr; }
IcdRoot &IcdBinding::root() const { return *root_; }

}  // namespace av::icd

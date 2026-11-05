#include "harness/executor/scripted_slice_importer.hpp"

#include "executor/owned_fd.hpp"

namespace av {

ScriptedSliceImporter::ScriptedSliceImporter()
    : refusing_(false), imports_(0u), last_bytes_(0u), last_descriptor_held_(false)
{
}

bool ScriptedSliceImporter::import_to_draw(OwnedFd &descriptor, std::uint64_t bytes, VulkanBuffer &)
{
    imports_++;
    last_bytes_ = bytes;
    last_descriptor_held_ = descriptor.held();
    return !refusing_;
}

void ScriptedSliceImporter::refuse(bool refusing) { refusing_ = refusing; }

std::uint32_t ScriptedSliceImporter::imports() const { return imports_; }
std::uint64_t ScriptedSliceImporter::last_bytes() const { return last_bytes_; }
bool          ScriptedSliceImporter::last_descriptor_held() const { return last_descriptor_held_; }

}

#include "icd/vulkan/host_extension_list.hpp"

#include "core/trace/function_scope.hpp"

namespace av::icd {

namespace {

/* What this ICD implements over the host rather than through it: the two
 * instance extensions a window needs, and the swapchain that presents on it. */
const char *const icd_extensions[] = {"VK_KHR_surface", "VK_KHR_win32_surface", "VK_KHR_swapchain"};
constexpr uint32_t icd_extension_count = sizeof icd_extensions / sizeof icd_extensions[0];

}  // namespace

HostExtensionList::HostExtensionList() : names_{}, count_(0u), given_(0u), kept_(0u)
{
    AV_SCOPE("HostExtensionList::HostExtensionList");
}

Status HostExtensionList::take(const char *const *names, uint32_t count)
{
    AV_SCOPE("HostExtensionList::take");
    if (count > max_names) return Status::too_large;
    if (names == nullptr && count != 0u) return Status::invalid_parameter;
    count_ = 0u;
    kept_ = 0u;
    given_ = count;
    for (uint32_t index = 0u; index < count; index++) {
        const char *const name = names[index];
        /* A name the application did not give is not one to pass on; what
         * answers for it is the caller's refusal, not a null dereference. */
        if (name == nullptr) return Status::invalid_parameter;
        if (is_the_icd_s(name)) {
            kept_++;
            continue;
        }
        names_[count_++] = name;
    }
    return Status::ok;
}

const char *const *HostExtensionList::names() const
{
    AV_SCOPE("HostExtensionList::names");
    return count_ != 0u ? names_ : nullptr;
}

uint32_t HostExtensionList::count() const
{
    AV_SCOPE("HostExtensionList::count");
    return count_;
}

uint32_t HostExtensionList::kept() const
{
    AV_SCOPE("HostExtensionList::kept");
    return kept_;
}

bool HostExtensionList::is_the_icd_s(const char *name) const
{
    AV_SCOPE("HostExtensionList::is_the_icd_s");
    for (uint32_t index = 0u; index < icd_extension_count; index++)
        if (__builtin_strcmp(name, icd_extensions[index]) == 0) return true;
    return false;
}

bool HostExtensionList::invariants_hold() const
{
    AV_SCOPE("HostExtensionList::invariants_hold");
    /* Every name it holds came from the list it was given, and the ones it
     * kept for this ICD are the rest of that list. `take` refuses a longer one
     * before it writes anything, so there is no room left to overrun. */
    return count_ + kept_ <= given_;
}

}  // namespace av::icd

#include "icd/vulkan/host_extension_list.hpp"

#include "core/trace/function_scope.hpp"

namespace av::icd {

namespace {

/* What this ICD implements over the host rather than through it: the two
 * instance extensions a window needs, and the swapchain that presents on it. */
const char *const icd_extensions[] = {"VK_KHR_surface", "VK_KHR_win32_surface", "VK_KHR_swapchain"};
constexpr uint32_t icd_extension_count = sizeof icd_extensions / sizeof icd_extensions[0];

/* What the host is asked for in place of an extension this ICD answers over a
 * different host mechanism: a Win32 external object is a host object shared by
 * fd, so the host's device is made with the fd extension the ICD will call
 * (feature 047, research.md §10). */
struct HostSpelling {
    const char *asked_;
    const char *host_;
};
constexpr HostSpelling host_spellings[] = {
    {"VK_KHR_external_memory_win32", "VK_KHR_external_memory_fd"},
    {"VK_KHR_external_semaphore_win32", "VK_KHR_external_semaphore_fd"},
};
constexpr uint32_t host_spelling_count = sizeof host_spellings / sizeof host_spellings[0];

}  // namespace

HostExtensionList::HostExtensionList(BlockSource &memory)
    : names_(memory, Pool::paged), given_names_(nullptr), given_(0u), kept_(0u), added_(0u)
{
    AV_SCOPE("HostExtensionList::HostExtensionList");
}

Status HostExtensionList::take(const char *const *names, uint32_t count)
{
    AV_SCOPE("HostExtensionList::take");
    names_.clear();
    given_names_ = nullptr;
    given_ = 0u;
    kept_ = 0u;
    added_ = 0u;
    if (names == nullptr && count != 0u) return Status::invalid_parameter;
    /* A name the application did not give is not one to pass on; what
     * answers for it is the caller's refusal, not a null dereference. */
    for (uint32_t index = 0u; index < count; index++)
        if (names[index] == nullptr) return Status::invalid_parameter;
    if (count == ~uint32_t{0} || names_.reserve(count + 1u) != Status::ok) return Status::no_memory;
    given_names_ = names;
    given_ = count;
    for (uint32_t index = 0u; index < count; index++) {
        if (is_the_icd_s(names[index])) {
            kept_++;
            continue;
        }
        /* Room was made, and an empty name is the loader's to refuse. */
        static_cast<void>(names_.add(host_spelling_of(names[index])));
    }
    return Status::ok;
}

Status HostExtensionList::add(const char *name)
{
    AV_SCOPE("HostExtensionList::add");
    if (names_.holds(name)) return Status::ok;
    const Status added = names_.add(name);
    if (added == Status::ok) added_++;
    return added;
}

bool HostExtensionList::named(const char *name) const
{
    AV_SCOPE("HostExtensionList::named");
    for (uint32_t index = 0u; index < given_; index++)
        if (__builtin_strcmp(given_names_[index], name) == 0) return true;
    return false;
}

const char *const *HostExtensionList::names() const
{
    AV_SCOPE("HostExtensionList::names");
    return names_.names();
}

uint32_t HostExtensionList::count() const
{
    AV_SCOPE("HostExtensionList::count");
    return names_.count();
}

uint32_t HostExtensionList::kept() const
{
    AV_SCOPE("HostExtensionList::kept");
    return kept_;
}

const char *HostExtensionList::host_spelling_of(const char *name) const
{
    AV_SCOPE("HostExtensionList::host_spelling_of");
    for (uint32_t index = 0u; index < host_spelling_count; index++)
        if (__builtin_strcmp(name, host_spellings[index].asked_) == 0) return host_spellings[index].host_;
    return name;
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
    /* Every name it holds came from the list it was given or was added, and
     * the ones it kept for this ICD are the rest of that list. */
    return names_.invariants_hold() && names_.count() + kept_ <= given_ + added_;
}

}  // namespace av::icd

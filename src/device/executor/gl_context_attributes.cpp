#include "device/executor/gl_context_attributes.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

/* EGL 1.5's names, written here so the list can be judged without EGL. */
constexpr int32_t egl_none = 0x3038;
constexpr int32_t egl_true = 1;
constexpr int32_t context_major_version = 0x3098;
constexpr int32_t context_minor_version = 0x30FB;
constexpr int32_t context_profile_mask = 0x30FD;
constexpr int32_t context_debug = 0x31B0;
constexpr int32_t context_forward_compatible = 0x31B1;
constexpr int32_t context_robust_access = 0x31B2;
constexpr int32_t context_reset_strategy = 0x31BD;
constexpr int32_t lose_context_on_reset = 0x31BF;

/* The highest minor each major version of OpenGL has, from 1 to 4. */
constexpr uint8_t highest_minor[] = {5u, 1u, 3u, 6u};
constexpr uint8_t highest_major = sizeof highest_minor / sizeof highest_minor[0];
constexpr uint8_t defined_flags = RVK_CONTEXT_FLAG_DEBUG | RVK_CONTEXT_FLAG_FORWARD_COMPATIBLE |
                                  RVK_CONTEXT_FLAG_ROBUST_ACCESS | RVK_CONTEXT_FLAG_LOSE_CONTEXT_ON_RESET;

}  // namespace

GlContextAttributes::GlContextAttributes(const RvkContextOptions *options)
    : options_(options != nullptr ? *options : RvkContextOptions{}), list_{}, used_(0u), valid_(acceptable())
{
    AV_SCOPE("GlContextAttributes::GlContextAttributes");
    list_[0] = egl_none;
    if (!valid_ || options_.major == 0u) return;
    add(context_major_version, options_.major);
    add(context_minor_version, options_.minor);
    if (options_.profile != 0u) add(context_profile_mask, options_.profile);
    if ((options_.flags & RVK_CONTEXT_FLAG_DEBUG) != 0u) add(context_debug, egl_true);
    if ((options_.flags & RVK_CONTEXT_FLAG_FORWARD_COMPATIBLE) != 0u) add(context_forward_compatible, egl_true);
    if ((options_.flags & RVK_CONTEXT_FLAG_ROBUST_ACCESS) != 0u) add(context_robust_access, egl_true);
    if ((options_.flags & RVK_CONTEXT_FLAG_LOSE_CONTEXT_ON_RESET) != 0u)
        add(context_reset_strategy, lose_context_on_reset);
}

bool GlContextAttributes::acceptable() const
{
    AV_SCOPE("GlContextAttributes::acceptable");
    if (options_.reserved != 0u) return false;
    if (options_.major == 0u) return options_.minor == 0u && options_.profile == 0u && options_.flags == 0u;
    return options_.major <= highest_major && options_.minor <= highest_minor[options_.major - 1u] &&
           options_.profile <= RVK_CONTEXT_PROFILE_COMPATIBILITY && (options_.flags & ~defined_flags) == 0u;
}

void GlContextAttributes::add(int32_t name, int32_t value)
{
    AV_SCOPE("GlContextAttributes::add");
    list_[used_++] = name;
    list_[used_++] = value;
    list_[used_] = egl_none;
}

bool           GlContextAttributes::valid() const
{
    AV_SCOPE("GlContextAttributes::valid");
    return valid_;
}
const int32_t *GlContextAttributes::list() const
{
    AV_SCOPE("GlContextAttributes::list");
    return list_;
}

}  // namespace av

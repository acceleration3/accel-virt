#include "icd/opengl/context_attributes.hpp"

#include "core/trace/function_scope.hpp"

namespace av::glicd {

namespace {

enum Name : uint32_t { at_major, at_minor, at_layer, at_flags, at_profile, at_reset };

constexpr int32_t known_names[] = {ContextAttributes::major_version, ContextAttributes::minor_version,
                                   ContextAttributes::layer_plane,   ContextAttributes::context_flags,
                                   ContextAttributes::profile_mask,  ContextAttributes::reset_strategy};
constexpr int32_t defined_flags = ContextAttributes::flag_debug | ContextAttributes::flag_forward_compatible |
                                  ContextAttributes::flag_robust_access | ContextAttributes::flag_reset_isolation;
constexpr int32_t last_minor[] = {0, 5, 1, 3, 6};
constexpr int32_t last_major = 4;
static_assert(sizeof known_names / sizeof known_names[0] < ContextAttributes::max_pairs);

bool has(uint32_t seen, Name name)
{
    AV_SCOPE("has");
    return (seen & (1u << name)) != 0u;
}

}

ContextAttributes::ContextAttributes(const int32_t *list) : list_(list), seen_(0u), values_{}, accepted_(false)
{
    AV_SCOPE("ContextAttributes::ContextAttributes");
}

uint32_t ContextAttributes::read_pairs()
{
    AV_SCOPE("ContextAttributes::read_pairs");
    if (list_ == nullptr) return 0u;
    for (uint32_t at = 0u;; at += 2u) {
        if (list_[at] == 0) return 0u;
        uint32_t name = 0u;
        while (name < names && known_names[name] != list_[at]) name++;
        if (name == names || (seen_ & (1u << name)) != 0u) return error_invalid_parameter;
        seen_ |= 1u << name;
        values_[name] = list_[at + 1u];
    }
}

uint32_t ContextAttributes::judge() const
{
    AV_SCOPE("ContextAttributes::judge");
    const int32_t reset = values_[at_reset];
    if (values_[at_layer] != 0) return error_invalid_parameter;
    if (has(seen_, at_reset) && reset != no_reset_notification && reset != lose_context_on_reset)
        return error_invalid_parameter;
    const int32_t flags = values_[at_flags];
    if ((flags & ~defined_flags) != 0) return error_invalid_version;
    const bool    versioned = has(seen_, at_major) || has(seen_, at_minor);
    const int32_t major = has(seen_, at_major) ? values_[at_major] : 1;
    const int32_t minor = values_[at_minor];
    if (major < 1 || major > last_major || minor < 0 || minor > last_minor[major]) return error_invalid_version;
    if ((flags & flag_forward_compatible) != 0 && major < 3) return error_invalid_version;
    const bool profiled = versioned && (major > 3 || (major == 3 && minor >= 2));
    const int32_t mask = values_[at_profile];
    if (profiled && has(seen_, at_profile) && mask != profile_core && mask != profile_compatibility)
        return error_invalid_profile;
    return 0u;
}

uint32_t ContextAttributes::decode()
{
    AV_SCOPE("ContextAttributes::decode");
    seen_ = 0u;
    for (int32_t &value : values_) value = 0;
    uint32_t error = read_pairs();
    if (error == 0u) error = judge();
    accepted_ = error == 0u;
    return error;
}

GlContextOptions ContextAttributes::options() const
{
    AV_SCOPE("ContextAttributes::options");
    if (!accepted_) return GlContextOptions();
    const int32_t flags = values_[at_flags];
    uint8_t       option_flags = 0u;
    if ((flags & flag_debug) != 0) option_flags |= GlContextOptions::debug;
    if ((flags & flag_forward_compatible) != 0) option_flags |= GlContextOptions::forward_compatible;
    if ((flags & flag_robust_access) != 0) option_flags |= GlContextOptions::robust_access;
    if (values_[at_reset] == lose_context_on_reset) option_flags |= GlContextOptions::lose_context_on_reset;
    const bool versioned = has(seen_, at_major) || has(seen_, at_minor);
    if (!versioned && option_flags == 0u) return GlContextOptions();
    const int32_t major = has(seen_, at_major) ? values_[at_major] : 1;
    const int32_t minor = values_[at_minor];
    GlContextOptions::Profile profile = GlContextOptions::Profile::none;
    if (versioned && (major > 3 || (major == 3 && minor >= 2))) {
        const bool compatibility = has(seen_, at_profile) && values_[at_profile] == profile_compatibility;
        profile = compatibility ? GlContextOptions::Profile::compatibility : GlContextOptions::Profile::core;
    }
    return GlContextOptions(static_cast<uint8_t>(major), static_cast<uint8_t>(minor), profile, option_flags, 0u);
}

bool ContextAttributes::invariants_hold() const
{
    AV_SCOPE("ContextAttributes::invariants_hold");
    if ((seen_ >> names) != 0u) return false;
    for (uint32_t name = 0u; name < names; name++)
        if (!has(seen_, static_cast<Name>(name)) && values_[name] != 0) return false;
    return !accepted_ || judge() == 0u;
}

}

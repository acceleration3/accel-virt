#include "icd/opengl/granted_profile.hpp"

#include "core/trace/function_scope.hpp"

namespace av::glicd {

GrantedProfile::GrantedProfile() : major_(0u), minor_(0u), mask_(0u), rung_(Rung::none)
{
    AV_SCOPE("GrantedProfile::GrantedProfile");
}

void GrantedProfile::granted(Rung rung)
{
    AV_SCOPE("GrantedProfile::granted");
    rung_ = rung;
}

void GrantedProfile::learn(uint32_t major, uint32_t minor, uint32_t mask)
{
    AV_SCOPE("GrantedProfile::learn");
    major_ = major;
    minor_ = minor;
    mask_ = mask == core_bit || mask == compatibility_bit ? mask : 0u;
}

GrantedProfile::Rung GrantedProfile::rung() const
{
    AV_SCOPE("GrantedProfile::rung");
    return rung_;
}

uint32_t GrantedProfile::mask() const
{
    AV_SCOPE("GrantedProfile::mask");
    return mask_;
}

uint32_t GrantedProfile::major() const
{
    AV_SCOPE("GrantedProfile::major");
    return major_;
}

uint32_t GrantedProfile::minor() const
{
    AV_SCOPE("GrantedProfile::minor");
    return minor_;
}

bool GrantedProfile::known() const
{
    AV_SCOPE("GrantedProfile::known");
    return major_ != 0u;
}

bool GrantedProfile::legacy_calls_live() const
{
    AV_SCOPE("GrantedProfile::legacy_calls_live");
    if (mask_ == compatibility_bit) return true;
    return mask_ == 0u && known() && major_ < profiles_from_major;
}

bool GrantedProfile::invariants_hold() const
{
    AV_SCOPE("GrantedProfile::invariants_hold");
    return mask_ == 0u || mask_ == core_bit || mask_ == compatibility_bit;
}

}

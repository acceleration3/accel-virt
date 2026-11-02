#ifndef AV_GLICD_GRANTED_PROFILE_HPP
#define AV_GLICD_GRANTED_PROFILE_HPP

#include <stdint.h>

namespace av::glicd {

struct GrantedProfileProbe;

class GrantedProfile {
public:
    enum class Rung : uint8_t { none, compatibility_46, host_default, core_46 };

    enum : uint32_t { core_bit = 0x1u, compatibility_bit = 0x2u };
    enum : uint32_t { mask_query = 0x9126u, major_query = 0x821Bu, minor_query = 0x821Cu };
    static constexpr uint32_t profiles_from_major = 3u;

    GrantedProfile();

    void     granted(Rung rung);
    void     learn(uint32_t major, uint32_t minor, uint32_t mask);

    Rung     rung() const;
    uint32_t mask() const;
    uint32_t major() const;
    uint32_t minor() const;
    bool     known() const;
    bool     legacy_calls_live() const;

    bool invariants_hold() const;

private:
    uint32_t major_;
    uint32_t minor_;
    uint32_t mask_;
    Rung     rung_;

    friend struct GrantedProfileProbe;
};

}

#endif

/* What the host granted one context, as opposed to what was asked for (051).
 *
 * A legacy wglCreateContext asks for a 4.6 compatibility context, then for the
 * driver's own default, then for core - and what comes back is read from
 * GL_CONTEXT_PROFILE_MASK, never from GL_VERSION, because NVIDIA's
 * compatibility string is identical to its core one (research.md §1). The
 * proc table's legacy entry points forward only where this says they are
 * there, so a mask no profile defines and a host that answered nothing both
 * mean the same thing as core: refuse (constitution VI). */
#ifndef AV_GLICD_GRANTED_PROFILE_HPP
#define AV_GLICD_GRANTED_PROFILE_HPP

#include <stdint.h>

namespace av::glicd {

struct GrantedProfileProbe;

class GrantedProfile {
public:
    /* Which ask the host answered (research.md §2). */
    enum class Rung : uint8_t { none, compatibility_46, host_default, core_46 };

    /* GL_CONTEXT_PROFILE_MASK's two bits; anything else is no profile. */
    enum : uint32_t { core_bit = 0x1u, compatibility_bit = 0x2u };
    /* GL_CONTEXT_PROFILE_MASK, GL_MAJOR_VERSION, GL_MINOR_VERSION. */
    enum : uint32_t { mask_query = 0x9126u, major_query = 0x821Bu, minor_query = 0x821Cu };
    /* The release where profiles began: before it every context has the fixed
     * function pipeline, after it a context without a profile is core. */
    static constexpr uint32_t profiles_from_major = 3u;

    GrantedProfile();

    /* Which ask is being answered; set before the host is asked. */
    void     granted(Rung rung);
    /* What the host answered. A mask that is neither bit, or both, is kept as
     * no profile - the host is not this driver, and what it sends is bytes. */
    void     learn(uint32_t major, uint32_t minor, uint32_t mask);

    Rung     rung() const;
    uint32_t mask() const;
    uint32_t major() const;
    uint32_t minor() const;
    /* Whether the host answered a version at all. */
    bool     known() const;
    /* Whether this context has the fixed-function pipeline, the matrix stacks
     * and the display lists: a compatibility profile, or a context made before
     * profiles existed. */
    bool     legacy_calls_live() const;

    bool invariants_hold() const;

private:
    uint32_t major_;
    uint32_t minor_;
    uint32_t mask_;
    Rung     rung_;

    friend struct GrantedProfileProbe;
};

}  // namespace av::glicd

#endif  // AV_GLICD_GRANTED_PROFILE_HPP

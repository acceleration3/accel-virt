/* One of the process's AV_ environment variables, as a guest driver reads it:
 * copied once, when it is made, and judged by SettingText's rules - the ones
 * the host's EnvSetting judges by, so a switch set to "0" is off on both
 * sides of the guest boundary (docs/testing.md, "Knobs"). A value longer than
 * the copy holds is read as not set rather than cut. */
#ifndef AV_UM_GUEST_SETTING_HPP
#define AV_UM_GUEST_SETTING_HPP

#include "core/support/setting_text.hpp"

namespace av {

class GuestSetting {
public:
    explicit GuestSetting(const char *name);
    GuestSetting(const GuestSetting &) = delete;
    GuestSetting &operator=(const GuestSetting &) = delete;

    /* What it was set to, by SettingText's rules; not set when it was not. */
    SettingText value() const;

private:
    static constexpr unsigned long value_bytes = 64u;

    char value_[value_bytes];
    bool set_;
};

}  // namespace av

#endif  // AV_UM_GUEST_SETTING_HPP

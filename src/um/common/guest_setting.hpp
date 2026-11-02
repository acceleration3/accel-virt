#ifndef AV_UM_GUEST_SETTING_HPP
#define AV_UM_GUEST_SETTING_HPP

#include "core/support/setting_text.hpp"

namespace av {

class GuestSetting {
public:
    explicit GuestSetting(const char *name);
    GuestSetting(const GuestSetting &) = delete;
    GuestSetting &operator=(const GuestSetting &) = delete;

    SettingText value() const;

private:
    static constexpr unsigned long value_bytes = 64u;

    char value_[value_bytes];
    bool set_;
};

}

#endif

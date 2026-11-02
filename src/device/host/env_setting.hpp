#ifndef AV_ENV_SETTING_HPP
#define AV_ENV_SETTING_HPP

#include "core/support/setting_text.hpp"

#include <stdint.h>

namespace av {

class EnvSetting {
public:
    explicit EnvSetting(const char *name);

    bool        on() const;
    const char *text() const;
    [[nodiscard]] bool number(uint64_t &value) const;

private:
    SettingText value_;
};

}

#endif

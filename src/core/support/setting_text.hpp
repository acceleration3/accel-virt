#ifndef AV_SETTING_TEXT_HPP
#define AV_SETTING_TEXT_HPP

#include <stdint.h>

namespace av {

class SettingText {
public:
    explicit SettingText(const char *text);

    bool        on() const;
    const char *text() const;
    [[nodiscard]] bool number(uint64_t &value) const;
    SettingText after(char mark) const;

private:
    const char *text_;
};

}

#endif

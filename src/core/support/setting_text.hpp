/* One setting's text, read by the rules every av knob is documented with,
 * whichever side of the guest boundary reads it (docs/testing.md, "Knobs"):
 * a switch is on when it is set and starts with '1' - so "0" turns nothing on
 * - and a number is the decimal digits it starts with. The host reads its
 * environment through EnvSetting, the guest's drivers through GuestSetting;
 * both judge the text here. */
#ifndef AV_SETTING_TEXT_HPP
#define AV_SETTING_TEXT_HPP

#include <stdint.h>

namespace av {

class SettingText {
public:
    /* `text` outlives this; null for a setting that is not set. */
    explicit SettingText(const char *text);

    bool        on() const;
    /* What it was set to; null when it was not. */
    const char *text() const;
    /* The decimal number it starts with, in `value`. False, and `value` left
     * alone, when it is not set, starts with no digit, or overflows. */
    [[nodiscard]] bool number(uint64_t &value) const;
    /* What follows the first `mark` in it, as a setting of its own: not set
     * when it is not, or holds no `mark` ("300000+20000" after '+' is
     * "20000"). */
    SettingText after(char mark) const;

private:
    const char *text_;
};

}  // namespace av

#endif  // AV_SETTING_TEXT_HPP

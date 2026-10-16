#include "core/support/setting_text.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

SettingText::SettingText(const char *text) : text_(text)
{
    AV_SCOPE("SettingText::SettingText");
}

bool SettingText::on() const
{
    AV_SCOPE("SettingText::on");
    return text_ != nullptr && text_[0] == '1';
}

const char *SettingText::text() const
{
    AV_SCOPE("SettingText::text");
    return text_;
}

bool SettingText::number(uint64_t &value) const
{
    AV_SCOPE("SettingText::number");
    if (text_ == nullptr || text_[0] < '0' || text_[0] > '9') return false;
    constexpr uint64_t most = ~uint64_t{0};
    uint64_t           read = 0u;
    for (const char *digit = text_; *digit >= '0' && *digit <= '9'; digit++) {
        const uint64_t place = static_cast<uint64_t>(*digit - '0');
        if (read > (most - place) / 10u) return false;
        read = read * 10u + place;
    }
    value = read;
    return true;
}

SettingText SettingText::after(char mark) const
{
    AV_SCOPE("SettingText::after");
    if (text_ == nullptr) return SettingText(nullptr);
    for (const char *at = text_; *at != '\0'; at++)
        if (*at == mark) return SettingText(at + 1);
    return SettingText(nullptr);
}

}  // namespace av

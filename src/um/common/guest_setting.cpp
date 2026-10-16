#include "um/common/guest_setting.hpp"

#include "core/trace/function_scope.hpp"

#include <windows.h>

namespace av {

GuestSetting::GuestSetting(const char *name) : value_{}, set_(false)
{
    AV_SCOPE("GuestSetting::GuestSetting");
    /* The length without its NUL when it fits; the size it needs, NUL
     * included, when it does not; 0 when it is not set. */
    const DWORD length = GetEnvironmentVariableA(name, value_, value_bytes);
    set_ = length != 0u && length < value_bytes;
    if (!set_) value_[0] = '\0';
}

SettingText GuestSetting::value() const
{
    AV_SCOPE("GuestSetting::value");
    return SettingText(set_ ? value_ : nullptr);
}

}  // namespace av

/* One of the host process's environment variables: the debug switches the
 * device and the executor read (AV_PERFETTO, AV_TRACE_SCOPES, AV_PTRACE,
 * AV_VK_SUBMIT_TIMING), the numbers (AV_TRACE_DEPTH) and the paths and lists
 * they are given (AV_D3D_PATH, AV_TRACE_ONLY), judged by SettingText's rules
 * as the guest's drivers judge theirs - so AV_PERFETTO=0 turns nothing on
 * rather than some readers' half of it. Read once, when it is made. */
#ifndef AV_ENV_SETTING_HPP
#define AV_ENV_SETTING_HPP

#include "core/support/setting_text.hpp"

#include <stdint.h>

namespace av {

class EnvSetting {
public:
    explicit EnvSetting(const char *name);

    /* Set, and starting with '1'. */
    bool        on() const;
    /* What it was set to; null when it was not. */
    const char *text() const;
    /* The decimal number it starts with (SettingText::number). */
    [[nodiscard]] bool number(uint64_t &value) const;

private:
    SettingText value_;
};

}  // namespace av

#endif  // AV_ENV_SETTING_HPP

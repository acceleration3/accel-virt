#include "device/host/env_setting.hpp"

#include "core/trace/function_scope.hpp"

/* Freestanding: the declaration <stdlib.h> would give, for libc's own. */
extern "C" char *getenv(const char *name);

namespace av {

EnvSetting::EnvSetting(const char *name) : value_(getenv(name))
{
    AV_SCOPE("EnvSetting::EnvSetting");
}

bool EnvSetting::on() const
{
    AV_SCOPE("EnvSetting::on");
    return value_ != nullptr && value_[0] == '1';
}

const char *EnvSetting::text() const
{
    AV_SCOPE("EnvSetting::text");
    return value_;
}

}  // namespace av

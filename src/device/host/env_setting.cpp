#include "device/host/env_setting.hpp"

#include "core/trace/function_scope.hpp"

extern "C" char *getenv(const char *name);

namespace av {

EnvSetting::EnvSetting(const char *name) : value_(getenv(name))
{
    AV_SCOPE("EnvSetting::EnvSetting");
}

bool EnvSetting::on() const
{
    AV_SCOPE("EnvSetting::on");
    return value_.on();
}

const char *EnvSetting::text() const
{
    AV_SCOPE("EnvSetting::text");
    return value_.text();
}

bool EnvSetting::number(uint64_t &value) const
{
    AV_SCOPE("EnvSetting::number");
    return value_.number(value);
}

}

#include "device/host/env_setting.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdlib>
#include <cstring>

using av::EnvSetting;

namespace {

constexpr const char *name = "AV_ENV_SETTING_TEST";

}

TEST(env_setting, on_only_when_set_and_starting_with_one)
{ unsetenv(name);
  CHECK(!EnvSetting(name).on());
  CHECK(EnvSetting(name).text() == nullptr);
  setenv(name, "1", 1);
  CHECK(EnvSetting(name).on());
  setenv(name, "10", 1);
  CHECK(EnvSetting(name).on());
  setenv(name, "0", 1);
  CHECK(!EnvSetting(name).on());
  setenv(name, "", 1);
  CHECK(!EnvSetting(name).on());
  CHECK(std::strcmp(EnvSetting(name).text(), "") == 0);
  setenv(name, "/opt/d3d", 1);
  CHECK(!EnvSetting(name).on());
  CHECK(std::strcmp(EnvSetting(name).text(), "/opt/d3d") == 0);
  unsetenv(name); }

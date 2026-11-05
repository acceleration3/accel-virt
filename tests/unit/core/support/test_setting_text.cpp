#include "core/support/setting_text.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>
#include <cstring>

using av::SettingText;

TEST(setting_text, is_on_only_for_text_that_begins_with_one)
{ CHECK(SettingText("1").on());
  CHECK(SettingText("1x").on());
  CHECK(!SettingText("0").on());
  CHECK(!SettingText("").on());
  CHECK(!SettingText("yes").on());
  CHECK(!SettingText(nullptr).on()); }

TEST(setting_text, keeps_the_text_it_was_given)
{ const char *const given = "a=b";
  CHECK(SettingText(given).text() == given);
  CHECK(SettingText(nullptr).text() == nullptr); }

TEST(setting_text, reads_a_number_up_to_the_first_character_that_is_no_digit)
{ std::uint64_t value = 77u;
  CHECK(SettingText("0").number(value));
  CHECK_EQ(value, 0u);
  CHECK(SettingText("1234x9").number(value));
  CHECK_EQ(value, 1234u);
  CHECK(SettingText("18446744073709551615").number(value));
  CHECK_EQ(value, ~std::uint64_t{0});
  value = 77u;
  CHECK(!SettingText("18446744073709551616").number(value));
  CHECK(!SettingText("99999999999999999999999").number(value));
  CHECK(!SettingText("x1").number(value));
  CHECK(!SettingText("-1").number(value));
  CHECK(!SettingText("").number(value));
  CHECK(!SettingText(nullptr).number(value));
  CHECK_EQ(value, 77u); }

TEST(setting_text, takes_what_follows_the_first_mark)
{ const SettingText setting("width=640:height=480");
  CHECK_EQ(std::strcmp(setting.after('=').text(), "640:height=480"), 0);
  CHECK_EQ(std::strcmp(setting.after(':').text(), "height=480"), 0);
  std::uint64_t value = 0u;
  CHECK(setting.after('=').number(value));
  CHECK_EQ(value, 640u);
  CHECK(setting.after('#').text() == nullptr);
  CHECK(SettingText(nullptr).after('=').text() == nullptr);
  CHECK(SettingText("a=").after('=').text()[0] == '\0'); }

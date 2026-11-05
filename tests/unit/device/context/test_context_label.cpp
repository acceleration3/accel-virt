#include "device/context/context_label.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstddef>
#include <cstring>

using av::ContextLabel;

TEST(context_label, a_short_name_is_kept)
{ const ContextLabel label("avtest.exe");
  CHECK(std::strcmp(label.text(), "avtest.exe") == 0); }

TEST(context_label, no_name_gives_an_empty_label)
{ const ContextLabel absent(nullptr);
  const ContextLabel empty("");
  CHECK_EQ(absent.text()[0], 0);
  CHECK_EQ(empty.text()[0], 0); }

TEST(context_label, a_name_of_exactly_the_capacity_is_kept_whole)
{ char name[ContextLabel::capacity + 1u];
  std::memset(name, 'n', ContextLabel::capacity);
  name[ContextLabel::capacity] = '\0';
  const ContextLabel label(name);
  CHECK_EQ(std::strlen(label.text()), ContextLabel::capacity);
  CHECK(std::strcmp(label.text(), name) == 0); }

TEST(context_label, a_longer_name_is_cut_at_the_capacity_and_terminated)
{ char name[ContextLabel::capacity * 4u + 1u];
  std::memset(name, 'n', sizeof name - 1u);
  name[sizeof name - 1u] = '\0';
  const ContextLabel label(name);
  CHECK_EQ(std::strlen(label.text()), ContextLabel::capacity);
  CHECK_EQ(label.text()[ContextLabel::capacity], 0); }

TEST(context_label, a_name_one_past_the_capacity_loses_only_its_last_byte)
{ char name[ContextLabel::capacity + 2u];
  std::memset(name, 'a', ContextLabel::capacity);
  name[ContextLabel::capacity] = 'b';
  name[ContextLabel::capacity + 1u] = '\0';
  const ContextLabel label(name);
  CHECK_EQ(std::strlen(label.text()), ContextLabel::capacity);
  CHECK(std::strchr(label.text(), 'b') == nullptr); }

TEST(context_label, a_label_does_not_alias_the_name_it_was_made_from)
{ char name[8] = "abc";
  const ContextLabel label(name);
  name[0] = 'X';
  CHECK(std::strcmp(label.text(), "abc") == 0); }

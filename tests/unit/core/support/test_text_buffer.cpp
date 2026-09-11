#include "core/support/text_buffer.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstring>

using av::TextBuffer;

TEST(text_buffer_append, builds_one_terminated_message)
{ char bytes[32];
  std::memset(bytes, 'x', sizeof bytes);
  TextBuffer text(bytes, sizeof bytes);
  CHECK_EQ(text.length(), 0u);
  CHECK(std::strcmp(text.text(), "") == 0);
  text.append("host-gpu '");
  text.append("auto");
  text.append("'");
  CHECK(std::strcmp(text.text(), "host-gpu 'auto'") == 0);
  CHECK_EQ(text.length(), 15u); }

TEST(text_buffer_append, truncates_at_capacity_and_stays_terminated)
{ char bytes[8];
  std::memset(bytes, 'x', sizeof bytes);
  TextBuffer text(bytes, 6u);
  text.append("abc");
  text.append("defgh");
  CHECK(std::strcmp(text.text(), "abcde") == 0);
  CHECK_EQ(text.length(), 5u);
  CHECK_EQ(bytes[6], 'x');
  text.append("more");
  CHECK_EQ(text.length(), 5u); }

TEST(text_buffer, without_room_writes_nothing)
{ char bytes[1] = {'x'};
  TextBuffer none(nullptr, 16u);
  none.append("ignored");
  CHECK(std::strcmp(none.text(), "") == 0);
  TextBuffer empty(bytes, 0u);
  empty.append("ignored");
  CHECK_EQ(bytes[0], 'x');
  CHECK_EQ(empty.length(), 0u);
  CHECK(std::strcmp(empty.text(), "") == 0);
  TextBuffer only_terminator(bytes, 1u);
  only_terminator.append("ignored");
  CHECK_EQ(bytes[0], '\0'); }

TEST(text_buffer_append_decimal, writes_every_digit_and_truncates_like_text)
{ char bytes[32];
  TextBuffer text(bytes, sizeof bytes);
  text.append("tail ");
  text.append_decimal(0u);
  text.append(" ");
  text.append_decimal(18446744073709551615ull);
  CHECK(std::strcmp(text.text(), "tail 0 18446744073709551615") == 0);
  char small[4];
  TextBuffer cut(small, sizeof small);
  cut.append_decimal(98765u);
  CHECK(std::strcmp(cut.text(), "987") == 0); }

TEST(text_buffer_append_hex, writes_a_fixed_width_of_uppercase_digits_and_truncates_like_text)
{ char bytes[48];
  TextBuffer text(bytes, sizeof bytes);
  text.append_hex(0xC0000001u, 8u);
  text.append(" ");
  text.append_hex(0xAu, 4u);
  text.append(" ");
  text.append_hex(0x123u, 2u);   /* too narrow: the low digits */
  text.append(" ");
  text.append_hex(0xFFu, 0u);    /* no digits asked for */
  text.append_hex(0x0123456789ABCDEFull, 20u);   /* wider than a u64: 16 */
  CHECK(std::strcmp(text.text(), "C0000001 000A 23 0123456789ABCDEF") == 0);
  char small[4];
  TextBuffer cut(small, sizeof small);
  cut.append_hex(0xBEEFu, 4u);
  CHECK(std::strcmp(cut.text(), "BEE") == 0); }

#include "core/support/four_cc.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::FourCc;

TEST(four_cc, packs_the_first_character_into_the_low_byte)
{ CHECK_EQ(FourCc('D', 'X', 'T', '1').value(), 0x31545844u);
  CHECK_EQ(FourCc('A', 'R', '2', '4').value(), 0x34325241u); }

TEST(four_cc, a_high_character_fills_only_its_own_byte)
{ CHECK_EQ(FourCc('\x80', 'A', 'B', 'C').value(), 0x43424180u);
  CHECK_EQ(FourCc('A', '\xff', 'B', 'C').value(), 0x4342ff41u);
  CHECK_EQ(FourCc('A', 'B', '\xfe', 'C').value(), 0x43fe4241u);
  CHECK_EQ(FourCc('A', 'B', 'C', '\xff').value(), 0xff434241u); }

TEST(four_cc, is_a_constant_a_switch_can_name)
{ constexpr std::uint32_t code = FourCc('I', 'N', 'T', 'Z').value();
  static_assert(code == 0x5a544e49u, "packed at compile time");
  CHECK_EQ(code, 0x5a544e49u); }

TEST(four_cc, distinct_names_never_pack_alike)
{ CHECK(FourCc('D', 'X', 'T', '1').value() != FourCc('1', 'T', 'X', 'D').value());
  CHECK(FourCc('X', 'R', '2', '4').value() != FourCc('A', 'R', '2', '4').value()); }

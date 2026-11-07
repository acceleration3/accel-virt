#include "harness/framework/test_registry.hpp"
#include "icd/d3d/vcache_info.hpp"

#include <cstdint>
#include <cstring>

using av::d3d::VcacheInfo;

TEST(vcache_info, writes_the_four_words_of_the_ddi_structure_with_the_cache_pattern_first)
{ std::uint32_t words[VcacheInfo::bytes / sizeof(std::uint32_t)];
  std::memset(words, 0xAB, sizeof words);
  CHECK(VcacheInfo().write(words, sizeof words));
  CHECK_EQ(words[0], 0x48434143u);
  CHECK_EQ(words[1], 0u);
  CHECK_EQ(words[2], 0u);
  CHECK_EQ(words[3], 0u);
  CHECK_EQ(VcacheInfo::bytes, 16u); }

TEST(vcache_info, writes_nothing_to_a_missing_or_short_destination)
{ std::uint8_t bytes[VcacheInfo::bytes + 4u];
  std::memset(bytes, 0x5C, sizeof bytes);
  CHECK(!VcacheInfo().write(nullptr, sizeof bytes));
  CHECK(!VcacheInfo().write(bytes, VcacheInfo::bytes - 1u));
  for (const std::uint8_t byte : bytes) CHECK_EQ(byte, 0x5Cu);
  CHECK(VcacheInfo().write(bytes, sizeof bytes));
  CHECK_EQ(bytes[VcacheInfo::bytes], 0x5Cu); }

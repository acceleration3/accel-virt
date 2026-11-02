#include "harness/framework/test_registry.hpp"
#include "icd/d3d/ddi_version_list.hpp"
#include "icd/d3d/hresult.hpp"

#include <cstdint>

using av::d3d::DdiVersionList;
using av::d3d::RuntimeKind;

namespace {

constexpr std::uint64_t d3d11_word = 0x000B002D00010000ull;

}

TEST(ddi_version_list, the_d3d11_list_is_the_wddm_3_2_word_and_nothing_else)
{ const DdiVersionList list(RuntimeKind::d3d10_11);
  CHECK_EQ(list.count(), 1u);
  CHECK_EQ(list.word(0u), d3d11_word);
  CHECK_EQ(list.word(0u) >> 32, (11ull << 16) | 45ull);
  CHECK_EQ((list.word(0u) >> 16) & 0xFFFFull, 1u);
  CHECK(list.invariants_hold()); }

TEST(ddi_version_list, the_d3d12_list_is_the_newest_build_on_interface_r8)
{ const DdiVersionList list(RuntimeKind::d3d12);
  CHECK_EQ(list.count(), 1u);
  CHECK_EQ((list.word(0u) >> 16) & 0xFFFFull, 110u);
  CHECK(list.word(0u) != d3d11_word);
  CHECK(list.invariants_hold()); }

TEST(ddi_version_list, a_null_array_answers_the_count_and_writes_nothing)
{ const DdiVersionList list(RuntimeKind::d3d10_11);
  std::uint32_t entries = 0u;
  CHECK_EQ(list.answer(&entries, nullptr), av::d3d::hr_ok);
  CHECK_EQ(entries, 1u);
  CHECK_EQ(list.answer(&entries, nullptr), av::d3d::hr_ok);
  CHECK_EQ(entries, 1u); }

TEST(ddi_version_list, an_exact_array_is_filled_and_a_longer_one_is_trimmed)
{ const DdiVersionList list(RuntimeKind::d3d10_11);
  std::uint64_t exact[1] = {0ull};
  std::uint32_t entries = 1u;
  CHECK_EQ(list.answer(&entries, exact), av::d3d::hr_ok);
  CHECK_EQ(entries, 1u);
  CHECK_EQ(exact[0], d3d11_word);

  std::uint64_t roomy[4] = {1ull, 2ull, 3ull, 4ull};
  entries = 4u;
  CHECK_EQ(list.answer(&entries, roomy), av::d3d::hr_ok);
  CHECK_EQ(entries, 1u);
  CHECK_EQ(roomy[0], d3d11_word);
  CHECK_EQ(roomy[1], 2ull); }

TEST(ddi_version_list, a_short_array_is_refused_with_the_count_written_back)
{ const DdiVersionList list(RuntimeKind::d3d10_11);
  std::uint64_t none[1] = {0xDEADull};
  std::uint32_t entries = 0u;
  CHECK_EQ(list.answer(&entries, none), av::d3d::hr_out_of_memory);
  CHECK_EQ(entries, 1u);
  CHECK_EQ(none[0], 0xDEADull); }

TEST(ddi_version_list, a_null_count_is_refused_whatever_the_array)
{ const DdiVersionList list(RuntimeKind::d3d10_11);
  std::uint64_t words[1] = {0ull};
  CHECK_EQ(list.answer(nullptr, words), av::d3d::hr_invalid_arg);
  CHECK_EQ(words[0], 0ull);
  CHECK_EQ(list.answer(nullptr, nullptr), av::d3d::hr_invalid_arg); }

TEST(ddi_version_list, serves_only_the_interface_of_the_word_it_holds)
{ const DdiVersionList list(RuntimeKind::d3d10_11);
  const std::uint32_t served = static_cast<std::uint32_t>(d3d11_word >> 32);
  CHECK(list.serves(served));
  CHECK(!list.serves(served - 1u));
  CHECK(!list.serves(served + 1u));
  CHECK(!list.serves(0u));
  CHECK(!list.serves(0xFFFFFFFFu));
  CHECK(!list.serves(static_cast<std::uint32_t>(d3d11_word))); }

TEST(ddi_version_list, the_two_runtimes_serve_different_interfaces)
{ const DdiVersionList eleven(RuntimeKind::d3d10_11);
  const DdiVersionList twelve(RuntimeKind::d3d12);
  CHECK(!eleven.serves(static_cast<std::uint32_t>(twelve.word(0u) >> 32)));
  CHECK(!twelve.serves(static_cast<std::uint32_t>(eleven.word(0u) >> 32))); }

TEST(ddi_version_list, word_past_the_count_answers_zero)
{ const DdiVersionList list(RuntimeKind::d3d10_11);
  CHECK_EQ(list.word(1u), 0ull);
  CHECK_EQ(list.word(DdiVersionList::max_words), 0ull);
  CHECK_EQ(list.word(0xFFFFFFFFu), 0ull); }

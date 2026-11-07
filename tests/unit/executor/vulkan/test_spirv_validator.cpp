#include "executor/vulkan/guest_device.hpp"
#include "executor/vulkan/spirv_validator.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>
#include <cstring>
#include <vector>

using av::SpirvValidator;
using Verdict = SpirvValidator::Verdict;

namespace {

const std::uint32_t compute_nop[] = {
    0x07230203u, 0x00010600u, 0x00070000u, 0x00000005u, 0x00000000u, 0x00020011u, 0x00000001u, 0x0003000eu,
    0x00000000u, 0x00000001u, 0x0005000fu, 0x00000005u, 0x00000001u, 0x6e69616du, 0x00000000u, 0x00060010u,
    0x00000001u, 0x00000011u, 0x00000001u, 0x00000001u, 0x00000001u, 0x00020013u, 0x00000002u, 0x00030021u,
    0x00000003u, 0x00000002u, 0x00050036u, 0x00000002u, 0x00000001u, 0x00000000u, 0x00000003u, 0x000200f8u,
    0x00000004u, 0x000100fdu, 0x00010038u};

constexpr std::size_t nop_bytes = sizeof compute_nop;

std::vector<std::uint32_t> copy_of(const std::uint32_t *words, std::size_t bytes)
{ return std::vector<std::uint32_t>(words, words + bytes / sizeof(std::uint32_t)); }

}

TEST(spirv_validator, a_well_formed_module_is_accepted)
{ SpirvValidator validator;
  CHECK(validator.check(compute_nop, nop_bytes) == Verdict::valid);
  CHECK(validator.invariants_hold()); }

TEST(spirv_validator, a_module_without_the_magic_number_is_refused)
{ SpirvValidator validator;
  std::vector<std::uint32_t> bad = copy_of(compute_nop, nop_bytes);
  bad[0] = 0u;
  CHECK(validator.check(bad.data(), nop_bytes) == Verdict::invalid); }

TEST(spirv_validator, a_truncated_module_is_refused_without_reading_past_it)
{ SpirvValidator validator;
  for (std::size_t words = 0u; words < nop_bytes / 4u; words++) {
      std::vector<std::uint32_t> cut = copy_of(compute_nop, words * 4u);
      cut.shrink_to_fit();
      CHECK(validator.check(cut.data(), words * 4u) == Verdict::invalid);
  } }

TEST(spirv_validator, a_size_that_is_not_a_whole_number_of_words_is_refused)
{ SpirvValidator validator;
  CHECK(validator.check(compute_nop, nop_bytes - 2u) == Verdict::invalid);
  CHECK(validator.check(nullptr, nop_bytes) == Verdict::invalid); }

TEST(spirv_validator, an_instruction_that_runs_past_the_module_is_refused)
{ SpirvValidator validator;
  std::vector<std::uint32_t> bad = copy_of(compute_nop, nop_bytes);
  bad[5] = 0x00FF0011u;
  CHECK(validator.check(bad.data(), nop_bytes) == Verdict::invalid); }

TEST(spirv_validator, a_missing_entry_point_target_is_refused)
{ SpirvValidator validator;
  std::vector<std::uint32_t> bad = copy_of(compute_nop, nop_bytes);
  bad[11] = 0x00000063u;
  CHECK(validator.check(bad.data(), nop_bytes) == Verdict::invalid); }

TEST(spirv_validator, every_single_word_mutation_gets_a_verdict_and_never_crashes)
{ SpirvValidator validator;
  std::uint32_t refused = 0u;
  for (std::size_t word = 0u; word < nop_bytes / 4u; word++) {
      for (std::uint32_t pattern : {0u, 0xFFFFFFFFu, 0x80000000u, 0x00010001u}) {
          std::vector<std::uint32_t> bad = copy_of(compute_nop, nop_bytes);
          bad[word] = pattern;
          if (validator.check(bad.data(), nop_bytes) == Verdict::invalid) refused++;
      }
  }
  CHECK(refused > 40u);
  CHECK(validator.invariants_hold()); }

TEST(spirv_validator, a_second_check_of_the_same_module_costs_no_validation)
{ SpirvValidator validator;
  CHECK(validator.check(compute_nop, nop_bytes) == Verdict::valid);
  CHECK_EQ(validator.validations(), std::uint64_t{1});
  CHECK(validator.check(compute_nop, nop_bytes) == Verdict::valid);
  CHECK_EQ(validator.validations(), std::uint64_t{1});
  CHECK_EQ(validator.cache_hits(), std::uint64_t{1});
  CHECK_EQ(validator.validated_bytes(), std::uint64_t{nop_bytes}); }

TEST(spirv_validator, a_refusal_is_cached_too)
{ SpirvValidator validator;
  std::vector<std::uint32_t> bad = copy_of(compute_nop, nop_bytes);
  bad[0] = 1u;
  CHECK(validator.check(bad.data(), nop_bytes) == Verdict::invalid);
  CHECK(validator.check(bad.data(), nop_bytes) == Verdict::invalid);
  CHECK_EQ(validator.cache_hits(), std::uint64_t{1}); }

TEST(spirv_validator, a_module_that_differs_in_one_word_is_not_served_from_another_ones_verdict)
{ SpirvValidator validator;
  CHECK(validator.check(compute_nop, nop_bytes) == Verdict::valid);
  std::vector<std::uint32_t> bad = copy_of(compute_nop, nop_bytes);
  bad[10] = 0x00000009u;
  CHECK(validator.check(bad.data(), nop_bytes) == Verdict::invalid);
  CHECK_EQ(validator.cache_hits(), std::uint64_t{0}); }

TEST(spirv_validator, the_cache_survives_more_modules_than_it_has_slots)
{ SpirvValidator validator;
  for (std::uint32_t round = 0u; round < SpirvValidator::cache_slots * 2u; round++) {
      std::vector<std::uint32_t> variant = copy_of(compute_nop, nop_bytes);
      variant[0] = 0x10u + round;
      CHECK(validator.check(variant.data(), nop_bytes) == Verdict::invalid);
      CHECK(validator.check(compute_nop, nop_bytes) == Verdict::valid);
  }
  CHECK(validator.invariants_hold()); }

TEST(guest_device, a_refused_shader_is_reported_as_the_nv_error_only_when_the_extension_is_enabled)
{ av::GuestDevice device;
  CHECK_EQ(static_cast<int>(device.shader_refusal()), static_cast<int>(VK_ERROR_UNKNOWN));
  device.note_invalid_shader_extension(true);
  CHECK_EQ(static_cast<int>(device.shader_refusal()), static_cast<int>(VK_ERROR_INVALID_SHADER_NV));
  device.note_invalid_shader_extension(false);
  CHECK_EQ(static_cast<int>(device.shader_refusal()), static_cast<int>(VK_ERROR_UNKNOWN)); }

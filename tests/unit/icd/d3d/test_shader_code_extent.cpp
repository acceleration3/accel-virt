#include "harness/framework/test_registry.hpp"
#include "icd/d3d/shader_code_extent.hpp"

#include <cstdint>
#include <cstring>

using av::d3d::ShaderCodeExtent;

TEST(shader_code_extent, a_container_is_as_long_as_it_says_when_the_runtime_gave_that_much)
{ std::uint32_t code[16] = {};
  code[0] = ShaderCodeExtent::container_magic;
  const std::uint32_t total = 48u;
  std::memcpy(reinterpret_cast<std::uint8_t *>(code) + ShaderCodeExtent::container_size_at, &total, sizeof total);
  CHECK_EQ(ShaderCodeExtent(sizeof code).bytes(code), std::size_t{48}); }

TEST(shader_code_extent, a_container_that_claims_more_than_was_given_is_cut_to_what_was_given)
{ std::uint32_t code[16] = {};
  code[0] = ShaderCodeExtent::container_magic;
  const std::uint32_t total = 4096u;
  std::memcpy(reinterpret_cast<std::uint8_t *>(code) + ShaderCodeExtent::container_size_at, &total, sizeof total);
  CHECK_EQ(ShaderCodeExtent(40u).bytes(code), std::size_t{40});
  CHECK_EQ(ShaderCodeExtent(20u).bytes(code), std::size_t{0}); }

TEST(shader_code_extent, a_token_stream_with_a_wild_length_is_cut_to_what_was_given)
{ std::uint32_t code[8] = {0x12345u, 0x0ee9c49fu, 0u, 0u, 0u, 0u, 0u, 0u};
  CHECK_EQ(ShaderCodeExtent(sizeof code).bytes(code), sizeof code);
  const std::uint32_t small[4] = {0x12345u, 3u, 0u, 0u};
  CHECK_EQ(ShaderCodeExtent(sizeof small).bytes(small), std::size_t{12});
  CHECK_EQ(ShaderCodeExtent(4u).bytes(small), std::size_t{0});
  CHECK_EQ(ShaderCodeExtent(sizeof small).bytes(nullptr), std::size_t{0}); }

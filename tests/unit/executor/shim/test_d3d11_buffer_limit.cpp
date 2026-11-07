#include "executor/shim/d3d11_buffer_limit.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::D3d11BufferLimit;

namespace {
constexpr std::uint64_t mebibyte = std::uint64_t{1} << 20u;
constexpr std::uint64_t gibibyte = std::uint64_t{1} << 30u;
}

TEST(d3d11_buffer_limit, is_a_quarter_of_video_memory_between_128_and_2048_mebibytes)
{ CHECK_EQ(D3d11BufferLimit(0u).mebibytes(), 128u);
  CHECK_EQ(D3d11BufferLimit(256u * mebibyte).mebibytes(), 128u);
  CHECK_EQ(D3d11BufferLimit(512u * mebibyte).mebibytes(), 128u);
  CHECK_EQ(D3d11BufferLimit(gibibyte).mebibytes(), 256u);
  CHECK_EQ(D3d11BufferLimit(4u * gibibyte).mebibytes(), 1024u);
  CHECK_EQ(D3d11BufferLimit(8u * gibibyte).mebibytes(), 2048u); }

TEST(d3d11_buffer_limit, never_passes_2048_mebibytes_however_much_memory_the_adapter_has)
{ CHECK_EQ(D3d11BufferLimit(16u * gibibyte).mebibytes(), 2048u);
  CHECK_EQ(D3d11BufferLimit(~std::uint64_t{0}).mebibytes(), 2048u);
  CHECK_EQ(D3d11BufferLimit(8u * gibibyte + 3u * mebibyte).mebibytes(), 2048u);
  CHECK_EQ(D3d11BufferLimit(6u * gibibyte).mebibytes(), 1536u); }

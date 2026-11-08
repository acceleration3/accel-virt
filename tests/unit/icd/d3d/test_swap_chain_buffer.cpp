#include "harness/framework/test_registry.hpp"
#include "icd/d3d/swap_chain_buffer.hpp"

#include <cstdint>

using av::d3d::SwapChainBuffer;
using Model = av::d3d::SwapChainBuffer::Model;

namespace {
constexpr std::uint32_t flip_model_misc = 0x20002u;
constexpr std::uint32_t blt_model_misc = 0x8u;

}

TEST(swap_chain_buffer, a_shared_buffer_is_flipped_and_the_application_s_own_is_blitted)
{ CHECK(SwapChainBuffer(false, flip_model_misc).model() == Model::flip);
  CHECK(SwapChainBuffer(false, SwapChainBuffer::misc_shared).model() == Model::flip);
  CHECK(SwapChainBuffer(false, blt_model_misc).model() == Model::blt);
  CHECK(SwapChainBuffer(false, 0u).model() == Model::blt); }

TEST(swap_chain_buffer, a_primary_is_scanned_out)
{ CHECK(SwapChainBuffer(true, 0u).model() == Model::scanout);
  CHECK(SwapChainBuffer(true, flip_model_misc).model() == Model::scanout); }

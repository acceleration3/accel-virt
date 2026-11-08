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

TEST(swap_chain_buffer, a_blt_present_of_a_shared_buffer_is_blitted_and_a_flip_present_is_flipped)
{ const SwapChainBuffer shared(false, SwapChainBuffer::misc_shared);
  CHECK(shared.model_for(true, false) == Model::blt);
  CHECK(shared.model_for(false, true) == Model::flip);
  CHECK(shared.model_for(true, true) == Model::flip);
  CHECK(shared.model_for(false, false) == Model::flip);
  const SwapChainBuffer owned(false, blt_model_misc);
  CHECK(owned.model_for(false, true) == Model::blt);
  CHECK(owned.model_for(true, false) == Model::blt); }

TEST(swap_chain_buffer, a_primary_stays_scanned_out_whatever_the_present_says)
{ const SwapChainBuffer primary(true, 0u);
  CHECK(primary.model_for(true, false) == Model::scanout);
  CHECK(primary.model_for(false, true) == Model::scanout); }

TEST(swap_chain_buffer, a_default_buffer_is_blitted)
{ CHECK(SwapChainBuffer().model() == Model::blt); }

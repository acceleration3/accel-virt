#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/opengl/client_array_state.hpp"

#include <cstdint>

using av::MockPlatform;
using av::Status;
using av::glicd::ClientArrayState;

namespace {

constexpr std::uint32_t array_buffer = ClientArrayState::array_buffer;
constexpr std::uint32_t pack = ClientArrayState::pixel_pack_buffer;
constexpr std::uint32_t unpack = ClientArrayState::pixel_unpack_buffer;
constexpr std::uint32_t elements = ClientArrayState::element_array_buffer;
constexpr std::uint32_t everything = ClientArrayState::client_pixel_store_bit | ClientArrayState::client_vertex_array_bit;
constexpr std::uint32_t unpack_alignment = 0x0CF5u;
constexpr std::uint32_t pack_alignment = 0x0D05u;
constexpr std::uint32_t unknown = ClientArrayState::unknown;

void push_levels(ClientArrayState &state, std::uint32_t levels)
{
    for (std::uint32_t level = 1u; level <= levels; level++) {
        state.bind_buffer(array_buffer, level);
        state.push_client(ClientArrayState::client_vertex_array_bit);
        CHECK(state.invariants_hold());
    }
}

}

TEST(client_array_state_stack, a_pop_with_nothing_pushed_changes_nothing)
{ MockPlatform     platform;
  ClientArrayState state(platform);
  state.bind_buffer(array_buffer, 7u);
  state.pop_client();
  CHECK_EQ(state.bound_buffer(array_buffer), 7u);
  CHECK(state.invariants_hold()); }

TEST(client_array_state_stack, a_pop_restores_what_a_push_saved_for_the_vertex_array_bit)
{ MockPlatform     platform;
  ClientArrayState state(platform);
  state.bind_buffer(array_buffer, 5u);
  state.push_client(ClientArrayState::client_vertex_array_bit);
  state.bind_buffer(array_buffer, 9u);
  state.pop_client();
  CHECK_EQ(state.bound_buffer(array_buffer), 5u);
  CHECK(state.invariants_hold()); }

TEST(client_array_state_stack, a_mask_without_the_vertex_array_bit_leaves_the_array_binding_alone)
{ MockPlatform     platform;
  ClientArrayState state(platform);
  state.bind_buffer(array_buffer, 5u);
  state.push_client(ClientArrayState::client_pixel_store_bit);
  state.bind_buffer(array_buffer, 9u);
  state.pop_client();
  CHECK_EQ(state.bound_buffer(array_buffer), 9u); }

TEST(client_array_state_stack, the_pixel_store_bit_restores_the_pixel_buffers_and_both_stores)
{ MockPlatform     platform;
  ClientArrayState state(platform);
  state.bind_buffer(pack, 11u);
  state.bind_buffer(unpack, 12u);
  CHECK(state.store_pixel(pack_alignment, 1));
  CHECK(state.store_pixel(unpack_alignment, 2));
  state.push_client(ClientArrayState::client_pixel_store_bit);
  state.bind_buffer(pack, 21u);
  state.bind_buffer(unpack, 22u);
  CHECK(state.store_pixel(pack_alignment, 8));
  CHECK(state.store_pixel(unpack_alignment, 8));
  state.pop_client();
  CHECK_EQ(state.bound_buffer(pack), 11u);
  CHECK_EQ(state.bound_buffer(unpack), 12u);
  CHECK_EQ(state.pixel_value(pack_alignment), 1);
  CHECK_EQ(state.pixel_value(unpack_alignment), 2);
  CHECK(state.invariants_hold()); }

TEST(client_array_state_stack, a_mask_without_the_pixel_store_bit_leaves_the_pixel_state_alone)
{ MockPlatform     platform;
  ClientArrayState state(platform);
  state.bind_buffer(pack, 11u);
  CHECK(state.store_pixel(pack_alignment, 1));
  state.push_client(ClientArrayState::client_vertex_array_bit);
  state.bind_buffer(pack, 21u);
  CHECK(state.store_pixel(pack_alignment, 8));
  state.pop_client();
  CHECK_EQ(state.bound_buffer(pack), 21u);
  CHECK_EQ(state.pixel_value(pack_alignment), 8); }

TEST(client_array_state_stack, a_pop_restores_the_vertex_array_and_its_element_buffer)
{ MockPlatform     platform;
  ClientArrayState state(platform);
  CHECK_STATUS(state.bind_vertex_array(4u), Status::ok);
  state.bind_buffer(elements, 31u);
  state.push_client(everything);
  CHECK_STATUS(state.bind_vertex_array(5u), Status::ok);
  state.bind_buffer(elements, 32u);
  state.pop_client();
  CHECK_EQ(state.bound_buffer(elements), 31u);
  CHECK_STATUS(state.bind_vertex_array(4u), Status::ok);
  CHECK_EQ(state.bound_buffer(elements), 31u);
  CHECK(state.invariants_hold()); }

TEST(client_array_state_stack, a_vertex_array_deleted_since_the_push_is_unbound_on_pop)
{ MockPlatform     platform;
  ClientArrayState state(platform);
  CHECK_STATUS(state.bind_vertex_array(4u), Status::ok);
  state.push_client(ClientArrayState::client_vertex_array_bit);
  const std::uint32_t gone[] = {4u};
  state.delete_vertex_arrays(1, gone);
  state.pop_client();
  state.bind_buffer(elements, 1u);
  CHECK_EQ(state.bound_buffer(elements), 1u);
  CHECK(state.invariants_hold()); }

TEST(client_array_state_stack, the_default_pushed_depth_of_sixteen_ignores_a_push_past_it)
{ MockPlatform     platform;
  ClientArrayState state(platform);
  push_levels(state, 17u);
  CHECK_EQ(state.bound_buffer(array_buffer), 17u);
  state.pop_client();
  CHECK_EQ(state.bound_buffer(array_buffer), 16u);
  for (std::uint32_t level = 15u; level >= 1u; level--) {
      state.pop_client();
      CHECK_EQ(state.bound_buffer(array_buffer), level);
  }
  state.pop_client();
  CHECK_EQ(state.bound_buffer(array_buffer), 1u);
  CHECK(state.invariants_hold()); }

TEST(client_array_state_stack, a_reported_limit_raises_how_many_pushes_the_host_takes)
{ MockPlatform     platform;
  ClientArrayState state(platform);
  state.limit_client_depth(40);
  push_levels(state, 40u);
  state.push_client(everything);
  state.bind_buffer(array_buffer, 99u);
  state.pop_client();
  CHECK_EQ(state.bound_buffer(array_buffer), 40u);
  CHECK(state.invariants_hold()); }

TEST(client_array_state_stack, a_zero_or_negative_reported_limit_is_ignored)
{ MockPlatform     platform;
  ClientArrayState state(platform);
  state.limit_client_depth(0);
  state.limit_client_depth(-5);
  state.limit_client_depth(INT32_MIN);
  push_levels(state, 17u);
  state.pop_client();
  CHECK_EQ(state.bound_buffer(array_buffer), 16u); }

TEST(client_array_state_stack, a_lowered_limit_ignores_pushes_while_deeper_pops_still_restore)
{ MockPlatform     platform;
  ClientArrayState state(platform);
  push_levels(state, 5u);
  state.limit_client_depth(2);
  state.push_client(everything);
  state.bind_buffer(array_buffer, 99u);
  state.pop_client();
  CHECK_EQ(state.bound_buffer(array_buffer), 5u);
  CHECK(state.invariants_hold()); }

TEST(client_array_state_stack, pushes_past_the_shadow_are_counted_and_popped_without_restoring)
{ MockPlatform     platform;
  ClientArrayState state(platform);
  state.limit_client_depth(100);
  push_levels(state, ClientArrayState::most_pushed);
  state.bind_buffer(array_buffer, 700u);
  for (std::uint32_t beyond = 0u; beyond < 36u; beyond++) {
      state.push_client(everything);
      CHECK(state.invariants_hold());
  }
  state.push_client(everything);
  CHECK(state.invariants_hold());
  state.bind_buffer(array_buffer, 800u);
  state.pop_client();
  CHECK_EQ(state.bound_buffer(array_buffer), unknown);
  CHECK(state.invariants_hold()); }

TEST(client_array_state_stack, popping_past_the_shadow_forgets_every_binding_it_could_not_save)
{ MockPlatform     platform;
  ClientArrayState state(platform);
  state.limit_client_depth(70);
  CHECK_STATUS(state.bind_vertex_array(4u), Status::ok);
  push_levels(state, ClientArrayState::most_pushed);
  state.bind_buffer(pack, 11u);
  state.bind_buffer(unpack, 12u);
  state.bind_buffer(elements, 13u);
  state.push_client(everything);
  CHECK(state.invariants_hold());
  state.pop_client();
  CHECK_EQ(state.bound_buffer(array_buffer), unknown);
  CHECK_EQ(state.bound_buffer(pack), unknown);
  CHECK_EQ(state.bound_buffer(unpack), unknown);
  CHECK_EQ(state.bound_buffer(elements), unknown);
  CHECK(!state.pixel_buffer_bound(true) || state.bound_buffer(pack) == unknown);
  CHECK(state.invariants_hold()); }

TEST(client_array_state_stack, after_the_unsaved_pops_the_saved_levels_restore_as_before)
{ MockPlatform     platform;
  ClientArrayState state(platform);
  state.limit_client_depth(70);
  push_levels(state, ClientArrayState::most_pushed);
  for (std::uint32_t beyond = 0u; beyond < 6u; beyond++) state.push_client(everything);
  for (std::uint32_t beyond = 0u; beyond < 6u; beyond++) {
      state.pop_client();
      CHECK(state.invariants_hold());
  }
  state.pop_client();
  CHECK_EQ(state.bound_buffer(array_buffer), ClientArrayState::most_pushed);
  state.pop_client();
  CHECK_EQ(state.bound_buffer(array_buffer), ClientArrayState::most_pushed - 1u);
  CHECK(state.invariants_hold()); }

TEST(client_array_state_stack, the_unsaved_count_never_exceeds_what_the_host_would_take)
{ MockPlatform     platform;
  ClientArrayState state(platform);
  state.limit_client_depth(ClientArrayState::most_pushed + 2u);
  push_levels(state, ClientArrayState::most_pushed);
  for (std::uint32_t extra = 0u; extra < 50u; extra++) state.push_client(everything);
  state.pop_client();
  state.pop_client();
  state.pop_client();
  CHECK_EQ(state.bound_buffer(array_buffer), ClientArrayState::most_pushed);
  CHECK(state.invariants_hold()); }

TEST(client_array_state_stack, a_whole_unwind_of_a_full_stack_returns_to_the_first_saved_state)
{ MockPlatform     platform;
  ClientArrayState state(platform);
  state.limit_client_depth(1000);
  push_levels(state, ClientArrayState::most_pushed);
  for (std::uint32_t pop = 0u; pop < ClientArrayState::most_pushed; pop++) state.pop_client();
  CHECK_EQ(state.bound_buffer(array_buffer), 1u);
  state.pop_client();
  CHECK_EQ(state.bound_buffer(array_buffer), 1u);
  CHECK(state.invariants_hold()); }

TEST(client_array_state_stack, reset_client_puts_the_masked_state_back_to_its_defaults)
{ MockPlatform     platform;
  ClientArrayState state(platform);
  state.bind_buffer(array_buffer, 5u);
  state.bind_buffer(pack, 6u);
  CHECK(state.store_pixel(pack_alignment, 1));
  state.reset_client(ClientArrayState::client_pixel_store_bit);
  CHECK_EQ(state.bound_buffer(pack), 0u);
  CHECK_EQ(state.pixel_value(pack_alignment), 4);
  CHECK_EQ(state.bound_buffer(array_buffer), 5u);
  state.reset_client(ClientArrayState::client_vertex_array_bit);
  CHECK_EQ(state.bound_buffer(array_buffer), 0u);
  CHECK(state.invariants_hold()); }

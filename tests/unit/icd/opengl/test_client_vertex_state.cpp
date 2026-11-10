#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/opengl/client_vertex_state.hpp"

#include <cstdint>

using av::glicd::ClientVertexState;
using Mode = av::glicd::ClientVertexState::Mode;

namespace av::glicd {

struct ClientVertexStateProbe {
    static void desync(ClientVertexState &state) { state.default_.client_ = 1u; }
    static void break_unit(ClientVertexState &state) { state.unit_ = 99u; }
};

}

namespace {

constexpr std::uint32_t gl_float = 0x1406u, gl_unsigned_byte = 0x1401u, gl_short = 0x1402u;

}

TEST(client_vertex_state, a_client_array_counts_only_while_enabled)
{ av::MockPlatform platform;
  ClientVertexState state(platform);
  CHECK(!state.client_active());
  state.set_pointer(ClientVertexState::vertex_slot, 2, gl_float, false, 0, 0x1000u, Mode::client);
  CHECK(!state.client_active());
  state.set_enabled(ClientVertexState::vertex_slot, true);
  CHECK(state.client_active());
  CHECK_EQ(state.client_slots(), std::uint64_t{1} << ClientVertexState::vertex_slot);
  state.set_pointer(ClientVertexState::vertex_slot, 2, gl_float, false, 0, 0x10u, Mode::buffer);
  CHECK(!state.client_active());
  state.set_host_enabled(ClientVertexState::vertex_slot, true);
  CHECK_EQ(state.host_slots(), std::uint64_t{1} << ClientVertexState::vertex_slot);
  state.clear_slot(ClientVertexState::vertex_slot);
  CHECK_EQ(state.host_slots(), 0u);
  CHECK(state.invariants_hold()); }

TEST(client_vertex_state, every_vertex_array_object_keeps_its_own_arrays)
{ av::MockPlatform platform;
  ClientVertexState state(platform);
  state.set_enabled(ClientVertexState::color_slot, true);
  state.set_pointer(ClientVertexState::color_slot, 3, gl_float, true, 0, 0x2000u, Mode::client);
  CHECK(state.bind(5u) == av::Status::ok);
  CHECK_EQ(state.bound(), 5u);
  CHECK(!state.client_active());
  state.set_enabled(ClientVertexState::normal_slot, true);
  state.set_pointer(ClientVertexState::normal_slot, 3, gl_float, true, 0, 0x3000u, Mode::client);
  CHECK_EQ(state.client_slots(), std::uint64_t{1} << ClientVertexState::normal_slot);
  CHECK(state.bind(0u) == av::Status::ok);
  CHECK_EQ(state.client_slots(), std::uint64_t{1} << ClientVertexState::color_slot);
  CHECK(state.bind(5u) == av::Status::ok);
  const std::uint32_t gone[] = {5u, 0u, 9u};
  state.forget(3, gone);
  CHECK_EQ(state.bound(), 0u);
  CHECK(state.bind(5u) == av::Status::ok);
  CHECK(!state.client_active());
  state.forget(1, nullptr);
  CHECK(state.invariants_hold()); }

TEST(client_vertex_state, capabilities_map_to_slots_and_the_texture_unit_follows_the_client_unit)
{ av::MockPlatform platform;
  ClientVertexState state(platform);
  CHECK_EQ(state.slot_of_capability(0x8074u), ClientVertexState::vertex_slot);
  CHECK_EQ(state.slot_of_capability(0x8075u), ClientVertexState::normal_slot);
  CHECK_EQ(state.slot_of_capability(0x8076u), ClientVertexState::color_slot);
  CHECK_EQ(state.slot_of_capability(0x8077u), ClientVertexState::index_slot);
  CHECK_EQ(state.slot_of_capability(0x8079u), ClientVertexState::edge_flag_slot);
  CHECK_EQ(state.slot_of_capability(0x8457u), ClientVertexState::fog_coord_slot);
  CHECK_EQ(state.slot_of_capability(0x845Eu), ClientVertexState::secondary_color_slot);
  CHECK_EQ(state.slot_of_capability(0x1234u), ClientVertexState::no_slot);
  CHECK_EQ(state.slot_of_capability(0x8078u), ClientVertexState::texcoord_slot);
  CHECK(state.select_texcoord_unit(0x84C3u));
  CHECK_EQ(state.texcoord_unit(), 3u);
  CHECK_EQ(state.slot_of_capability(0x8078u), ClientVertexState::texcoord_slot + 3u);
  CHECK(!state.select_texcoord_unit(0x84C0u + ClientVertexState::texcoord_units));
  CHECK(!state.select_texcoord_unit(0x84BFu));
  CHECK_EQ(state.texcoord_slot_of_unit(2u), ClientVertexState::texcoord_slot + 2u);
  CHECK_EQ(state.texcoord_slot_of_unit(99u), ClientVertexState::texcoord_slot);
  state.host_selected_unit(3u);
  CHECK_EQ(state.host_unit(), 3u); }

TEST(client_vertex_state, pointers_are_checked_against_what_each_array_accepts)
{ av::MockPlatform platform;
  const ClientVertexState state(platform);
  CHECK_EQ(state.validate(ClientVertexState::vertex_slot, 2, gl_float, 0), 0u);
  CHECK_EQ(state.validate(ClientVertexState::vertex_slot, 1, gl_float, 0), 0x0501u);
  CHECK_EQ(state.validate(ClientVertexState::vertex_slot, 5, gl_float, 0), 0x0501u);
  CHECK_EQ(state.validate(ClientVertexState::vertex_slot, 3, gl_unsigned_byte, 0), 0x0500u);
  CHECK_EQ(state.validate(ClientVertexState::vertex_slot, 3, gl_float, -1), 0x0501u);
  CHECK_EQ(state.validate(ClientVertexState::normal_slot, 3, gl_short, 0), 0u);
  CHECK_EQ(state.validate(ClientVertexState::normal_slot, 4, gl_short, 0), 0x0501u);
  CHECK_EQ(state.validate(ClientVertexState::color_slot, 4, gl_unsigned_byte, 0), 0u);
  CHECK_EQ(state.validate(ClientVertexState::color_slot, 0x80E1, gl_unsigned_byte, 0), 0u);
  CHECK_EQ(state.validate(ClientVertexState::secondary_color_slot, 4, gl_float, 0), 0x0501u);
  CHECK_EQ(state.validate(ClientVertexState::index_slot, 1, gl_unsigned_byte, 0), 0u);
  CHECK_EQ(state.validate(ClientVertexState::index_slot, 1, 0x1405u, 0), 0x0500u);
  CHECK_EQ(state.validate(ClientVertexState::fog_coord_slot, 1, gl_float, 0), 0u);
  CHECK_EQ(state.validate(ClientVertexState::fog_coord_slot, 1, gl_short, 0), 0x0500u);
  CHECK_EQ(state.validate(ClientVertexState::edge_flag_slot, 1, gl_unsigned_byte, 0), 0u);
  CHECK_EQ(state.validate(ClientVertexState::texcoord_slot + 1u, 4, gl_short, 0), 0u);
  CHECK_EQ(state.validate(ClientVertexState::texcoord_slot, 2, gl_unsigned_byte, 0), 0x0500u);
  CHECK_EQ(state.validate(2u, 4, gl_unsigned_byte, 0), 0u);
  CHECK_EQ(state.validate(2u, 0x80E1, gl_unsigned_byte, 0), 0u);
  CHECK_EQ(state.validate(ClientVertexState::vertex_slot, 0x80E1, gl_float, 0), 0x0501u);
  CHECK_EQ(state.validate(2u, 5, gl_float, 0), 0x0501u);
  CHECK_EQ(state.validate(2u, 4, 0x140Cu, 0), 0u);
  CHECK_EQ(state.validate(2u, 4, 0x1234u, 0), 0x0500u);
  CHECK_EQ(state.validate(2u, 4, 0x8368u, 0), 0u);
  CHECK_EQ(state.validate(2u, 0x80E1, 0x8D9Fu, 0), 0u);
  CHECK_EQ(state.validate(2u, 3, 0x8C3Bu, 0), 0u);
  CHECK_EQ(state.validate(ClientVertexState::vertex_slot, 4, 0x8368u, 0), 0u);
  CHECK_EQ(state.validate(ClientVertexState::normal_slot, 3, 0x8D9Fu, 0), 0u);
  CHECK_EQ(state.validate(ClientVertexState::color_slot, 4, 0x8368u, 0), 0u); }

TEST(client_vertex_state, the_client_attribute_stack_restores_the_arrays_it_saved)
{ av::MockPlatform platform;
  ClientVertexState state(platform);
  state.set_enabled(ClientVertexState::vertex_slot, true);
  state.set_pointer(ClientVertexState::vertex_slot, 2, gl_float, false, 0, 0x1000u, Mode::client);
  state.push(ClientVertexState::client_vertex_array_bit);
  state.push(1u);
  state.set_pointer(ClientVertexState::vertex_slot, 3, gl_float, false, 0, 0x2000u, Mode::buffer);
  state.set_enabled(ClientVertexState::vertex_slot, false);
  state.pop();
  CHECK(!state.client_active());
  state.pop();
  CHECK(state.client_active());
  CHECK_EQ(state.slot(ClientVertexState::vertex_slot).pointer_, 0x1000u);
  state.pop();
  CHECK(state.client_active());
  state.push(ClientVertexState::client_vertex_array_bit);
  state.reset(1u);
  CHECK(state.client_active());
  state.reset(ClientVertexState::client_vertex_array_bit);
  CHECK(!state.client_active());
  state.pop();
  CHECK(state.client_active());
  for (std::uint32_t at = 0u; at < ClientVertexState::most_pushed + 3u; at++)
    state.push(ClientVertexState::client_vertex_array_bit);
  for (std::uint32_t at = 0u; at < ClientVertexState::most_pushed + 3u; at++) state.pop();
  CHECK(state.invariants_hold()); }

TEST(client_vertex_state, staging_memory_and_the_offset_table_grow_on_request)
{ av::MockPlatform platform;
  ClientVertexState state(platform);
  CHECK(state.staging(0u) == nullptr);
  CHECK(state.staging(64u) != nullptr);
  CHECK(state.staging(std::uint64_t{1} << 31u) == nullptr);
  CHECK(state.offset_table(0u) == nullptr);
  CHECK(state.offset_table(8u) != nullptr);
  CHECK_EQ(state.scratch(), 0u);
  state.set_scratch(9u);
  CHECK_EQ(state.scratch(), 9u);
  state.forget_scratch();
  CHECK_EQ(state.scratch(), 0u);
  CHECK(!state.indexed());
  state.set_indexed(true);
  CHECK(state.indexed()); }

TEST(client_vertex_state, a_broken_state_fails_its_invariants)
{ av::MockPlatform platform;
  ClientVertexState state(platform);
  CHECK(state.invariants_hold());
  av::glicd::ClientVertexStateProbe::desync(state);
  CHECK(!state.invariants_hold());
  ClientVertexState other(platform);
  av::glicd::ClientVertexStateProbe::break_unit(other);
  CHECK(!other.invariants_hold()); }

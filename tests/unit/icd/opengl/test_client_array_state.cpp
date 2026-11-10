#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/opengl/client_array_state.hpp"

#include <cstddef>
#include <cstdint>

using av::glicd::ClientArrayState;

namespace av {

struct KeyedSlotsProbe {
    template <typename Key, typename Entry>
    static void corrupt_count(KeyedSlots<Key, Entry> &table, std::uint32_t count) { table.count_ = count; }
};

}

namespace av::glicd {

using av::KeyedSlotsProbe;

struct PixelStoreProbe {
    static void zero_alignment(PixelStore &store) { store.values_[0] = 0; }
};

struct ClientArrayStateProbe {
    static void set_depth(ClientArrayState &state, std::uint32_t depth) { state.depth_ = depth; }
    static void set_unsaved(ClientArrayState &state, std::uint32_t unsaved) { state.unsaved_ = unsaved; }
    static void corrupt_elements(ClientArrayState &state) { KeyedSlotsProbe::corrupt_count(state.elements_, 99u); }
    static void break_unit(ClientArrayState &state) { state.active_unit_ = 40u; }
    static void name_default(ClientArrayState &state)
    {
        std::uint32_t slot = 0u;
        static_cast<void>(state.elements_.insert(0u, 0u, slot));
    }
    static void break_store(ClientArrayState &state, bool packing)
    {
        PixelStoreProbe::zero_alignment(packing ? state.pack_ : state.unpack_);
    }
};

}

namespace {

constexpr std::uint32_t rgba = 0x1908u, ubyte = 0x1401u;
constexpr std::uint32_t texture_2d = 0x0DE1u, texture_3d = 0x806Fu, texture0 = 0x84C0u;

}

TEST(client_array_state, pixel_stores_are_kept_by_name_for_pack_and_unpack)
{ av::MockPlatform platform;
  ClientArrayState state(platform);
  CHECK(state.invariants_hold());
  CHECK_EQ(state.pixel_bytes(rgba, ubyte, 2, 2, 1, true), 16u);
  const std::uint32_t pack_names[] = {0x0D05u, 0x0D02u, 0x806Cu, 0x0D04u, 0x0D03u, 0x806Bu};
  const std::uint32_t unpack_names[] = {0x0CF5u, 0x0CF2u, 0x806Eu, 0x0CF4u, 0x0CF3u, 0x806Du};
  const std::int32_t  values[] = {1, 3, 0, 1, 1, 0};
  for (std::uint32_t index = 0u; index < 6u; index++) {
      CHECK(state.store_pixel(pack_names[index], values[index]));
      CHECK(state.invariants_hold());
  }
  CHECK_EQ(state.pixel_bytes(rgba, ubyte, 2, 1, 1, true), 12u + 12u);
  CHECK_EQ(state.pixel_bytes(rgba, ubyte, 2, 1, 1, false), 8u);
  for (std::uint32_t index = 0u; index < 6u; index++) CHECK(state.store_pixel(unpack_names[index], values[index]));
  CHECK_EQ(state.pixel_bytes(rgba, ubyte, 2, 1, 1, false), 24u);
  CHECK(!state.store_pixel(0x0D05u, 3));
  CHECK(!state.store_pixel(0x0CF0u , 1));
  CHECK_EQ(state.pixel_bytes(rgba, ubyte, 2, 1, 1, true), 24u);
  CHECK(state.invariants_hold()); }

TEST(client_array_state, a_compressed_upload_is_its_image_size)
{ av::MockPlatform       platform;
  const ClientArrayState state(platform);
  CHECK_EQ(state.compressed_bytes(640, false), 640u);
  CHECK_EQ(state.compressed_bytes(0, false), 0u);
  CHECK_EQ(state.compressed_bytes(-1, false), 0u);
  CHECK_EQ(state.compressed_bytes(640, true), 0u); }

TEST(client_array_state, buffers_are_kept_per_target_and_unbound_when_deleted)
{ av::MockPlatform platform;
  ClientArrayState state(platform);
  CHECK(!state.pixel_buffer_bound(true));
  CHECK(!state.pixel_buffer_bound(false));
  CHECK(!state.query_buffer_bound());
  state.bind_buffer(ClientArrayState::pixel_pack_buffer, 4u);
  state.bind_buffer(ClientArrayState::pixel_unpack_buffer, 5u);
  state.bind_buffer(ClientArrayState::query_buffer, 4u);
  state.bind_buffer(ClientArrayState::array_buffer, 6u);
  state.bind_buffer(0x8A11u , 9u);
  CHECK(state.invariants_hold());
  CHECK(state.pixel_buffer_bound(true));
  CHECK(state.pixel_buffer_bound(false));
  CHECK(state.query_buffer_bound());
  CHECK_EQ(state.bound_buffer(ClientArrayState::array_buffer), 6u);
  CHECK_EQ(state.bound_buffer(0x8A11u), 9u);
  state.bind_buffer(0x0DE1u, 3u);
  CHECK_EQ(state.bound_buffer(0x0DE1u), 0u);
  const std::uint32_t deleted[] = {0u, 4u, 12u};
  state.delete_buffers(3, deleted);
  CHECK(!state.pixel_buffer_bound(true));
  CHECK(!state.query_buffer_bound());
  CHECK(state.pixel_buffer_bound(false));
  state.delete_buffers(2, nullptr);
  state.delete_buffers(-1, deleted);
  CHECK_EQ(state.bound_buffer(ClientArrayState::array_buffer), 6u);
  CHECK(state.invariants_hold()); }

TEST(client_array_state, every_buffer_target_is_kept)
{ constexpr std::uint32_t targets[] = {0x8892u, 0x88EBu, 0x88ECu, 0x9192u, 0x8A11u, 0x8C2Au, 0x8C8Eu,
                                       0x8F36u, 0x8F37u, 0x8F3Fu, 0x90D2u, 0x90EEu, 0x92C0u};
  av::MockPlatform platform;
  ClientArrayState state(platform);
  std::uint32_t    buffer = 20u;
  for (const std::uint32_t target : targets) state.bind_buffer(target, buffer++);
  buffer = 20u;
  for (const std::uint32_t target : targets) CHECK_EQ(state.bound_buffer(target), buffer++);
  CHECK(state.invariants_hold()); }

TEST(client_array_state, a_buffer_binding_the_host_may_have_moved_is_forgotten_and_a_later_bind_is_known_again)
{ av::MockPlatform platform;
  ClientArrayState state(platform);
  constexpr std::uint32_t indexed[] = {0x8A11u, 0x8C8Eu, 0x90D2u, 0x92C0u};
  for (const std::uint32_t target : indexed) {
    state.bind_buffer(target, 5u);
    state.forget_buffer(target);
    CHECK_EQ(state.bound_buffer(target), ClientArrayState::unknown);
  }
  state.bind_buffer(ClientArrayState::pixel_pack_buffer, 6u);
  state.bind_buffer(ClientArrayState::array_buffer, 7u);
  state.forget_buffer(ClientArrayState::pixel_pack_buffer);
  state.forget_buffer(0x0DE1u);
  CHECK_EQ(state.bound_buffer(ClientArrayState::pixel_pack_buffer), ClientArrayState::unknown);
  CHECK_EQ(state.bound_buffer(ClientArrayState::array_buffer), 7u);
  CHECK(!state.tracks(0x0DE1u));
  CHECK_EQ(state.bound_buffer(0x0DE1u), 0u);
  const std::uint32_t deleted[] = {5u};
  state.delete_buffers(1, deleted);
  CHECK_EQ(state.bound_buffer(0x8A11u), ClientArrayState::unknown);
  state.bind_buffer(0x8A11u, 7u);
  CHECK_EQ(state.bound_buffer(0x8A11u), 7u);
  CHECK(state.invariants_hold()); }

TEST(client_array_state, every_target_with_a_row_is_tracked_and_the_element_buffer_follows_its_vertex_array)
{ av::MockPlatform platform;
  ClientArrayState state(platform);
  CHECK_EQ(state.bound_buffer(ClientArrayState::array_buffer), 0u);
  state.bind_buffer(0x8F37u, 4u);
  CHECK(state.tracks(0x8F37u));
  CHECK_EQ(state.bound_buffer(0x8F37u), 4u);
  CHECK(!state.tracks(0x0DE1u));
  state.bind_buffer(0x0DE1u, 9u);
  CHECK_EQ(state.bound_buffer(0x0DE1u), 0u);
  CHECK(state.tracks(ClientArrayState::element_array_buffer));
  state.bind_buffer(ClientArrayState::element_array_buffer, 5u);
  CHECK_EQ(state.bound_buffer(ClientArrayState::element_array_buffer), 5u);
  CHECK_STATUS(state.bind_vertex_array(2u), av::Status::ok);
  CHECK_EQ(state.bound_buffer(ClientArrayState::element_array_buffer), 0u);
  CHECK_STATUS(state.bind_vertex_array(0u), av::Status::ok);
  CHECK_EQ(state.bound_buffer(ClientArrayState::element_array_buffer), 5u);
  state.bind_buffer(0x8A11u, 6u);
  state.forget_buffer(0x8A11u);
  CHECK_EQ(state.bound_buffer(0x8A11u), ClientArrayState::unknown);
  CHECK_EQ(state.bound_buffer(0x8F37u), 4u);
  CHECK(state.invariants_hold()); }

TEST(client_array_state, each_vertex_array_keeps_its_element_buffer)
{ av::MockPlatform platform;
  ClientArrayState state(platform);
  CHECK(!state.indices_are_offset());
  CHECK_EQ(state.bound_buffer(ClientArrayState::element_array_buffer), 0u);
  CHECK_STATUS(state.bind_vertex_array(3u), av::Status::ok);
  CHECK(!state.indices_are_offset());
  state.bind_buffer(ClientArrayState::element_array_buffer, 8u);
  CHECK(state.indices_are_offset());
  CHECK_STATUS(state.bind_vertex_array(4u), av::Status::ok);
  CHECK(!state.indices_are_offset());
  CHECK_STATUS(state.bind_vertex_array(3u), av::Status::ok);
  CHECK_EQ(state.bound_buffer(ClientArrayState::element_array_buffer), 8u);
  CHECK(state.invariants_hold());
  const std::uint32_t buffer[] = {8u};
  state.delete_buffers(1, buffer);
  CHECK(!state.indices_are_offset());
  state.bind_buffer(ClientArrayState::element_array_buffer, 9u);
  const std::uint32_t arrays[] = {0u, 3u, 77u};
  state.delete_vertex_arrays(3, arrays);
  CHECK_EQ(state.bound_buffer(ClientArrayState::element_array_buffer), 0u);
  CHECK_STATUS(state.bind_vertex_array(3u), av::Status::ok);
  CHECK(!state.indices_are_offset());
  state.delete_vertex_arrays(1, nullptr);
  CHECK_STATUS(state.bind_vertex_array(4u), av::Status::ok);
  const std::uint32_t other[] = {3u};
  state.delete_vertex_arrays(1, other);
  CHECK_EQ(state.bound_buffer(ClientArrayState::element_array_buffer), 0u);
  CHECK(state.invariants_hold()); }

TEST(client_array_state, every_vertex_array_keeps_its_element_buffer)
{ av::MockPlatform platform;
  ClientArrayState state(platform);
  for (std::uint32_t array = 1u; array <= 5000u; array++) {
      CHECK_STATUS(state.bind_vertex_array(array), av::Status::ok);
      if (array % 2u == 0u) state.bind_buffer(ClientArrayState::element_array_buffer, array + 100000u);
  }
  CHECK(state.invariants_hold());
  for (std::uint32_t array = 1u; array <= 5000u; array++) {
      CHECK_STATUS(state.bind_vertex_array(array), av::Status::ok);
      CHECK_EQ(state.bound_buffer(ClientArrayState::element_array_buffer), array % 2u == 0u ? array + 100000u : 0u);
  }
  CHECK_STATUS(state.bind_vertex_array(0u), av::Status::ok);
  CHECK_EQ(state.bound_buffer(ClientArrayState::element_array_buffer), 0u);
  platform.fail_allocation_at(1u);
  av::Status bound = av::Status::ok;
  std::uint32_t array = 5001u;
  while (bound == av::Status::ok && array < 20000u) bound = state.bind_vertex_array(array++);
  CHECK_STATUS(bound, av::Status::no_memory);
  CHECK(state.indices_are_offset());
  CHECK_EQ(state.bound_buffer(ClientArrayState::element_array_buffer), 0xFFFFFFFFu);
  state.bind_buffer(ClientArrayState::element_array_buffer, 5u);
  CHECK(state.indices_are_offset());
  CHECK(state.invariants_hold()); }

TEST(client_array_state, a_client_pointer_is_one_with_no_array_buffer)
{ av::MockPlatform platform;
  ClientArrayState state(platform);
  CHECK(!state.client_pointer(0u));
  CHECK(state.client_pointer(0x1000u));
  state.bind_buffer(ClientArrayState::array_buffer, 2u);
  CHECK(!state.client_pointer(0x1000u));
  state.set_attribute_pointer(3u, 0x40u);
  state.set_attribute_pointer(ClientArrayState::max_attributes, 0x80u);
  CHECK_EQ(state.attribute_pointer(3u), 0x40u);
  CHECK_EQ(state.attribute_pointer(4u), 0u);
  CHECK_EQ(state.attribute_pointer(ClientArrayState::max_attributes), 0u); }

TEST(client_array_state, multi_draw_offsets_are_the_pointers_values)
{ av::MockPlatform platform;
  ClientArrayState state(platform);
  const void *const pointers[] = {nullptr, reinterpret_cast<const void *>(0x10u),
                                  reinterpret_cast<const void *>(0xFFFFFFFFFFFFFFF0ull)};
  const std::uint64_t *const offsets = state.offsets_of(pointers, 3u);
  CHECK(offsets != nullptr);
  CHECK_EQ(offsets[0], 0u);
  CHECK_EQ(offsets[1], 0x10u);
  CHECK_EQ(offsets[2], 0xFFFFFFFFFFFFFFF0ull);
  CHECK(state.offsets_of(nullptr, 0u) != nullptr);
  CHECK(state.offsets_of(nullptr, 1u) == nullptr);
  CHECK(state.offsets_of(pointers, static_cast<std::size_t>(-1)) == nullptr); }

TEST(client_array_state, multi_draw_offsets_go_past_the_old_bound)
{ av::MockPlatform platform;
  ClientArrayState state(platform);
  constexpr std::size_t many = 5000u;
  static const void *pointers[many];
  for (std::size_t index = 0u; index < many; index++) pointers[index] = reinterpret_cast<const void *>(index * 4u);
  const std::uint64_t *const offsets = state.offsets_of(pointers, many);
  CHECK(offsets != nullptr);
  CHECK_EQ(offsets[4999], 4999u * 4u);
  platform.fail_allocation_at(1u);
  static const void *more[many * 4u];
  CHECK(state.offsets_of(more, many * 4u) == nullptr); }

TEST(client_array_state, a_bind_repeating_the_last_one_is_skipped)
{ av::MockPlatform platform;
  ClientArrayState state(platform);
  CHECK(!state.skip_bind_texture(texture_2d, 1u));
  CHECK(!state.skip_active_texture(texture0 + 1u));
  CHECK(state.skip_active_texture(texture0 + 1u));
  CHECK(!state.skip_bind_texture(texture_2d, 1u));
  CHECK(state.skip_bind_texture(texture_2d, 1u));
  CHECK(!state.skip_bind_texture(texture_3d, 1u));
  CHECK(!state.skip_bind_texture(texture_2d, 2u));
  CHECK(!state.skip_bind_texture(0x1234u, 2u));
  CHECK(!state.skip_active_texture(texture0));
  CHECK(!state.skip_bind_texture(texture_2d, 2u));
  CHECK(!state.skip_active_texture(texture0 + 32u));
  CHECK(!state.skip_active_texture(0x1000u));
  CHECK(!state.skip_bind_sampler(2u, 5u));
  CHECK(state.skip_bind_sampler(2u, 5u));
  CHECK(!state.skip_bind_sampler(32u, 5u));
  CHECK(!state.skip_bind_sampler(32u, 5u));
  CHECK(state.invariants_hold());
  state.forget_bindings();
  CHECK(!state.skip_bind_sampler(2u, 5u));
  CHECK(!state.skip_bind_texture(texture_2d, 2u));
  CHECK(!state.skip_active_texture(texture0));
  CHECK(state.invariants_hold()); }

TEST(client_array_state_invariants, fail_for_state_no_call_makes)
{ av::MockPlatform platform;
  ClientArrayState unit(platform);
  av::glicd::ClientArrayStateProbe::break_unit(unit);
  CHECK(!unit.invariants_hold());
  ClientArrayState none(platform);
  av::glicd::ClientArrayStateProbe::name_default(none);
  CHECK(!none.invariants_hold());
  ClientArrayState pack(platform);
  av::glicd::ClientArrayStateProbe::break_store(pack, true);
  CHECK(!pack.invariants_hold());
  ClientArrayState unpack(platform);
  av::glicd::ClientArrayStateProbe::break_store(unpack, false);
  CHECK(!unpack.invariants_hold());
  ClientArrayState deep(platform);
  av::glicd::ClientArrayStateProbe::set_depth(deep, ClientArrayState::most_pushed + 1u);
  CHECK(!deep.invariants_hold());
  ClientArrayState unsaved(platform);
  av::glicd::ClientArrayStateProbe::set_unsaved(unsaved, 1u);
  CHECK(!unsaved.invariants_hold());
  av::glicd::ClientArrayStateProbe::set_depth(unsaved, ClientArrayState::most_pushed);
  CHECK(unsaved.invariants_hold());
  ClientArrayState table(platform);
  CHECK_STATUS(table.bind_vertex_array(3u), av::Status::ok);
  CHECK(table.invariants_hold());
  av::glicd::ClientArrayStateProbe::corrupt_elements(table);
  CHECK(!table.invariants_hold()); }

TEST(client_array_state, answers_the_pixel_store_values_it_keeps)
{ av::MockPlatform platform;
  ClientArrayState state(platform);
  CHECK_EQ(state.pixel_value(0x0D05u), 4);
  CHECK_EQ(state.pixel_value(0x0CF5u), 4);
  CHECK(state.store_pixel(0x0D05u, 8));
  CHECK(state.store_pixel(0x0D04u, 3));
  CHECK_EQ(state.pixel_value(0x0D05u), 8);
  CHECK_EQ(state.pixel_value(0x0D04u), 3);
  CHECK_EQ(state.pixel_value(0x0CF5u), 4);
  CHECK_EQ(state.pixel_value(0x1234u), 0);
  CHECK(state.invariants_hold()); }

TEST(client_array_state, an_element_buffer_set_for_a_vertex_array_is_the_one_bound_while_it_is_bound)
{ av::MockPlatform platform;
  ClientArrayState state(platform);
  CHECK_STATUS(state.bind_vertex_array(3u), av::Status::ok);
  state.set_vertex_array_elements(3u, 8u);
  CHECK_EQ(state.bound_buffer(ClientArrayState::element_array_buffer), 8u);
  CHECK(state.indices_are_offset());
  state.set_vertex_array_elements(0u, 5u);
  CHECK_EQ(state.bound_buffer(ClientArrayState::element_array_buffer), 8u);
  state.set_vertex_array_elements(3u, ClientArrayState::unknown);
  CHECK_EQ(state.bound_buffer(ClientArrayState::element_array_buffer), ClientArrayState::unknown);
  state.set_vertex_array_elements(3u, 0u);
  CHECK(!state.indices_are_offset());
  CHECK_STATUS(state.bind_vertex_array(0u), av::Status::ok);
  CHECK_EQ(state.bound_buffer(ClientArrayState::element_array_buffer), 5u);
  state.set_vertex_array_elements(99u, 7u);
  CHECK_STATUS(state.bind_vertex_array(99u), av::Status::ok);
  CHECK_EQ(state.bound_buffer(ClientArrayState::element_array_buffer), 0u);
  CHECK(state.invariants_hold()); }

TEST(client_array_state, deleting_the_buffer_the_default_vertex_array_uses_unbinds_it)
{ av::MockPlatform platform;
  ClientArrayState state(platform);
  state.bind_buffer(ClientArrayState::element_array_buffer, 7u);
  CHECK_EQ(state.bound_buffer(ClientArrayState::element_array_buffer), 7u);
  const std::uint32_t other = 8u;
  state.delete_buffers(1, &other);
  CHECK_EQ(state.bound_buffer(ClientArrayState::element_array_buffer), 7u);
  const std::uint32_t names[] = {7u};
  state.delete_buffers(1, names);
  CHECK_EQ(state.bound_buffer(ClientArrayState::element_array_buffer), 0u);
  CHECK(state.invariants_hold()); }

TEST(client_array_state, pointer_sized_values_are_widened_for_the_wire)
{ av::MockPlatform platform;
  ClientArrayState state(platform);
  const std::intptr_t values[3] = {-1, 8, 16};
  const std::int64_t *wide = state.widened(values, 3u, 0u);
  CHECK(wide != nullptr);
  CHECK_EQ(wide[0], -1);
  CHECK_EQ(wide[2], 16);
  CHECK(state.widened(nullptr, 3u, 0u) == nullptr);
  CHECK(state.widened(values, 0u, 1u) != nullptr);
  CHECK(state.widened(values, 3u, 5u) != nullptr);
  CHECK(state.widened(values, 3u, 1u) != nullptr); }

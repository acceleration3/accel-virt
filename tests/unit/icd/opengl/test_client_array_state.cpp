/* What a context remembers between calls that shapes the next one: pixel
 * stores by name, the buffers a transfer or draw reads, vertex arrays' element
 * buffers, attribute pointers, multi-draw offsets and redundant binds. */
#include "harness/framework/test_registry.hpp"
#include "icd/opengl/client_array_state.hpp"

#include <cstddef>
#include <cstdint>

using av::glicd::ClientArrayState;

namespace av::glicd {

struct PixelStoreProbe {
    static void zero_alignment(PixelStore &store) { store.values_[0] = 0; }
};

struct ClientArrayStateProbe {
    static void break_unit(ClientArrayState &state) { state.active_unit_ = 40u; }
    static void break_arrays(ClientArrayState &state) { state.arrays_known_ = 0u; }
    static void overfill_arrays(ClientArrayState &state) { state.arrays_known_ = ClientArrayState::max_vertex_arrays + 1u; }
    static void rename_default(ClientArrayState &state) { state.array_names_[0] = 5u; }
    static void repeat_array(ClientArrayState &state)
    {
        state.array_names_[1] = 7u;
        state.array_names_[2] = 7u;
        state.arrays_known_ = 3u;
    }
    static void break_store(ClientArrayState &state, bool packing)
    {
        PixelStoreProbe::zero_alignment(packing ? state.pack_ : state.unpack_);
    }
};

}  // namespace av::glicd

namespace {

constexpr std::uint32_t rgba = 0x1908u, ubyte = 0x1401u;
constexpr std::uint32_t texture_2d = 0x0DE1u, texture_3d = 0x806Fu, texture0 = 0x84C0u;

}  // namespace

TEST(client_array_state, pixel_stores_are_kept_by_name_for_pack_and_unpack)
{ ClientArrayState state;
  CHECK(state.invariants_hold());
  /* Four bytes per pixel, alignment 4: 2x2 is 8 + 8. */
  CHECK_EQ(state.pixel_bytes(rgba, ubyte, 2, 2, 1, true), 16u);
  const std::uint32_t pack_names[] = {0x0D05u, 0x0D02u, 0x806Cu, 0x0D04u, 0x0D03u, 0x806Bu};
  const std::uint32_t unpack_names[] = {0x0CF5u, 0x0CF2u, 0x806Eu, 0x0CF4u, 0x0CF3u, 0x806Du};
  const std::int32_t  values[] = {1, 3, 0, 1, 1, 0};
  for (std::uint32_t index = 0u; index < 6u; index++) {
      CHECK(state.store_pixel(pack_names[index], values[index]));
      CHECK(state.invariants_hold());
  }
  /* Row length 3 and one skipped row and pixel: 3*4 + (1 + 2) * 4. */
  CHECK_EQ(state.pixel_bytes(rgba, ubyte, 2, 1, 1, true), 12u + 12u);
  CHECK_EQ(state.pixel_bytes(rgba, ubyte, 2, 1, 1, false), 8u);
  for (std::uint32_t index = 0u; index < 6u; index++) CHECK(state.store_pixel(unpack_names[index], values[index]));
  CHECK_EQ(state.pixel_bytes(rgba, ubyte, 2, 1, 1, false), 24u);
  CHECK(!state.store_pixel(0x0D05u, 3));
  CHECK(!state.store_pixel(0x0CF0u /* GL_UNPACK_SWAP_BYTES */, 1));
  CHECK_EQ(state.pixel_bytes(rgba, ubyte, 2, 1, 1, true), 24u);
  CHECK(state.invariants_hold()); }

TEST(client_array_state, a_compressed_upload_is_its_image_size)
{ const ClientArrayState state;
  CHECK_EQ(state.compressed_bytes(640, false), 640u);
  CHECK_EQ(state.compressed_bytes(0, false), 0u);
  CHECK_EQ(state.compressed_bytes(-1, false), 0u);
  CHECK_EQ(state.compressed_bytes(640, true), 0u); }

TEST(client_array_state, buffers_are_kept_per_target_and_unbound_when_deleted)
{ ClientArrayState state;
  CHECK(!state.pixel_buffer_bound(true));
  CHECK(!state.pixel_buffer_bound(false));
  CHECK(!state.query_buffer_bound());
  state.bind_buffer(ClientArrayState::pixel_pack_buffer, 4u);
  state.bind_buffer(ClientArrayState::pixel_unpack_buffer, 5u);
  state.bind_buffer(ClientArrayState::query_buffer, 4u);
  state.bind_buffer(ClientArrayState::array_buffer, 6u);
  state.bind_buffer(0x8A11u /* GL_UNIFORM_BUFFER */, 9u);
  CHECK(state.invariants_hold());
  CHECK(state.pixel_buffer_bound(true));
  CHECK(state.pixel_buffer_bound(false));
  CHECK(state.query_buffer_bound());
  CHECK_EQ(state.bound_buffer(ClientArrayState::array_buffer), 6u);
  CHECK_EQ(state.bound_buffer(0x8A11u), 0u);
  const std::uint32_t deleted[] = {0u, 4u, 12u};
  state.delete_buffers(3, deleted);
  CHECK(!state.pixel_buffer_bound(true));
  CHECK(!state.query_buffer_bound());
  CHECK(state.pixel_buffer_bound(false));
  state.delete_buffers(2, nullptr);
  state.delete_buffers(-1, deleted);
  CHECK_EQ(state.bound_buffer(ClientArrayState::array_buffer), 6u);
  CHECK(state.invariants_hold()); }

TEST(client_array_state, each_vertex_array_keeps_its_element_buffer)
{ ClientArrayState state;
  /* Array 0 starts known, with nothing bound. */
  CHECK(!state.indices_are_offset());
  CHECK_EQ(state.bound_buffer(ClientArrayState::element_array_buffer), 0u);
  state.bind_vertex_array(3u);
  CHECK(!state.indices_are_offset());
  state.bind_buffer(ClientArrayState::element_array_buffer, 8u);
  CHECK(state.indices_are_offset());
  state.bind_vertex_array(4u);
  CHECK(!state.indices_are_offset());
  state.bind_vertex_array(3u);
  CHECK_EQ(state.bound_buffer(ClientArrayState::element_array_buffer), 8u);
  CHECK(state.invariants_hold());
  /* Deleting the element buffer unbinds it from the array that held it. */
  const std::uint32_t buffer[] = {8u};
  state.delete_buffers(1, buffer);
  CHECK(!state.indices_are_offset());
  state.bind_buffer(ClientArrayState::element_array_buffer, 9u);
  /* Deleting the bound array binds 0; array 0 and unknown names stay. */
  const std::uint32_t arrays[] = {0u, 3u, 77u};
  state.delete_vertex_arrays(3, arrays);
  CHECK_EQ(state.bound_buffer(ClientArrayState::element_array_buffer), 0u);
  state.bind_vertex_array(3u);
  CHECK(!state.indices_are_offset());
  state.delete_vertex_arrays(1, nullptr);
  state.bind_vertex_array(4u);
  const std::uint32_t other[] = {3u};
  state.delete_vertex_arrays(1, other);
  CHECK_EQ(state.bound_buffer(ClientArrayState::element_array_buffer), 0u);
  CHECK(state.invariants_hold()); }

TEST(client_array_state, past_the_arrays_it_keeps_a_draw_takes_its_indices_as_an_offset)
{ ClientArrayState state;
  for (std::uint32_t array = 1u; array < ClientArrayState::max_vertex_arrays; array++) state.bind_vertex_array(array);
  CHECK(state.invariants_hold());
  state.bind_vertex_array(1000u);
  CHECK(state.indices_are_offset());
  CHECK_EQ(state.bound_buffer(ClientArrayState::element_array_buffer), 0xFFFFFFFFu);
  state.bind_buffer(ClientArrayState::element_array_buffer, 5u);
  CHECK(state.indices_are_offset());
  state.bind_vertex_array(ClientArrayState::max_vertex_arrays - 1u);
  CHECK(!state.indices_are_offset()); }

TEST(client_array_state, a_client_pointer_is_one_with_no_array_buffer)
{ ClientArrayState state;
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
{ ClientArrayState state;
  const void *const pointers[] = {nullptr, reinterpret_cast<const void *>(0x10u),
                                  reinterpret_cast<const void *>(0xFFFFFFFFFFFFFFF0ull)};
  const std::uint64_t *const offsets = state.offsets_of(pointers, 3u);
  CHECK(offsets != nullptr);
  CHECK_EQ(offsets[0], 0u);
  CHECK_EQ(offsets[1], 0x10u);
  CHECK_EQ(offsets[2], 0xFFFFFFFFFFFFFFF0ull);
  CHECK(state.offsets_of(nullptr, 0u) != nullptr);
  CHECK(state.offsets_of(nullptr, 1u) == nullptr);
  CHECK(state.offsets_of(pointers, ClientArrayState::max_offsets + 1u) == nullptr); }

TEST(client_array_state, a_bind_repeating_the_last_one_is_skipped)
{ ClientArrayState state;
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
{ ClientArrayState unit;
  av::glicd::ClientArrayStateProbe::break_unit(unit);
  CHECK(!unit.invariants_hold());
  ClientArrayState none;
  av::glicd::ClientArrayStateProbe::break_arrays(none);
  CHECK(!none.invariants_hold());
  ClientArrayState over;
  av::glicd::ClientArrayStateProbe::overfill_arrays(over);
  CHECK(!over.invariants_hold());
  ClientArrayState renamed;
  av::glicd::ClientArrayStateProbe::rename_default(renamed);
  CHECK(!renamed.invariants_hold());
  ClientArrayState repeated;
  av::glicd::ClientArrayStateProbe::repeat_array(repeated);
  CHECK(!repeated.invariants_hold());
  ClientArrayState pack;
  av::glicd::ClientArrayStateProbe::break_store(pack, true);
  CHECK(!pack.invariants_hold());
  ClientArrayState unpack;
  av::glicd::ClientArrayStateProbe::break_store(unpack, false);
  CHECK(!unpack.invariants_hold()); }

TEST(client_array_state, answers_the_pixel_store_values_it_keeps)
{ ClientArrayState state;
  CHECK_EQ(state.pixel_value(0x0D05u), 4);
  CHECK_EQ(state.pixel_value(0x0CF5u), 4);
  CHECK(state.store_pixel(0x0D05u, 8));
  CHECK(state.store_pixel(0x0D04u, 3));
  CHECK_EQ(state.pixel_value(0x0D05u), 8);
  CHECK_EQ(state.pixel_value(0x0D04u), 3);
  CHECK_EQ(state.pixel_value(0x0CF5u), 4);
  CHECK_EQ(state.pixel_value(0x1234u), 0);
  CHECK(state.invariants_hold()); }

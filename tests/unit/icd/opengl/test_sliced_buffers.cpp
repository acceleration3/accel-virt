#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/gl_context_rig.hpp"
#include "harness/icd/scripted_gpu_timeline.hpp"
#include "icd/common/host_channel.hpp"
#include "proto/context/sub_api.hpp"
#include "icd/opengl/buffer_mapping.hpp"
#include "icd/opengl/guest_gl_context.hpp"
#include "icd/opengl/slice_pin.hpp"
#include "icd/opengl/sliced_buffers.hpp"
#include "proto/generated/glmem/glmem_protocol_ids.h"
#include "proto/submission/command_header.hpp"

#include "gl_dispatch_client.h"

#include <cstdint>
#include <cstring>

using av::GlContextOptions;
using av::GlContextRig;
using av::MockKmt;
using av::ScriptedGpuTimeline;
using av::Status;
using av::glicd::BufferMapping;
using av::glicd::GuestGlContext;
using av::glicd::SlicePin;
using av::glicd::SlicedBuffers;

namespace {

constexpr std::uint32_t storage = BufferMapping::map_persistent | BufferMapping::map_write | BufferMapping::map_read;
constexpr std::uint32_t coherent = SlicedBuffers::gl_map_coherent_bit;
constexpr std::uint32_t dynamic_storage = SlicedBuffers::gl_dynamic_storage_bit;
constexpr std::uint32_t write_only = BufferMapping::map_write;
constexpr std::uint32_t no_error = SlicedBuffers::gl_no_error;
constexpr std::uint32_t invalid_value = SlicedBuffers::gl_invalid_value;
constexpr std::uint32_t invalid_operation = SlicedBuffers::gl_invalid_operation;
constexpr std::uint64_t block_memory = 0x0000700000000100ull;
constexpr std::uint32_t bytes = 4096u;
constexpr std::uint32_t gl_r8 = 0x8229u, gl_red = 0x1903u, gl_unsigned_byte = 0x1401u;

struct Rig {
    GlContextRig   context_rig;
    GuestGlContext context{context_rig.connection(), context_rig.platform(), context_rig.send_lock(),
                           context_rig.groups(), context_rig.lines(), context_rig.extensions(), context_rig.handles(), 1,
                           GlContextOptions()};
    std::uint64_t  next_block = block_memory;
    std::uint8_t   pattern[bytes];

    Rig(bool sliced = true)
    {
        for (std::uint32_t at = 0u; at < bytes; at++) pattern[at] = static_cast<std::uint8_t>(at * 7u + 1u);
        context_rig.host().gl_make("renderer", "4.6");
        context_rig.host().gl_slices(sliced ? 1u : 0u);
        CHECK_STATUS(context.make(), Status::ok);
    }

    av::ScriptedHost &host() { return context_rig.host(); }
    SlicedBuffers    &slices() { return context.slices(); }

    Status place(std::uint32_t buffer, std::uint32_t flags = storage, bool opens = true, const void *data = nullptr,
                 std::uint32_t size = bytes)
    {
        if (opens) host().blob_import(0u, next_block++);
        host().gl_slices(1u);
        return slices().place(*context.wire(), buffer, size, flags, data);
    }

    std::uint32_t forgotten()
    {
        context.flush();
        std::uint32_t count = 0u;
        for (std::uint32_t at = 0u; at < context_rig.kmt().commands(); at++)
            count += context_rig.kmt().command_id(at) == av::glmemproto::kCmd_ForgetBufferSlice ? 1u : 0u;
        return count;
    }
};

}

TEST(sliced_buffers, only_a_granted_unstopped_buffer_with_persistent_mappable_storage_is_offered)
{ Rig rig;
  SlicedBuffers &slices = rig.slices();
  CHECK(slices.offers(bytes, storage));
  CHECK(slices.offers(bytes, BufferMapping::map_persistent | BufferMapping::map_write));
  CHECK(!slices.offers(0, storage));
  CHECK(!slices.offers(-4, storage));
  CHECK(!slices.offers(static_cast<std::int64_t>(SlicedBuffers::most_bytes) + 1, storage));
  CHECK(slices.offers(static_cast<std::int64_t>(SlicedBuffers::most_bytes), storage));
  CHECK(!slices.offers(bytes, BufferMapping::map_write));
  CHECK(!slices.offers(bytes, BufferMapping::map_persistent));
  CHECK(!slices.offers(bytes, storage | dynamic_storage));
  CHECK(!slices.offers(bytes, storage | 0x4000u));
  CHECK(!slices.offers(bytes, write_only | coherent));
  Rig unsliced(false);
  CHECK(!unsliced.slices().offers(bytes, storage));
  CHECK_EQ(unsliced.place(1u), Status::not_supported); }

TEST(sliced_buffers, a_context_that_stopped_placing_offers_and_places_nothing_more_but_keeps_what_it_holds)
{ Rig rig;
  CHECK_EQ(rig.place(1u), Status::ok);
  CHECK(rig.slices().holds(1u));
  rig.slices().stop_placing();
  CHECK(!rig.slices().offers(bytes, storage));
  CHECK_EQ(rig.place(2u, storage, false), Status::not_supported);
  CHECK(!rig.slices().holds(2u));
  CHECK(rig.slices().holds(1u));
  CHECK_EQ(rig.slices().owned(), 1u);
  CHECK(rig.slices().invariants_hold()); }

TEST(sliced_buffers, a_placed_buffer_is_held_owned_and_seeded_with_its_data)
{ Rig rig;
  CHECK_EQ(rig.place(5u, storage, true, rig.pattern), Status::ok);
  CHECK(rig.slices().holds(5u));
  CHECK_EQ(rig.slices().held(), 1u);
  CHECK_EQ(rig.slices().owned(), 1u);
  std::uint8_t seen[bytes] = {};
  CHECK_EQ(rig.slices().read(5u, 0, bytes, seen), no_error);
  CHECK(std::memcmp(seen, rig.pattern, bytes) == 0);
  CHECK_EQ(rig.place(5u, storage, false), Status::conflict);
  CHECK_EQ(rig.place(0u, storage, false), Status::conflict);
  CHECK(rig.slices().invariants_hold()); }

TEST(sliced_buffers, an_unpack_source_is_the_slices_own_bytes_at_the_offset_and_only_inside_the_buffer)
{ Rig rig;
  CHECK_EQ(rig.place(1u, storage, true, rig.pattern), Status::ok);
  const std::uint8_t *data = nullptr;
  CHECK_EQ(rig.slices().unpack_source(1u, 16u, 32u, data), no_error);
  CHECK(data != nullptr);
  CHECK(std::memcmp(data, rig.pattern + 16u, 32u) == 0);
  CHECK_EQ(rig.slices().unpack_source(1u, bytes - 8u, 8u, data), no_error);
  CHECK_EQ(rig.slices().unpack_source(1u, bytes - 8u, 9u, data), invalid_operation);
  CHECK(data == nullptr);
  CHECK_EQ(rig.slices().unpack_source(1u, bytes + 1u, 0u, data), invalid_operation);
  CHECK_EQ(rig.slices().unpack_source(9u, 0u, 1u, data), invalid_operation);
  CHECK_EQ(rig.slices().pinned(), 0u);
  CHECK(rig.slices().invariants_hold()); }

TEST(sliced_buffers, a_pack_target_is_the_slices_own_bytes_so_what_is_written_there_is_what_a_read_returns)
{ Rig rig;
  CHECK_EQ(rig.place(1u), Status::ok);
  std::uint8_t *target = nullptr;
  CHECK_EQ(rig.slices().pack_target(1u, 64u, 8u, target), no_error);
  CHECK(target != nullptr);
  for (std::uint32_t at = 0u; at < 8u; at++) target[at] = static_cast<std::uint8_t>(0xA0u + at);
  std::uint8_t seen[8] = {};
  CHECK_EQ(rig.slices().read(1u, 64, 8, seen), no_error);
  for (std::uint32_t at = 0u; at < 8u; at++) CHECK_EQ(seen[at], 0xA0u + at);
  CHECK_EQ(rig.slices().pack_target(1u, bytes, 1u, target), invalid_operation);
  CHECK(target == nullptr);
  CHECK_EQ(rig.slices().pack_target(2u, 0u, 1u, target), invalid_operation);
  CHECK(rig.slices().invariants_hold()); }

TEST(sliced_buffers, packing_into_or_unpacking_from_a_buffer_mapped_without_persistence_is_refused)
{ Rig rig;
  CHECK_EQ(rig.place(1u), Status::ok);
  void *pointer = nullptr;
  CHECK_EQ(rig.slices().map(1u, 0, 128, BufferMapping::map_write, pointer), no_error);
  const std::uint8_t *source = nullptr;
  std::uint8_t       *target = nullptr;
  CHECK_EQ(rig.slices().unpack_source(1u, 0u, 4u, source), invalid_operation);
  CHECK_EQ(rig.slices().pack_target(1u, 0u, 4u, target), invalid_operation);
  CHECK_EQ(rig.slices().unmap(1u), no_error);
  CHECK_EQ(rig.slices().unpack_source(1u, 0u, 4u, source), no_error);
  CHECK_EQ(rig.slices().pack_target(1u, 0u, 4u, target), no_error); }

TEST(sliced_buffers, clearing_all_of_a_buffer_fills_it_with_the_pattern_and_refuses_what_it_cannot_convert)
{ Rig rig;
  CHECK_EQ(rig.place(1u, storage, true, rig.pattern), Status::ok);
  const std::uint8_t fill = 0x7Fu;
  CHECK_EQ(rig.slices().clear_all(1u, gl_r8, gl_red, gl_unsigned_byte, &fill), no_error);
  std::uint8_t seen[bytes] = {};
  CHECK_EQ(rig.slices().read(1u, 0, bytes, seen), no_error);
  for (std::uint32_t at = 0u; at < bytes; at++) CHECK_EQ(seen[at], 0x7Fu);
  CHECK_EQ(rig.slices().clear_all(9u, gl_r8, gl_red, gl_unsigned_byte, &fill), invalid_operation);
  CHECK_EQ(rig.slices().clear_all(1u, 0u, gl_red, gl_unsigned_byte, &fill), 0x0500u);
  CHECK_EQ(rig.slices().read(1u, 0, 1, &seen[0]), no_error);
  CHECK_EQ(seen[0], 0x7Fu);
  void *pointer = nullptr;
  CHECK_EQ(rig.slices().map(1u, 0, 16, BufferMapping::map_write, pointer), no_error);
  CHECK_EQ(rig.slices().clear_all(1u, gl_r8, gl_red, gl_unsigned_byte, &fill), invalid_operation);
  CHECK(rig.slices().invariants_hold()); }

TEST(sliced_buffers, invalidating_a_range_is_judged_against_the_buffer_and_any_mapping_that_is_not_persistent)
{ Rig rig;
  CHECK_EQ(rig.place(1u), Status::ok);
  CHECK_EQ(rig.slices().invalidate(9u, 0, 4), invalid_operation);
  CHECK_EQ(rig.slices().invalidate(1u, -1, 4), invalid_value);
  CHECK_EQ(rig.slices().invalidate(1u, 0, -1), invalid_value);
  CHECK_EQ(rig.slices().invalidate(1u, bytes - 4, 8), invalid_value);
  CHECK_EQ(rig.slices().invalidate(1u, 0, bytes), no_error);
  void *pointer = nullptr;
  CHECK_EQ(rig.slices().map(1u, 0, 128, BufferMapping::map_write, pointer), no_error);
  CHECK_EQ(rig.slices().invalidate(1u, 64, 16), invalid_operation);
  CHECK_EQ(rig.slices().invalidate(1u, 128, 16), no_error);
  CHECK_EQ(rig.slices().invalidate(1u, 64, 0), no_error);
  CHECK_EQ(rig.slices().unmap(1u), no_error);
  CHECK_EQ(rig.slices().map(1u, 0, 128, BufferMapping::map_write | BufferMapping::map_persistent, pointer), no_error);
  CHECK_EQ(rig.slices().invalidate(1u, 64, 16), no_error); }

TEST(sliced_buffers, waiting_for_what_is_recorded_waits_for_the_gpu_to_pass_the_batch_still_open)
{ Rig rig;
  av::glproto::av_glFlush(*rig.context.wire());
  rig.context.flush();
  const std::uint32_t waits = rig.context_rig.kmt().calls(MockKmt::Call::wait_for_context);
  CHECK(rig.slices().wait_recorded());
  CHECK(rig.context_rig.kmt().calls(MockKmt::Call::wait_for_context) >= waits);
  CHECK(rig.slices().completed() >= 1u);
  CHECK(rig.slices().covering() >= 1u); }

namespace {

class Sibling {
public:
    explicit Sibling(Rig &rig)
        : channel_(rig.context_rig.platform(), rig.context_rig.lines(),
                   av::icd::ChannelShape(av::SubApi::opengl, 12u, 4096u, 4096u, 0u, 0u), rig.context_rig.send_lock()),
          timeline_(),
          buffers_(rig.context_rig.platform(), channel_, timeline_, rig.context_rig.groups(), rig.context.slice_group(),
                   rig.context_rig.lines(), this)
    {
        CHECK_STATUS(buffers_.enroll(), Status::ok);
    }
    ~Sibling() { buffers_.release(); }

    av::icd::HostChannel channel_;
    ScriptedGpuTimeline  timeline_;
    SlicedBuffers        buffers_;
};

}

TEST(sliced_buffers, a_buffer_its_owner_forgets_is_forgotten_on_the_host_at_once)
{ Rig rig;
  CHECK_EQ(rig.place(1u), Status::ok);
  const std::uint32_t buffers[] = {1u};
  rig.slices().forget(1, buffers);
  CHECK(!rig.slices().holds(1u));
  CHECK_EQ(rig.slices().owned(), 0u);
  CHECK_EQ(rig.forgotten(), 1u);
  rig.slices().send_forgets();
  CHECK_EQ(rig.forgotten(), 1u);
  CHECK(rig.slices().invariants_hold()); }

TEST(sliced_buffers, a_buffer_another_member_forgets_is_told_to_the_host_once_by_its_owner)
{ Rig rig;
  Sibling sibling(rig);
  CHECK_EQ(rig.place(1u), Status::ok);
  const std::uint32_t buffers[] = {1u};
  sibling.buffers_.forget(1, buffers);
  CHECK(!rig.slices().holds(1u));
  CHECK(!sibling.buffers_.holds(1u));
  rig.slices().send_forgets();
  CHECK_EQ(rig.forgotten(), 1u);
  rig.slices().send_forgets();
  CHECK_EQ(rig.forgotten(), 1u);
  CHECK(rig.slices().invariants_hold()); }

TEST(sliced_buffers, a_slice_another_member_forgot_is_given_back_only_once_every_member_has_passed_its_work)
{ Rig rig;
  Sibling sibling(rig);
  sibling.timeline_.send();
  sibling.timeline_.send();
  CHECK_EQ(rig.place(1u), Status::ok);
  const std::uint32_t buffers[] = {1u};
  sibling.buffers_.forget(1, buffers);
  CHECK_EQ(rig.slices().owned(), 0u);
  CHECK_EQ(rig.slices().deferred(), 1u);
  CHECK_EQ(rig.slices().busy(), 1u);
  rig.slices().reclaim_deferred();
  CHECK_EQ(rig.slices().deferred(), 1u);
  sibling.timeline_.finish(1u);
  rig.slices().reclaim_deferred();
  CHECK_EQ(rig.slices().deferred(), 1u);
  sibling.timeline_.finish(2u);
  rig.slices().reclaim_deferred();
  CHECK_EQ(rig.slices().deferred(), 0u);
  CHECK_EQ(rig.slices().busy(), 0u);
  CHECK(rig.slices().invariants_hold()); }

TEST(guest_gl_context_slices, the_context_answers_for_its_group_and_what_its_buffers_owe)
{ Rig rig;
  Sibling sibling(rig);
  CHECK_EQ(rig.context.slice_group(), rig.slices().group());
  CHECK_EQ(sibling.buffers_.group(), rig.context.slice_group());
  CHECK_EQ(rig.context.slices_owned(), 0u);
  CHECK_EQ(rig.context.slices_busy(), 0u);
  sibling.timeline_.send();
  CHECK_EQ(rig.place(1u, storage, true, rig.pattern), Status::ok);
  CHECK_EQ(rig.context.slices_owned(), 1u);
  CHECK_EQ(rig.context.slices_busy(), 1u);
  CHECK_EQ(rig.context.slices_pinned(), 0u);
  CHECK_EQ(rig.context.slices_deferred(), 0u);
  const std::uint32_t buffers[] = {1u};
  sibling.buffers_.forget(1, buffers);
  CHECK_EQ(rig.context.slices_owned(), 0u);
  CHECK_EQ(rig.context.slices_deferred(), 1u);
  CHECK_EQ(rig.context.slices_busy(), 1u);
  rig.context.send_forgets();
  CHECK_EQ(rig.forgotten(), 1u);
  sibling.timeline_.finish(1u);
  rig.context.slices_reclaim();
  CHECK_EQ(rig.context.slices_deferred(), 0u);
  CHECK_EQ(rig.context.slices_busy(), 0u);
  CHECK(rig.context.invariants_hold()); }

TEST(guest_gl_context_slices, a_sliced_read_copies_the_buffer_and_records_the_error_of_one_it_cannot)
{ Rig rig;
  CHECK_EQ(rig.place(1u, storage, true, rig.pattern), Status::ok);
  std::uint8_t seen[16] = {};
  rig.context.read_sliced(1u, 32, 16, seen);
  CHECK(std::memcmp(seen, rig.pattern + 32u, 16u) == 0);
  CHECK_EQ(rig.context.take_error(), 0u);
  rig.context.read_sliced(9u, 0, 16, seen);
  CHECK_EQ(rig.context.take_error(), invalid_operation);
  rig.context.read_sliced(1u, bytes, 16, seen);
  CHECK_EQ(rig.context.take_error(), invalid_value);
  CHECK_EQ(rig.context.take_error(), 0u); }

TEST(slice_pin, a_pin_holds_its_owner_until_released_replaced_or_destroyed)
{ Rig rig;
  Sibling sibling(rig);
  CHECK_EQ(rig.slices().pinned(), 0u);
  {
      SlicePin pin;
      CHECK(pin.invariants_hold());
      pin.hold(&rig.slices());
      CHECK_EQ(rig.slices().pinned(), 1u);
      CHECK_EQ(rig.slices().busy(), 1u);
      pin.hold(&rig.slices());
      CHECK_EQ(rig.slices().pinned(), 1u);
      pin.hold(&sibling.buffers_);
      CHECK_EQ(rig.slices().pinned(), 0u);
      CHECK_EQ(sibling.buffers_.pinned(), 1u);
      CHECK(pin.invariants_hold());
      pin.hold(nullptr);
      CHECK_EQ(sibling.buffers_.pinned(), 0u);
      pin.hold(&rig.slices());
      CHECK_EQ(rig.slices().pinned(), 1u);
      rig.slices().unpin();
      CHECK(!pin.invariants_hold());
      rig.slices().pin();
      CHECK(pin.invariants_hold());
  }
  CHECK_EQ(rig.slices().pinned(), 0u);
  CHECK_EQ(rig.slices().busy(), 0u);
  {
      SlicePin empty;
      CHECK_EQ(rig.slices().pinned(), 0u);
  }
  CHECK_EQ(rig.slices().pinned(), 0u); }

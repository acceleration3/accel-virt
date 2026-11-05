#include "core/support/status.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/kmt_blob_arena.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "proto/client/client_blob_arena.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/resources/host_memory_request.hpp"
#include "proto/submission/blob_import_answer.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/submission/published_slot_layout.hpp"
#include "proto/wire/guest_bytes.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>
#include <cstring>

using av::ClientBlobLease;
using av::CommandHeader;
using av::GuestConnection;
using av::HostMemoryRequest;
using av::KmtBlobArena;
using av::KmtTransport;
using av::MockKmt;
using av::PciIdentity;
using av::ScriptedHost;
using av::Status;
using av::SubApi;

namespace {

const PciIdentity av(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u);
constexpr std::uint64_t mib = std::uint64_t{1} << 20u;
constexpr std::uint64_t imported_memory = 0x0000600000000077ull;
constexpr std::uint32_t reply_bytes = 4096u;
constexpr std::uint32_t link_objects = 7u;
constexpr std::uint64_t window_bytes = std::uint64_t{256} << 20u;
constexpr std::uint64_t slot = av::GuestBytes::slot_bytes;

class Rig {
public:
    Rig()
        : adapter_(0u), connection_(kmt_), host_(kmt_), staging_{}, spare_{},
          transport_(connection_, SubApi::opengl, 12u, reply_bytes, staging_, sizeof staging_),
          arena_(connection_, transport_)
    {
        kmt_.add_adapter(av, 3200u);
        std::uint32_t count = 0u;
        CHECK_STATUS(kmt_.enumerate_adapters(&adapter_, 1u, count), Status::ok);
        CHECK_STATUS(connection_.open(adapter_), Status::ok);
        CHECK_STATUS(transport_.initialize(), Status::ok);
        kmt_.open_host_window(window_bytes);
        CHECK(arena_.invariants_hold());
    }
    ~Rig() { CHECK_EQ(kmt_.misuses(), 0u); }

    MockKmt         &kmt() { return kmt_; }
    ScriptedHost    &host() { return host_; }
    GuestConnection &connection() { return connection_; }
    std::uint8_t    *spare_staging() { return spare_; }
    KmtTransport &transport() { return transport_; }
    KmtBlobArena &arena() { return arena_; }

    void host_takes_the_imports(std::uint32_t count = 2u)
    {
        for (std::uint32_t index = 0u; index < count; index++) host_.blob_import(0u, imported_memory + index);
    }
    void host_takes_the_releases(std::uint32_t count)
    {
        for (std::uint32_t index = 0u; index < count; index++) host_.nop();
    }

    void check_nothing_is_held(std::uint32_t kernels = 0u)
    {
        CHECK(arena_.invariants_hold());
        CHECK(!arena_.lending());
        CHECK_EQ(arena_.bytes(), 0u);
        CHECK(!transport_.blobs().ready());
        CHECK(!transport_.blobs().take(1u).taken());
        CHECK_EQ(kmt_.open_objects(), link_objects);
        CHECK_EQ(kmt_.window_chunks(), kernels);
    }

private:
    MockKmt         kmt_;
    std::uint32_t   adapter_;
    GuestConnection connection_;
    ScriptedHost    host_;
    std::uint8_t    staging_[1024];
    std::uint8_t    spare_[1024];
    KmtTransport    transport_;
    KmtBlobArena    arena_;
};

std::uint32_t command_of(const MockKmt &kmt, std::uint32_t id, std::uint32_t nth)
{
    for (std::uint32_t at = 0u; at < kmt.commands(); at++)
        if (kmt.command_id(at) == id && nth-- == 0u) return at;
    return 0xFFFFFFFFu;
}

const std::uint8_t *body_of(const MockKmt &kmt, std::uint32_t id, std::uint32_t nth)
{
    return kmt.command_bytes(command_of(kmt, id, nth)) + CommandHeader::encoded_size;
}

std::uint32_t u32_at(const std::uint8_t *bytes, std::uint32_t at)
{
    std::uint32_t value = 0u;
    std::memcpy(&value, bytes + at, sizeof value);
    return value;
}

std::uint64_t u64_at(const std::uint8_t *bytes, std::uint32_t at)
{
    std::uint64_t value = 0u;
    std::memcpy(&value, bytes + at, sizeof value);
    return value;
}

struct ReleaseWitness {
    MockKmt      *kmt;
    std::uint32_t bindings_at_release;
};

void witness_release(void *context)
{
    ReleaseWitness &witness = *static_cast<ReleaseWitness *>(context);
    witness.bindings_at_release = witness.kmt->window_chunks();
}

}

TEST(kmt_blob_arena, lends_nothing_before_it_is_opened)
{ Rig rig;
  CHECK(!rig.arena().lending());
  CHECK(!rig.transport().blobs().ready());
  CHECK(!rig.transport().blobs().take(4096u).taken());
  CHECK_STATUS(rig.arena().close(), Status::ok);
  CHECK_EQ(rig.kmt().commands(), 0u);
  uint8_t *view = nullptr;
  std::uint64_t offset = 0u;
  CHECK(!rig.arena().take_query_slots(4u, view, offset));
  rig.check_nothing_is_held(); }

TEST(kmt_blob_arena, opens_a_control_segment_at_slot_zero_and_a_ring_at_the_next_slot)
{ Rig rig;
  rig.host_takes_the_imports();
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  CHECK(rig.arena().invariants_hold());
  CHECK(rig.arena().lending());
  CHECK_EQ(rig.arena().bytes(), av::PublishedSlotLayout::control_bytes + KmtBlobArena::default_bytes);
  CHECK_EQ(rig.kmt().window_chunks(), 2u);
  const std::uint8_t *const control_request = body_of(rig.kmt(), CommandHeader::import_blob_id, 0u);
  CHECK_EQ(u32_at(control_request, 0u), MockKmt::first_window_resource);
  CHECK_EQ(u32_at(control_request, 4u), 1u);
  CHECK_EQ(u64_at(control_request, 8u), 0u);
  const std::uint8_t *const ring_request = body_of(rig.kmt(), CommandHeader::import_blob_id, 1u);
  CHECK_EQ(u32_at(ring_request, 0u), MockKmt::first_window_resource + 1u);
  CHECK_EQ(u32_at(ring_request, 4u), 1u);
  CHECK_EQ(u64_at(ring_request, 8u), slot);
  av::ClientBlobArena &blobs = rig.transport().blobs();
  CHECK(blobs.ready());
  CHECK_EQ(blobs.segments(), 1u);
  const std::uint8_t *const ring = rig.kmt().window_chunk_memory(MockKmt::first_window_resource + 1u);
  const ClientBlobLease first = blobs.take(4096u);
  CHECK_EQ(first.offset(), slot);
  CHECK(static_cast<const void *>(first.data()) == static_cast<const void *>(ring));
  CHECK(!blobs.take(16u * mib + 1u).taken());
  CHECK(blobs.take(16u * mib - 4096u).taken());
  const std::uint32_t before = rig.kmt().commands();
  CHECK_STATUS(rig.arena().open(0u), Status::invalid_state);
  CHECK_EQ(rig.kmt().commands(), before);
  rig.host_takes_the_releases(2u);
  CHECK_STATUS(rig.arena().close(), Status::ok);
  CHECK_EQ(u64_at(body_of(rig.kmt(), CommandHeader::release_blob_id, 0u), 0u), imported_memory + 1u);
  CHECK_EQ(u64_at(body_of(rig.kmt(), CommandHeader::release_blob_id, 1u), 0u), imported_memory);
  rig.check_nothing_is_held(); }

TEST(kmt_blob_arena, a_ring_is_whole_pages_between_the_default_and_one_slot)
{ struct Case { std::uint64_t asked; std::uint64_t ring; };
  const Case cases[] = {{1u, 16u * mib}, {16u * mib - 1u, 16u * mib}, {16u * mib, 16u * mib},
                        {16u * mib + 1u, 16u * mib + 4096u}, {40u * mib + 5u, 40u * mib + 4096u},
                        {slot, slot}, {slot + 1u, slot}, {KmtBlobArena::max_bytes, slot}};
  for (const Case &one : cases) {
      Rig rig;
      rig.host_takes_the_imports();
      CHECK_STATUS(rig.arena().open(one.asked), Status::ok);
      CHECK_EQ(rig.arena().bytes(), av::PublishedSlotLayout::control_bytes + one.ring);
      CHECK_EQ(rig.transport().blobs().ring_bytes(), one.ring);
      CHECK(rig.arena().invariants_hold());
      rig.host_takes_the_releases(2u);
      CHECK_STATUS(rig.arena().close(), Status::ok);
      rig.check_nothing_is_held();
  }
  const std::uint64_t pasts[] = {KmtBlobArena::max_bytes + 1u, ~std::uint64_t{0}};
  for (const std::uint64_t past : pasts) {
      Rig rig;
      CHECK_STATUS(rig.arena().open(past), Status::invalid_parameter);
      CHECK_EQ(rig.kmt().commands(), 0u);
      rig.check_nothing_is_held();
  } }

TEST(kmt_blob_arena, the_error_word_is_the_last_word_of_the_control_segment)
{ Rig rig;
  CHECK_EQ(rig.arena().host_errors(), 0u);
  rig.host_takes_the_imports();
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  std::uint8_t *const control = rig.kmt().window_chunk_memory(MockKmt::first_window_resource);
  CHECK_EQ(rig.arena().host_errors(), 0u);
  const std::uint64_t word = 0x8000000000030201ull;
  std::memcpy(control + av::PublishedSlotLayout::control_bytes - 8u, &word, sizeof word);
  CHECK_EQ(rig.arena().host_errors(), word);
  rig.host_takes_the_releases(2u);
  CHECK_STATUS(rig.arena().close(), Status::ok);
  CHECK_EQ(rig.arena().host_errors(), 0u); }

TEST(kmt_blob_arena, query_slots_come_from_the_control_segment_once)
{ Rig rig;
  rig.host_takes_the_imports();
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  std::uint8_t *view = nullptr;
  std::uint64_t offset = 0u;
  CHECK(!rig.arena().take_query_slots(0u, view, offset));
  CHECK(!rig.arena().take_query_slots(av::PublishedSlotLayout::control_bytes, view, offset));
  CHECK(rig.arena().take_query_slots(64u, view, offset));
  const av::PublishedSlotLayout tail(av::PublishedSlotLayout::control_bytes);
  CHECK_EQ(offset, tail.slots_at(64u));
  CHECK(view == rig.kmt().window_chunk_memory(MockKmt::first_window_resource) + offset);
  CHECK(offset + 64u * av::PublishedSlotLayout::slot_bytes <= tail.error_word());
  CHECK(!rig.arena().take_query_slots(64u, view, offset));
  rig.host_takes_the_releases(2u);
  CHECK_STATUS(rig.arena().close(), Status::ok); }

TEST(kmt_blob_arena, a_region_is_its_own_segment_named_apart_from_the_ring)
{ Rig rig;
  rig.host_takes_the_imports();
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  rig.host().blob_import(0u, imported_memory + 2u);
  av::ArenaSegment *region = nullptr;
  CHECK_STATUS(rig.arena().open_region(3u * mib + 5u, region), Status::ok);
  CHECK(region != nullptr);
  CHECK_EQ(region->base(), 2u * slot);
  CHECK_EQ(region->bytes(), 3u * mib + 4096u);
  CHECK_EQ(u64_at(body_of(rig.kmt(), CommandHeader::import_blob_id, 2u), 8u), 2u * slot);
  CHECK_EQ(rig.transport().blobs().set_aside_bytes(), av::PublishedSlotLayout::control_bytes + 3u * mib + 4096u);
  std::uint64_t offset = 0u;
  CHECK(rig.transport().blobs().holds(region->view() + 100, 8u, offset));
  CHECK_EQ(offset, 2u * slot + 100u);
  CHECK(rig.arena().invariants_hold());
  CHECK_STATUS(rig.arena().close_region(slot * 9u), Status::not_found);
  rig.host().nop();
  CHECK_STATUS(rig.arena().close_region(2u * slot), Status::ok);
  CHECK(!rig.transport().blobs().holds(region->view(), 1u, offset));
  CHECK_EQ(rig.transport().blobs().set_aside_bytes(), av::PublishedSlotLayout::control_bytes);
  rig.host().blob_import(0u, imported_memory + 3u);
  av::ArenaSegment *again = nullptr;
  CHECK_STATUS(rig.arena().open_region(4096u, again), Status::ok);
  CHECK_EQ(again->base(), 2u * slot);
  rig.host_takes_the_releases(3u);
  CHECK_STATUS(rig.arena().close(), Status::ok);
  rig.check_nothing_is_held(); }

TEST(kmt_blob_arena, a_region_needs_a_lending_arena_and_a_size_it_can_name)
{ Rig rig;
  av::ArenaSegment *region = reinterpret_cast<av::ArenaSegment *>(1u);
  CHECK_STATUS(rig.arena().open_region(4096u, region), Status::invalid_parameter);
  CHECK(region == nullptr);
  rig.host_takes_the_imports();
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  CHECK_STATUS(rig.arena().open_region(0u, region), Status::invalid_parameter);
  CHECK_STATUS(rig.arena().open_region(KmtBlobArena::max_bytes + 1u, region), Status::invalid_parameter);
  rig.host_takes_the_releases(2u);
  CHECK_STATUS(rig.arena().close(), Status::ok); }

TEST(kmt_blob_arena, only_eight_regions_open_at_once)
{ Rig rig;
  rig.host_takes_the_imports();
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  av::ArenaSegment *region = nullptr;
  for (std::uint32_t index = 0u; index < KmtBlobArena::max_regions; index++) {
      rig.host().blob_import(0u, imported_memory + 2u + index);
      CHECK_STATUS(rig.arena().open_region(4096u, region), Status::ok);
  }
  CHECK_STATUS(rig.arena().open_region(4096u, region), Status::no_space);
  CHECK(region == nullptr);
  rig.host_takes_the_releases(2u + KmtBlobArena::max_regions);
  CHECK_STATUS(rig.arena().close(), Status::ok);
  rig.check_nothing_is_held(); }

TEST(kmt_blob_arena, a_control_import_the_host_refuses_leaves_nothing_held)
{ { Rig rig;
    rig.host().blob_import(av::BlobImportAnswer::executor_refused, 0u);
    CHECK_STATUS(rig.arena().open(0u), Status::invalid_parameter);
    CHECK_EQ(rig.kmt().commands(), 1u);
    rig.check_nothing_is_held(); }
  { Rig rig;
    rig.host().nop();
    CHECK_STATUS(rig.arena().open(0u), Status::not_supported);
    rig.check_nothing_is_held(); }
  { Rig rig;
    rig.kmt().withhold_answer_at(1u);
    CHECK_STATUS(rig.arena().open(0u), Status::invalid_state);
    CHECK(rig.transport().faulted());
    rig.check_nothing_is_held(1u); } }

TEST(kmt_blob_arena, a_ring_the_host_refuses_keeps_the_control_for_the_next_try_and_close_lets_it_go)
{ Rig rig;
  rig.host().blob_import(0u, imported_memory);
  rig.host().blob_import(av::BlobImportAnswer::no_room, 0u);
  CHECK_STATUS(rig.arena().open(0u), Status::no_memory);
  CHECK(!rig.arena().lending());
  CHECK(!rig.transport().blobs().ready());
  CHECK(rig.arena().invariants_hold());
  rig.host().blob_import(0u, imported_memory + 1u);
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  CHECK_EQ(rig.kmt().window_chunks(), 2u);
  rig.host_takes_the_releases(2u);
  CHECK_STATUS(rig.arena().close(), Status::ok);
  rig.check_nothing_is_held(); }

TEST(kmt_blob_arena, close_releases_every_import_while_its_chunk_is_still_bound)
{ Rig rig;
  rig.host_takes_the_imports();
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  ReleaseWitness witness = {&rig.kmt(), 0xFFu};
  rig.kmt().play_at_wait(1u, &witness_release, &witness);
  rig.host_takes_the_releases(2u);
  CHECK_STATUS(rig.arena().close(), Status::ok);
  CHECK_EQ(witness.bindings_at_release, 2u);
  rig.check_nothing_is_held();
  const std::uint32_t escapes = rig.kmt().calls(MockKmt::Call::escape);
  CHECK_STATUS(rig.arena().close(), Status::ok);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), escapes); }

TEST(kmt_blob_arena, close_lets_every_part_go_and_answers_the_first_failure)
{ Rig rig;
  rig.host_takes_the_imports();
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  rig.host().blob_import(0u, 1u);
  rig.host().nop();
  CHECK_STATUS(rig.arena().close(), Status::malformed);
  CHECK(!rig.arena().lending());
  CHECK(!rig.transport().blobs().ready());
  CHECK_EQ(rig.kmt().window_chunks(), 0u); }

TEST(kmt_blob_arena, abandon_lets_every_chunk_go_without_a_host_call)
{ Rig rig;
  CHECK_STATUS(rig.arena().abandon(), Status::ok);
  rig.host_takes_the_imports();
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  const std::uint32_t commands = rig.kmt().commands();
  CHECK_STATUS(rig.arena().abandon(), Status::ok);
  CHECK_EQ(rig.kmt().commands(), commands);
  rig.check_nothing_is_held();
  const std::uint32_t escapes = rig.kmt().calls(MockKmt::Call::escape);
  CHECK_STATUS(rig.arena().abandon(), Status::ok);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), escapes); }

TEST(kmt_blob_arena, the_invariant_catches_rings_the_transport_does_not_lend)
{ Rig rig;
  rig.host_takes_the_imports();
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  CHECK(rig.arena().invariants_hold());
  rig.transport().blobs().lend(nullptr, 0u);
  CHECK(!rig.arena().invariants_hold());
  rig.transport().blobs().lend(rig.kmt().window_chunk_memory(MockKmt::first_window_resource + 1u), 16u * mib);
  CHECK(rig.arena().invariants_hold());
  rig.host_takes_the_releases(2u);
  CHECK_STATUS(rig.arena().close(), Status::ok); }

namespace {

void fill_the_ring(Rig &rig)
{
    av::ClientBlobArena &blobs = rig.transport().blobs();
    CHECK(blobs.take(blobs.room()).taken());
}


}

TEST(kmt_transport_grow, a_request_the_ring_cannot_hold_gains_a_segment_as_big_as_the_ring_before_a_writer_is_lent)
{ Rig rig;
  rig.host_takes_the_imports();
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  fill_the_ring(rig);
  av::ClientBlobArena &blobs = rig.transport().blobs();
  const std::uint64_t wanted[1] = {1u * mib};
  CHECK(!blobs.fits(wanted, 1u));
  rig.host().blob_import(0u, imported_memory + 2u);
  CHECK(rig.transport().make_room_for_blobs(wanted, 1u));
  CHECK_EQ(blobs.segments(), 2u);
  CHECK_EQ(blobs.ring_bytes(), 32u * mib);
  CHECK_EQ(blobs.segment_base(1u), 2u * slot);
  CHECK(rig.arena().invariants_hold());
  const std::uint8_t *const request = body_of(rig.kmt(), CommandHeader::import_blob_id, 2u);
  CHECK_EQ(u32_at(request, 4u), 1u);
  CHECK_EQ(u64_at(request, 8u), 2u * slot);
  CHECK_EQ(blobs.segment_bytes(1u), 16u * mib);
  CHECK(blobs.fits(wanted, 1u));
  rig.host_takes_the_releases(3u);
  CHECK_STATUS(rig.arena().close(), Status::ok);
  rig.check_nothing_is_held(); }

TEST(kmt_transport_grow, a_room_that_already_fits_adds_nothing_and_asks_the_host_nothing)
{ Rig rig;
  rig.host_takes_the_imports();
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  const std::uint32_t before = rig.kmt().commands();
  const std::uint64_t wanted[2] = {1u * mib, 0u};
  CHECK(rig.transport().make_room_for_blobs(wanted, 2u));
  CHECK_EQ(rig.kmt().commands(), before);
  CHECK_EQ(rig.transport().blobs().segments(), 1u);
  rig.host_takes_the_releases(2u);
  CHECK_STATUS(rig.arena().close(), Status::ok); }

TEST(kmt_transport_grow, a_request_bigger_than_the_ring_gets_a_segment_of_the_request_s_size)
{ Rig rig;
  rig.host_takes_the_imports();
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  fill_the_ring(rig);
  rig.host().blob_import(0u, imported_memory + 2u);
  const std::uint64_t wanted[2] = {18u * mib, 2u * mib + 5u};
  CHECK(rig.transport().make_room_for_blobs(wanted, 2u));
  CHECK_EQ(rig.transport().blobs().segment_bytes(1u), 20u * mib + 4096u);
  rig.host_takes_the_releases(3u);
  CHECK_STATUS(rig.arena().close(), Status::ok);
  rig.check_nothing_is_held(); }

TEST(kmt_transport_grow, doubling_stops_at_one_slot_per_segment)
{ Rig rig;
  rig.host_takes_the_imports();
  CHECK_STATUS(rig.arena().open(slot), Status::ok);
  fill_the_ring(rig);
  rig.host().blob_import(0u, imported_memory + 2u);
  const std::uint64_t wanted[1] = {1u * mib};
  CHECK(rig.transport().make_room_for_blobs(wanted, 1u));
  CHECK_EQ(rig.transport().blobs().segment_bytes(1u), slot);
  CHECK_EQ(rig.transport().blobs().ring_bytes(), 2u * slot);
  rig.host_takes_the_releases(3u);
  CHECK_STATUS(rig.arena().close(), Status::ok); }

TEST(kmt_transport_grow, a_request_past_the_channel_ceiling_grows_nothing)
{ Rig rig;
  rig.host_takes_the_imports();
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  const std::uint32_t before = rig.kmt().commands();
  const std::uint64_t wanted[1] = {KmtBlobArena::max_bytes};
  CHECK(!rig.transport().make_room_for_blobs(wanted, 1u));
  const std::uint64_t over[2] = {KmtBlobArena::max_bytes, KmtBlobArena::max_bytes};
  CHECK(!rig.transport().make_room_for_blobs(over, 2u));
  CHECK_EQ(rig.kmt().commands(), before);
  CHECK_EQ(rig.transport().blobs().segments(), 1u);
  CHECK_STATUS(rig.arena().grow(KmtBlobArena::max_bytes), Status::no_space);
  CHECK_STATUS(rig.arena().grow(~std::uint64_t{0}), Status::no_space);
  CHECK(rig.arena().invariants_hold());
  rig.host_takes_the_releases(2u);
  CHECK_STATUS(rig.arena().close(), Status::ok); }

TEST(kmt_transport_grow, a_host_that_refuses_the_segment_leaves_the_ring_and_its_slots_as_they_were)
{ Rig rig;
  rig.host_takes_the_imports();
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  fill_the_ring(rig);
  rig.host().blob_import(av::BlobImportAnswer::no_room, 0u);
  const std::uint64_t wanted[1] = {18u * mib};
  CHECK(!rig.transport().make_room_for_blobs(wanted, 1u));
  CHECK_EQ(rig.transport().blobs().segments(), 1u);
  CHECK(rig.arena().invariants_hold());
  rig.host().blob_import(0u, imported_memory + 3u);
  CHECK(rig.transport().make_room_for_blobs(wanted, 1u));
  CHECK_EQ(rig.transport().blobs().segment_base(1u), 2u * slot);
  rig.host_takes_the_releases(3u);
  CHECK_STATUS(rig.arena().close(), Status::ok); }

TEST(kmt_transport_grow, no_arena_no_growth)
{ Rig rig;
  const std::uint64_t wanted[1] = {1u * mib};
  CHECK(!rig.transport().make_room_for_blobs(wanted, 1u));
  CHECK_STATUS(rig.arena().grow(1u), Status::invalid_state); }


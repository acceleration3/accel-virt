#include "core/log/driver_log.hpp"
#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/transport/arena_segment.hpp"
#include "guest/transport/arena_slots.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/kmt_blob_arena.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "harness/icd/recording_log.hpp"
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

namespace av {

struct ArenaSegmentProbe {
    static void set_bytes(ArenaSegment &segment, std::uint64_t bytes) { segment.bytes_ = bytes; }
    static void set_base(ArenaSegment &segment, std::uint64_t base) { segment.base_ = base; }
};

struct KmtBlobArenaProbe {
    static ObjectStorage<ArenaSegment> &control(KmtBlobArena &arena) { return arena.control_; }
    static ObjectStorage<ArenaSegment> &ring(KmtBlobArena &arena, std::uint32_t index) { return arena.rings_[index]; }
    static ArenaSlots                  &slots(KmtBlobArena &arena) { return arena.slots_; }
    static std::uint64_t               *idle_since(KmtBlobArena &arena) { return arena.idle_since_; }
};

struct KmtTransportProbe {
    static void     press(KmtTransport &transport, std::uint32_t waits) { transport.room_pressure_ = waits; }
    static void     press_for(KmtTransport &transport, std::uint64_t ticks) { transport.room_wait_ticks_ = ticks; }
    static std::uint32_t pressure(const KmtTransport &transport) { return transport.room_pressure_; }
};

}

using av::ArenaSegmentProbe;
using av::KmtBlobArenaProbe;
using av::KmtTransportProbe;

namespace {

const PciIdentity av(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u);
constexpr std::uint64_t mib = std::uint64_t{1} << 20u;
constexpr std::uint64_t imported_memory = 0x0000600000000077ull;
constexpr std::uint32_t reply_bytes = 4096u;
constexpr std::uint32_t link_objects = 7u;
constexpr std::uint64_t window_bytes = MockKmt::rig_window_bytes;
constexpr std::uint64_t slot = av::GuestBytes::slot_bytes;

class Rig {
public:
    Rig()
        : adapter_(0u), connection_(kmt_), host_(kmt_), staging_{}, spare_{},
          transport_(connection_, SubApi::opengl, 12u, reply_bytes, staging_, sizeof staging_),
          sink_(), lines_(sink_, "test"), arena_(connection_, transport_, lines_)
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
    av::RecordingLog &log() { return sink_; }

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
    av::RecordingLog sink_;
    av::DriverLog   lines_;
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
  CHECK(rig.arena().take_query_slots(512u, view, offset));
  const av::PublishedSlotLayout tail(av::PublishedSlotLayout::control_bytes);
  CHECK_EQ(offset, tail.slots_at(512u));
  CHECK(view == rig.kmt().window_chunk_memory(MockKmt::first_window_resource) + offset);
  CHECK(offset + 512u * av::PublishedSlotLayout::slot_bytes <= tail.error_word());
  CHECK(!rig.arena().take_query_slots(512u, view, offset));
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
    CHECK(rig.arena().failed_step() == nullptr);
    CHECK_STATUS(rig.arena().open(0u), Status::invalid_parameter);
    CHECK_EQ(std::strcmp(rig.arena().failed_step(), "control segment open"), 0);
    CHECK(rig.arena().failed_status() == Status::invalid_parameter);
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
  CHECK_EQ(std::strcmp(rig.arena().failed_step(), "ring segment open"), 0);
  CHECK(rig.arena().failed_status() == Status::no_memory);
  CHECK(!rig.arena().lending());
  CHECK(!rig.transport().blobs().ready());
  CHECK(rig.arena().invariants_hold());
  rig.host().blob_import(0u, imported_memory + 1u);
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  CHECK(rig.arena().failed_step() == nullptr);
  CHECK(rig.arena().failed_status() == Status::ok);
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

TEST(kmt_transport_grow, a_request_the_drained_ring_cannot_hold_gains_a_segment_of_the_request_s_size_before_a_writer_is_lent)
{ Rig rig;
  rig.host_takes_the_imports();
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  fill_the_ring(rig);
  av::ClientBlobArena &blobs = rig.transport().blobs();
  const std::uint64_t wanted[1] = {18u * mib};
  CHECK(!blobs.fits(wanted, 1u));
  CHECK(!blobs.fits_drained(wanted, 1u));
  rig.host().blob_import(0u, imported_memory + 2u);
  CHECK(rig.transport().make_room_for_blobs(wanted, 1u));
  CHECK_EQ(blobs.segments(), 2u);
  CHECK_EQ(blobs.ring_bytes(), 34u * mib);
  CHECK_EQ(blobs.segment_base(1u), 2u * slot);
  CHECK(rig.arena().invariants_hold());
  const std::uint8_t *const request = body_of(rig.kmt(), CommandHeader::import_blob_id, 2u);
  CHECK_EQ(u32_at(request, 4u), 1u);
  CHECK_EQ(u64_at(request, 8u), 2u * slot);
  CHECK_EQ(blobs.segment_bytes(1u), 18u * mib);
  CHECK(blobs.fits(wanted, 1u));
  CHECK_EQ(rig.log().lines(), 0u);
  rig.host_takes_the_releases(3u);
  CHECK_STATUS(rig.arena().close(), Status::ok);
  rig.check_nothing_is_held(); }

TEST(kmt_transport_grow, a_full_ring_the_host_will_drain_is_not_grown_for_a_request_it_can_hold)
{ Rig rig;
  rig.host_takes_the_imports();
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  fill_the_ring(rig);
  const std::uint32_t before = rig.kmt().commands();
  const std::uint64_t wanted[1] = {1u * mib};
  CHECK(rig.transport().blobs().fits_drained(wanted, 1u));
  CHECK(!rig.transport().make_room_for_blobs(wanted, 1u));
  CHECK_EQ(rig.transport().blobs().segments(), 1u);
  CHECK_EQ(rig.kmt().commands(), before);
  CHECK(rig.arena().invariants_hold());
  rig.host_takes_the_releases(2u);
  CHECK_STATUS(rig.arena().close(), Status::ok); }

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

TEST(kmt_transport_pressure, repeated_room_waits_on_a_ring_near_its_peak_double_it_up_to_one_slot_per_segment)
{ Rig rig;
  rig.host_takes_the_imports();
  CHECK_STATUS(rig.arena().open(slot), Status::ok);
  fill_the_ring(rig);
  rig.host().blob_import(0u, imported_memory + 2u);
  KmtTransportProbe::press(rig.transport(), KmtTransport::pressure_waits);
  rig.transport().grow_if_pressed();
  CHECK_EQ(rig.transport().blobs().segment_bytes(1u), slot);
  CHECK_EQ(rig.transport().blobs().ring_bytes(), 2u * slot);
  CHECK_EQ(KmtTransportProbe::pressure(rig.transport()), 0u);
  CHECK_EQ(rig.log().lines(), 0u);
  CHECK(rig.arena().invariants_hold());
  rig.host_takes_the_releases(3u);
  CHECK_STATUS(rig.arena().close(), Status::ok); }

TEST(kmt_transport_pressure, a_hundred_milliseconds_of_waiting_grows_the_ring_however_few_the_waits)
{ Rig rig;
  rig.host_takes_the_imports();
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  fill_the_ring(rig);
  rig.host().blob_import(0u, imported_memory + 2u);
  KmtTransportProbe::press(rig.transport(), 1u);
  KmtTransportProbe::press_for(rig.transport(), KmtTransport::pressure_wait_ticks);
  rig.transport().grow_if_pressed();
  CHECK_EQ(rig.transport().blobs().segments(), 2u);
  CHECK_EQ(KmtTransportProbe::pressure(rig.transport()), 0u);
  rig.host_takes_the_releases(3u);
  CHECK_STATUS(rig.arena().close(), Status::ok); }

TEST(kmt_transport_pressure, a_ring_that_never_came_near_full_or_too_few_waits_grow_nothing_and_the_waits_end_with_their_second)
{ Rig rig;
  rig.host_takes_the_imports();
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  const std::uint32_t before = rig.kmt().commands();
  rig.kmt().reads_clock(nullptr, MockKmt::ticks_per_second / 100u);
  KmtTransportProbe::press(rig.transport(), 12u);
  CHECK(!rig.transport().grow_if_pressed());
  CHECK_EQ(rig.kmt().commands(), before);
  CHECK_EQ(KmtTransportProbe::pressure(rig.transport()), 12u);
  rig.kmt().reads_clock(nullptr, 2u * MockKmt::ticks_per_second);
  CHECK(!rig.transport().grow_if_pressed());
  CHECK_EQ(KmtTransportProbe::pressure(rig.transport()), 0u);
  fill_the_ring(rig);
  rig.kmt().reads_clock(nullptr, MockKmt::ticks_per_second / 100u);
  KmtTransportProbe::press(rig.transport(), KmtTransport::pressure_waits - 1u);
  CHECK(!rig.transport().grow_if_pressed());
  CHECK_EQ(rig.kmt().commands(), before);
  CHECK_EQ(KmtTransportProbe::pressure(rig.transport()), KmtTransport::pressure_waits - 1u);
  CHECK_EQ(rig.transport().blobs().segments(), 1u);
  rig.host_takes_the_releases(2u);
  CHECK_STATUS(rig.arena().close(), Status::ok); }

TEST(kmt_transport_pressure, a_ring_that_grew_is_not_grown_again_by_the_peak_of_the_window_before)
{ Rig rig;
  rig.host_takes_the_imports();
  CHECK_STATUS(rig.arena().open(slot), Status::ok);
  fill_the_ring(rig);
  rig.host().blob_import(0u, imported_memory + 2u);
  KmtTransportProbe::press(rig.transport(), KmtTransport::pressure_waits);
  CHECK(rig.transport().grow_if_pressed());
  CHECK_EQ(rig.transport().blobs().segments(), 2u);
  CHECK_EQ(rig.transport().blobs().window_peak(), 0u);
  KmtTransportProbe::press(rig.transport(), KmtTransport::pressure_waits);
  CHECK(!rig.transport().grow_if_pressed());
  CHECK_EQ(rig.transport().blobs().segments(), 2u);
  CHECK(rig.transport().blobs().peak_ring() >= slot);
  CHECK(rig.arena().invariants_hold());
  rig.host_takes_the_releases(3u);
  CHECK_STATUS(rig.arena().close(), Status::ok); }

TEST(kmt_transport_pressure, a_host_that_refuses_the_pressure_grow_is_logged_once_with_its_status)
{ Rig rig;
  rig.host_takes_the_imports();
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  fill_the_ring(rig);
  rig.host().blob_import(av::BlobImportAnswer::no_room, 0u);
  KmtTransportProbe::press(rig.transport(), KmtTransport::pressure_waits);
  rig.transport().grow_if_pressed();
  CHECK_EQ(rig.transport().blobs().segments(), 1u);
  CHECK_EQ(rig.log().lines(), 1u);
  CHECK_EQ(rig.log().holding("did not grow"), 1u);
  rig.host_takes_the_releases(2u);
  CHECK_STATUS(rig.arena().close(), Status::ok); }

TEST(kmt_transport_pressure, a_transport_with_no_arena_has_nothing_to_grow)
{ Rig rig;
  KmtTransportProbe::press(rig.transport(), 100u);
  const std::uint32_t before = rig.kmt().commands();
  rig.transport().grow_if_pressed();
  CHECK_EQ(rig.kmt().commands(), before);
  CHECK_EQ(KmtTransportProbe::pressure(rig.transport()), 100u); }

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
  CHECK_EQ(rig.log().lines(), 1u);
  rig.host_takes_the_releases(2u);
  CHECK_STATUS(rig.arena().close(), Status::ok); }

TEST(kmt_transport_grow, a_ring_segment_is_never_larger_than_one_slot_so_a_bigger_request_is_refused_untouched)
{ Rig rig;
  rig.host_takes_the_imports();
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  const std::uint32_t before = rig.kmt().commands();
  const std::uint64_t wanted[1] = {slot + 1u};
  CHECK(!rig.transport().make_room_for_blobs(wanted, 1u));
  CHECK_STATUS(rig.arena().grow(slot + 1u), Status::no_space);
  CHECK_EQ(rig.kmt().commands(), before);
  CHECK_EQ(rig.transport().blobs().segments(), 1u);
  CHECK_EQ(rig.log().lines(), 1u);
  CHECK(rig.arena().invariants_hold());
  rig.host().blob_import(0u, imported_memory + 2u);
  CHECK_STATUS(rig.arena().grow(slot), Status::ok);
  CHECK_EQ(rig.transport().blobs().segment_bytes(1u), slot);
  rig.host_takes_the_releases(3u);
  CHECK_STATUS(rig.arena().close(), Status::ok); }

TEST(kmt_transport_grow, sizes_that_add_past_sixty_four_bits_or_past_a_slot_never_wrap_into_a_small_segment)
{ Rig rig;
  rig.host_takes_the_imports();
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  const std::uint32_t before = rig.kmt().commands();
  const std::uint64_t wrapping[3] = {~std::uint64_t{0}, 2u, 3u};
  CHECK(!rig.transport().make_room_for_blobs(wrapping, 3u));
  const std::uint64_t huge[2] = {~std::uint64_t{0} - 1u, ~std::uint64_t{0} - 1u};
  CHECK(!rig.transport().make_room_for_blobs(huge, 2u));
  CHECK_EQ(rig.kmt().commands(), before);
  CHECK_EQ(rig.transport().blobs().segments(), 1u);
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

TEST(kmt_blob_arena_shrink, a_grown_segment_idle_for_ten_seconds_is_released_and_the_first_never_is)
{ Rig rig;
  rig.host_takes_the_imports();
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  av::ClientBlobArena &blobs = rig.transport().blobs();
  rig.host().blob_import(0u, imported_memory + 2u);
  const std::uint64_t wanted[1] = {18u * mib};
  CHECK(rig.transport().make_room_for_blobs(wanted, 1u));
  CHECK(blobs.idle_at(slot));
  CHECK(blobs.idle_at(2u * slot));
  const std::uint64_t t0 = 1000u;
  CHECK_STATUS(rig.arena().release_idle(t0), Status::ok);
  CHECK_STATUS(rig.arena().release_idle(t0 + KmtBlobArena::idle_release_ticks - 1u), Status::ok);
  CHECK_EQ(blobs.segments(), 2u);
  rig.host().nop();
  CHECK_STATUS(rig.arena().release_idle(t0 + KmtBlobArena::idle_release_ticks), Status::ok);
  CHECK_EQ(blobs.segments(), 1u);
  CHECK_EQ(blobs.segment_base(0u), slot);
  CHECK_EQ(blobs.ring_bytes(), 16u * mib);
  CHECK(rig.arena().invariants_hold());
  CHECK_EQ(u64_at(body_of(rig.kmt(), CommandHeader::release_blob_id, 0u), 0u), imported_memory + 2u);
  CHECK_STATUS(rig.arena().release_idle(t0 + 100u * KmtBlobArena::idle_release_ticks), Status::ok);
  CHECK_EQ(blobs.segments(), 1u);
  rig.host_takes_the_releases(2u);
  CHECK_STATUS(rig.arena().close(), Status::ok);
  rig.check_nothing_is_held(); }

TEST(kmt_blob_arena_shrink, a_segment_with_data_in_it_restarts_its_idle_clock)
{ Rig rig;
  rig.host_takes_the_imports();
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  av::ClientBlobArena &blobs = rig.transport().blobs();
  rig.host().blob_import(0u, imported_memory + 2u);
  const std::uint64_t wanted[1] = {18u * mib};
  CHECK(rig.transport().make_room_for_blobs(wanted, 1u));
  CHECK_STATUS(rig.arena().release_idle(1000u), Status::ok);
  const ClientBlobLease lease = blobs.take(18u * mib);
  CHECK_EQ(lease.offset(), 2u * slot);
  CHECK_STATUS(rig.arena().release_idle(1000u + KmtBlobArena::idle_release_ticks), Status::ok);
  CHECK_EQ(blobs.segments(), 2u);
  blobs.give_back(lease);
  CHECK_STATUS(rig.arena().release_idle(1000u + KmtBlobArena::idle_release_ticks + 1u), Status::ok);
  CHECK_EQ(blobs.segments(), 2u);
  CHECK_STATUS(rig.arena().release_idle(1000u + 2u * KmtBlobArena::idle_release_ticks), Status::ok);
  CHECK_EQ(blobs.segments(), 2u);
  rig.host().nop();
  CHECK_STATUS(rig.arena().release_idle(1000u + 2u * KmtBlobArena::idle_release_ticks + 1u), Status::ok);
  CHECK_EQ(blobs.segments(), 1u);
  rig.host_takes_the_releases(2u);
  CHECK_STATUS(rig.arena().close(), Status::ok); }

TEST(kmt_transport_shrink, the_transport_releases_an_idle_segment_once_its_own_clock_says_ten_seconds_passed)
{ Rig rig;
  rig.host_takes_the_imports();
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  rig.host().blob_import(0u, imported_memory + 2u);
  const std::uint64_t wanted[1] = {18u * mib};
  CHECK(rig.transport().make_room_for_blobs(wanted, 1u));
  CHECK_EQ(rig.transport().blobs().segments(), 2u);
  rig.kmt().reads_clock(nullptr, 4u * MockKmt::ticks_per_second);
  rig.transport().release_idle_segments();
  rig.transport().release_idle_segments();
  CHECK_EQ(rig.transport().blobs().segments(), 2u);
  rig.host().nop();
  rig.transport().release_idle_segments();
  rig.transport().release_idle_segments();
  CHECK_EQ(rig.transport().blobs().segments(), 1u);
  CHECK(rig.arena().invariants_hold());
  rig.host_takes_the_releases(2u);
  CHECK_STATUS(rig.arena().close(), Status::ok); }

TEST(kmt_transport_shrink, tuning_an_unpressed_channel_releases_its_idle_segment)
{ Rig rig;
  rig.host_takes_the_imports();
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  rig.host().blob_import(0u, imported_memory + 2u);
  const std::uint64_t wanted[1] = {18u * mib};
  CHECK(rig.transport().make_room_for_blobs(wanted, 1u));
  rig.kmt().reads_clock(nullptr, 4u * MockKmt::ticks_per_second);
  rig.transport().tune_arena();
  rig.transport().tune_arena();
  CHECK_EQ(rig.transport().blobs().segments(), 2u);
  rig.host().nop();
  rig.transport().tune_arena();
  rig.transport().tune_arena();
  CHECK_EQ(rig.transport().blobs().segments(), 1u);
  CHECK(rig.arena().invariants_hold());
  rig.host_takes_the_releases(2u);
  CHECK_STATUS(rig.arena().close(), Status::ok); }

TEST(kmt_transport_shrink, making_room_never_releases_a_segment_however_long_it_has_been_idle)
{ Rig rig;
  rig.host_takes_the_imports();
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  rig.host().blob_import(0u, imported_memory + 2u);
  const std::uint64_t grown[1] = {18u * mib};
  CHECK(rig.transport().make_room_for_blobs(grown, 1u));
  rig.kmt().reads_clock(nullptr, 100u * MockKmt::ticks_per_second);
  rig.transport().release_idle_segments();
  const std::uint32_t before = rig.kmt().commands();
  const std::uint64_t wanted[1] = {1u * mib};
  for (std::uint32_t round = 0u; round < 3u; round++) CHECK(rig.transport().make_room_for_blobs(wanted, 1u));
  CHECK_EQ(rig.transport().blobs().segments(), 2u);
  CHECK_EQ(rig.kmt().commands(), before);
  rig.host_takes_the_releases(3u);
  CHECK_STATUS(rig.arena().close(), Status::ok); }

TEST(kmt_transport_shrink, a_transport_with_no_arena_attached_has_nothing_to_release_or_open)
{ Rig rig;
  rig.host_takes_the_imports();
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  rig.transport().attach_arena(nullptr);
  rig.kmt().reads_clock(nullptr, 100u * MockKmt::ticks_per_second);
  const std::uint32_t before = rig.kmt().commands();
  rig.transport().release_idle_segments();
  CHECK_EQ(rig.kmt().commands(), before);
  CHECK(!rig.transport().open_region(4096u).taken());
  const std::uint64_t wanted[1] = {18u * mib};
  CHECK(!rig.transport().make_room_for_blobs(wanted, 1u));
  rig.transport().attach_arena(&rig.arena());
  rig.host_takes_the_releases(2u);
  CHECK_STATUS(rig.arena().close(), Status::ok); }

TEST(kmt_transport_region, a_region_opened_through_the_transport_is_the_arena_s_and_closes_back_into_it)
{ Rig rig;
  rig.host_takes_the_imports();
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  rig.host().blob_import(0u, imported_memory + 2u);
  const ClientBlobLease region = rig.transport().open_region(3u * mib + 5u);
  CHECK(region.taken());
  CHECK_EQ(region.offset(), 2u * slot);
  CHECK_EQ(region.bytes(), 3u * mib + 5u);
  std::uint64_t offset = 0u;
  CHECK(rig.transport().blobs().holds(region.data(), 16u, offset));
  CHECK_EQ(offset, 2u * slot);
  rig.host().nop();
  rig.transport().close_region(region);
  CHECK(!rig.transport().blobs().holds(region.data(), 16u, offset));
  rig.transport().close_region(ClientBlobLease());
  rig.host_takes_the_releases(2u);
  CHECK_STATUS(rig.arena().close(), Status::ok);
  rig.check_nothing_is_held(); }

TEST(kmt_blob_arena, a_control_or_ring_the_blob_table_has_no_room_for_is_let_go_and_said)
{ { Rig rig;
    std::uint8_t other[64] = {};
    CHECK(rig.transport().blobs().add_region(0u, other, sizeof other));
    rig.host().blob_import(0u, imported_memory);
    rig.host().nop();
    CHECK_STATUS(rig.arena().open(0u), Status::invalid_state);
    CHECK_EQ(std::strcmp(rig.arena().failed_step(), "control region add"), 0);
    CHECK(rig.transport().blobs().remove_region(0u));
    rig.check_nothing_is_held(); }
  { Rig rig;
    std::uint8_t other[64] = {};
    CHECK(rig.transport().blobs().add_segment(slot, other, sizeof other));
    rig.host().blob_import(0u, imported_memory);
    rig.host().blob_import(0u, imported_memory + 1u);
    rig.host().nop();
    CHECK_STATUS(rig.arena().open(0u), Status::invalid_state);
    CHECK_EQ(std::strcmp(rig.arena().failed_step(), "ring region add"), 0);
    CHECK(!rig.arena().lending());
    CHECK(rig.transport().blobs().remove_segment(slot));
    rig.host().nop();
    CHECK_STATUS(rig.arena().close(), Status::ok);
    rig.check_nothing_is_held(); } }

TEST(kmt_blob_arena, a_ring_or_region_the_blob_table_has_no_room_for_is_let_go_and_refused)
{ Rig rig;
  rig.host_takes_the_imports();
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  std::uint8_t other[64] = {};
  CHECK(rig.transport().blobs().add_segment(2u * slot, other, sizeof other));
  rig.host().blob_import(0u, imported_memory + 2u);
  rig.host().nop();
  CHECK_STATUS(rig.arena().grow(4096u), Status::no_space);
  CHECK(rig.transport().blobs().remove_segment(2u * slot));
  CHECK(rig.arena().invariants_hold());
  CHECK(rig.transport().blobs().add_region(2u * slot, other, sizeof other));
  rig.host().blob_import(0u, imported_memory + 3u);
  rig.host().nop();
  av::ArenaSegment *region = nullptr;
  CHECK_STATUS(rig.arena().open_region(4096u, region), Status::no_space);
  CHECK(region == nullptr);
  CHECK(rig.transport().blobs().remove_region(2u * slot));
  rig.host_takes_the_releases(2u);
  CHECK_STATUS(rig.arena().close(), Status::ok);
  rig.check_nothing_is_held(); }

TEST(kmt_blob_arena, abandon_lets_an_open_region_go_without_a_host_call)
{ Rig rig;
  rig.host_takes_the_imports();
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  rig.host().blob_import(0u, imported_memory + 2u);
  av::ArenaSegment *region = nullptr;
  CHECK_STATUS(rig.arena().open_region(3u * mib, region), Status::ok);
  CHECK_EQ(rig.arena().bytes(), av::PublishedSlotLayout::control_bytes + KmtBlobArena::default_bytes + 3u * mib);
  const std::uint32_t commands = rig.kmt().commands();
  CHECK_STATUS(rig.arena().abandon(), Status::ok);
  CHECK_EQ(rig.kmt().commands(), commands);
  rig.check_nothing_is_held(); }

TEST(kmt_blob_arena, every_ring_slot_taken_is_no_space_for_one_more)
{ Rig rig;
  rig.host_takes_the_imports();
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  for (std::uint32_t index = 1u; index < av::ClientBlobArena::max_segments; index++) {
      rig.host().blob_import(0u, imported_memory + 1u + index);
      CHECK_STATUS(rig.arena().grow(4096u), Status::ok);
  }
  CHECK_STATUS(rig.arena().grow(4096u), Status::no_space);
  CHECK(rig.arena().invariants_hold());
  rig.host_takes_the_releases(1u + av::ClientBlobArena::max_segments);
  CHECK_STATUS(rig.arena().close(), Status::ok);
  rig.check_nothing_is_held(); }

TEST(kmt_blob_arena_shrink, a_release_the_host_refuses_for_two_idle_segments_answers_the_first_failure)
{ Rig rig;
  rig.host_takes_the_imports();
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  for (std::uint32_t grown = 0u; grown < 2u; grown++) {
      rig.host().blob_import(0u, imported_memory + 2u + grown);
      CHECK_STATUS(rig.arena().grow(4096u), Status::ok);
  }
  const std::uint64_t t0 = 1000u;
  CHECK_STATUS(rig.arena().release_idle(t0), Status::ok);
  rig.host().blob_import(0u, 1u);
  rig.host().blob_import(0u, 2u);
  CHECK_STATUS(rig.arena().release_idle(t0 + KmtBlobArena::idle_release_ticks), Status::malformed);
  CHECK_EQ(rig.transport().blobs().segments(), 1u);
  rig.host_takes_the_releases(2u);
  CHECK_STATUS(rig.arena().close(), Status::ok); }

TEST(kmt_blob_arena, the_invariant_catches_a_control_a_ring_or_a_region_that_is_not_what_it_was_made)
{ { Rig rig;
    CHECK(rig.arena().invariants_hold());
    CHECK_STATUS(KmtBlobArenaProbe::ring(rig.arena(), 1u).construct(rig.connection(), rig.transport(),
                                                                    KmtBlobArenaProbe::slots(rig.arena())),
                 Status::ok);
    CHECK(!rig.arena().invariants_hold());
    KmtBlobArenaProbe::ring(rig.arena(), 1u).destroy();
    CHECK(rig.arena().invariants_hold()); }
  { Rig rig;
    rig.host_takes_the_imports();
    CHECK_STATUS(rig.arena().open(0u), Status::ok);
    av::ArenaSegment &control = KmtBlobArenaProbe::control(rig.arena()).object();
    ArenaSegmentProbe::set_bytes(control, 5u);
    CHECK(!rig.arena().invariants_hold());
    ArenaSegmentProbe::set_bytes(control, av::PublishedSlotLayout::control_bytes);
    ArenaSegmentProbe::set_base(control, slot);
    CHECK(!rig.arena().invariants_hold());
    ArenaSegmentProbe::set_base(control, 0u);
    av::ArenaSegment &ring = KmtBlobArenaProbe::ring(rig.arena(), 0u).object();
    ArenaSegmentProbe::set_bytes(ring, 5u);
    CHECK(!rig.arena().invariants_hold());
    ArenaSegmentProbe::set_bytes(ring, KmtBlobArena::default_bytes);
    CHECK(rig.arena().invariants_hold());
    rig.host().blob_import(0u, imported_memory + 2u);
    av::ArenaSegment *region = nullptr;
    CHECK_STATUS(rig.arena().open_region(4096u, region), Status::ok);
    ArenaSegmentProbe::set_bytes(*region, 5u);
    CHECK(!rig.arena().invariants_hold());
    ArenaSegmentProbe::set_bytes(*region, 4096u);
    CHECK(rig.arena().invariants_hold());
    CHECK_STATUS(rig.arena().abandon(), Status::ok);
    rig.check_nothing_is_held(); }
  { Rig rig;
    rig.host_takes_the_imports();
    CHECK_STATUS(rig.arena().open(0u), Status::ok);
    av::ObjectStorage<av::ArenaSegment> &big = KmtBlobArenaProbe::ring(rig.arena(), 1u);
    CHECK_STATUS(big.construct(rig.connection(), rig.transport(), KmtBlobArenaProbe::slots(rig.arena())), Status::ok);
    rig.host().blob_import(0u, imported_memory + 2u);
    CHECK_STATUS(big.object().open(slot + 4096u), Status::ok);
    CHECK(!rig.arena().invariants_hold());
    CHECK_STATUS(rig.arena().abandon(), Status::ok);
    rig.check_nothing_is_held(); } }

TEST(kmt_transport_pressure, a_lost_transport_neither_grows_nor_lets_go_of_what_the_arena_holds)
{ Rig rig;
  rig.host_takes_the_imports();
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  fill_the_ring(rig);
  rig.kmt().fail_at(MockKmt::Call::submit, 1u, MockKmt::invalid_parameter);
  std::uint8_t framed[1] = {};
  av::WireWriter writer = rig.transport().request_writer();
  writer.write_u32(1u);
  CHECK(rig.transport().send_async(0x10001u, writer) == av::TransportResult::failed);
  CHECK(rig.transport().faulted());
  KmtTransportProbe::press(rig.transport(), KmtTransport::pressure_waits);
  const std::uint32_t commands = rig.kmt().commands();
  rig.transport().tune_arena();
  CHECK_EQ(rig.kmt().commands(), commands);
  CHECK_EQ(rig.transport().blobs().segments(), 1u);
  const std::uint64_t wanted[1] = {18u * mib};
  CHECK(!rig.transport().make_room_for_blobs(wanted, 1u));
  CHECK_EQ(rig.transport().blobs().segments(), 1u);
  static_cast<void>(framed);
  CHECK_STATUS(rig.arena().abandon(), Status::ok); }

TEST(kmt_transport_region, closing_a_region_the_transport_never_lent_does_nothing)
{ Rig rig;
  std::uint8_t bytes[16] = {};
  rig.transport().close_region(ClientBlobLease(bytes, 0u, sizeof bytes));
  CHECK_EQ(rig.kmt().commands(), 0u);
  rig.check_nothing_is_held(); }

TEST(kmt_transport_grow, two_requests_that_together_pass_a_slot_gain_a_segment_for_the_larger_alone)
{ Rig rig;
  rig.host_takes_the_imports();
  CHECK_STATUS(rig.arena().open(0u), Status::ok);
  fill_the_ring(rig);
  rig.host().blob_import(0u, imported_memory + 2u);
  const std::uint64_t wanted[2] = {slot / 2u + mib, slot / 2u + mib};
  CHECK(!rig.transport().make_room_for_blobs(wanted, 2u));
  CHECK_EQ(rig.transport().blobs().segments(), 2u);
  CHECK(rig.arena().invariants_hold());
  rig.host_takes_the_releases(3u);
  CHECK_STATUS(rig.arena().close(), Status::ok); }

namespace {

class ConsumeAt final : public av::ClockReader {
public:
    ConsumeAt(MockKmt &kmt, std::uint32_t context, std::uint32_t at) : kmt_(kmt), context_(context), at_(at) {}
    void clock_read() override
    {
        if (++reads_ == at_) kmt_.consume_now(context_);
    }

private:
    MockKmt      &kmt_;
    std::uint32_t context_;
    std::uint32_t at_;
    std::uint32_t reads_ = 0u;
};

}

TEST(kmt_transport_grow, room_the_host_frees_while_the_driver_spins_for_it_is_found_without_a_wait_or_a_segment)
{ for (std::uint32_t at = 1u; at <= 8u; at++) {
      Rig rig;
      rig.host_takes_the_imports();
      CHECK_STATUS(rig.arena().open(0u), Status::ok);
      av::ClientBlobArena &blobs = rig.transport().blobs();
      CHECK(blobs.take(blobs.room()).taken());
      av::WireWriter writer = rig.transport().request_writer();
      writer.write_u32(1u);
      CHECK(rig.transport().send_async(0x30001u, writer) == av::TransportResult::ok);
      ConsumeAt consume(rig.kmt(), rig.transport().context(), at);
      rig.kmt().reads_clock(&consume, 1u);
      const std::uint64_t wanted[1] = {1u * mib};
      CHECK(rig.transport().make_room_for_blobs(wanted, 1u));
      rig.kmt().reads_clock(nullptr, 0u);
      CHECK_EQ(blobs.segments(), 1u);
      CHECK(!rig.transport().faulted());
      rig.host_takes_the_releases(2u);
      CHECK_STATUS(rig.arena().close(), Status::ok);
  } }

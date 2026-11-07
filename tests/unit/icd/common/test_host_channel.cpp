#include "core/support/status.hpp"
#include "core/log/keyed_log.hpp"
#include "core/trace/state_writer.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/kmt_blob_arena.hpp"
#include "guest/transport/channel_stats.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "harness/icd/host_channel_probe.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/common/host_channel.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/resources/host_memory_request.hpp"
#include "proto/submission/blob_import_answer.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/submission/fault_code.hpp"
#include "proto/submission/published_slot_layout.hpp"
#include "proto/wire/spill_block.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace av {
struct KmtTransportProbe {
    static std::uint32_t forget_name(KmtTransport &t)
    {
        const std::uint32_t name = t.allocations_[0];
        t.allocations_[0] = 0u;
        return name;
    }
    static void restore_name(KmtTransport &t, std::uint32_t name) { t.allocations_[0] = name; }
    static std::uint64_t counted(KmtTransport &t, ChannelCounter counter) { return t.stats_.snapshot().value(counter); }
};

}

using av::ChannelCounter;
using av::GuestConnection;
using av::HostAllocation;
using av::HostMemoryRequest;
using av::KmtBlobArena;
using av::KmtTransportProbe;
using av::MockKmt;
using av::PciIdentity;
using av::PublishedSlotLayout;
using av::Status;
using av::SubApi;
using av::TransportResult;
using av::WireReader;
using av::WireWriter;
using av::icd::ChannelShape;
using av::icd::HostChannel;
using av::icd::HostChannelProbe;

namespace {

const PciIdentity avgpu(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u);
constexpr std::uint8_t  ring_order = 12u;
constexpr std::uint32_t reply_bytes = 4096u;
constexpr std::size_t   staging_bytes = 4096u;
constexpr std::uint32_t command_id = 0x10001u;
constexpr std::uint32_t import_refused = 1u;
constexpr std::uint64_t imported_memory = 0x0000600000000077ull;
constexpr std::uint64_t first_memory_of_lock_test = 0x0000600000000099ull;
constexpr std::uint32_t memory_tables = 2u;

ChannelShape batching()
{
    return ChannelShape(SubApi::opengl, ring_order, reply_bytes, staging_bytes, 2u * staging_bytes,
                        2u * KmtBlobArena::default_bytes);
}

ChannelShape plain()
{
    return ChannelShape(SubApi::vulkan, ring_order, reply_bytes, staging_bytes, 0u, 0u);
}

class Rig {
public:
    Rig() : kmt_(), platform_(), connection_(kmt_), host_(kmt_), log_(), lines_(platform_, log_, "avtest: "), trace_(), lock_(trace_, 'S'), linked_(0u)
    {
        kmt_.add_adapter(avgpu, 3200u);
        kmt_.open_host_window(MockKmt::rig_window_bytes);
        std::uint32_t adapter = 0u, count = 0u;
        CHECK_STATUS(kmt_.enumerate_adapters(&adapter, 1u, count), Status::ok);
        CHECK_STATUS(connection_.open(adapter), Status::ok);
        linked_ = kmt_.open_objects();
    }
    ~Rig() { CHECK_EQ(kmt_.misuses(), 0u); }

    MockKmt              &kmt() { return kmt_; }
    void arena_imports()
    {
        host_.blob_import(0u, imported_memory);
        host_.blob_import(0u, imported_memory + 1u);
    }
    av::MockPlatform     &platform() { return platform_; }
    GuestConnection      &device() { return connection_; }
    av::ScriptedHost     &host() { return host_; }
    av::RecordingLog     &log() { return log_; }
    av::KeyedLog         &lines() { return lines_; }
    av::RecordingThreadLock &lock() { return lock_; }
    const char *locked()
    {
        static char kept[64];
        std::strncpy(kept, trace_.text(), sizeof kept - 1u);
        trace_.clear();
        return kept;
    }

    void check_nothing_held(const HostChannel &channel, std::uint32_t tables = 0u)
    {
        CHECK(!channel.opened());
        CHECK(!channel.faulted());
        CHECK(!channel.has_arena());
        CHECK_EQ(channel.arena_bytes(), 0u);
        CHECK_EQ(kmt_.open_objects(), linked_);
        CHECK_EQ(platform_.live_allocations(), tables);
        CHECK(channel.invariants_hold());
    }

    std::uint32_t commands() const
    {
        std::uint32_t counted = 0u;
        for (std::uint32_t index = 0u; index < kmt_.commands(); index++)
            if (kmt_.command_id(index) != av::CommandHeader::channel_stats_id) counted++;
        return counted;
    }

    std::uint32_t command_id_at(std::uint32_t counted) const
    {
        for (std::uint32_t index = 0u; index < kmt_.commands(); index++) {
            if (kmt_.command_id(index) == av::CommandHeader::channel_stats_id) continue;
            if (counted-- == 0u) return kmt_.command_id(index);
        }
        return 0u;
    }

    TransportResult send(av::ClientTransport &wire)
    {
        WireWriter writer = wire.request_writer();
        writer.write_zeros(8u);
        return wire.send_async(command_id, writer);
    }

private:
    MockKmt          kmt_;
    av::MockPlatform platform_;
    GuestConnection  connection_;
    av::ScriptedHost host_;
    av::RecordingLog log_;
    av::KeyedLog     lines_;
    av::LockTrace    trace_;
    av::RecordingThreadLock lock_;
    std::uint32_t    linked_;
};

const char *report_of(const HostChannel &channel, char *bytes, std::size_t size)
{
    av::StateWriter writer(bytes, size);
    writer.open("channel", 0u);
    channel.report(writer);
    return writer.dump();
}

}

TEST(host_channel, a_closed_channel_holds_nothing_and_closing_it_again_is_nothing)
{ Rig rig;
  HostChannel channel(rig.platform(), rig.lines(), batching(), rig.lock());
  rig.check_nothing_held(channel);
  CHECK_EQ(channel.host_errors(), 0u);
  channel.close(false);
  channel.close(true);
  rig.check_nothing_held(channel);
  char bytes[128];
  CHECK(std::strcmp(report_of(channel, bytes, sizeof bytes), "channel/0 opened=false faulted=false arena_bytes=0") ==
        0);
  CHECK_EQ(rig.log().lines(), 0u); }

TEST(host_channel, opening_holds_the_transport_the_batch_and_the_arena_at_the_shapes_size)
{ Rig rig;
  HostChannel channel(rig.platform(), rig.lines(), batching(), rig.lock());
  rig.arena_imports();
  CHECK_STATUS(channel.open(rig.device()), Status::ok);
  CHECK(channel.opened());
  CHECK(!channel.faulted());
  CHECK_EQ(channel.arena_bytes(), PublishedSlotLayout::control_bytes + 2u * KmtBlobArena::default_bytes);
  CHECK(channel.has_arena());
  CHECK(channel.transport().blobs().ready());
  CHECK(channel.invariants_hold());
  char bytes[128];
  CHECK(std::strcmp(report_of(channel, bytes, sizeof bytes),
                    "channel/0 opened=true faulted=false arena_bytes=33685504") == 0);
  CHECK_STATUS(channel.open(rig.device()), Status::invalid_state);
  CHECK(channel.opened());
  CHECK_EQ(channel.arena_bytes(), PublishedSlotLayout::control_bytes + 2u * KmtBlobArena::default_bytes);
  rig.host().nop();
  rig.host().nop();
  channel.close(false);
  rig.check_nothing_held(channel);
  CHECK_EQ(rig.log().lines(), 0u); }

TEST(host_channel, the_host_error_word_is_the_control_segments_tail)
{ Rig rig;
  HostChannel channel(rig.platform(), rig.lines(), batching(), rig.lock());
  rig.arena_imports();
  CHECK_STATUS(channel.open(rig.device()), Status::ok);
  CHECK_EQ(channel.host_errors(), 0u);
  const std::uint64_t word = 0x0000000500000502ull;
  std::uint8_t  *slots = nullptr;
  std::uint64_t  offset = 0u;
  CHECK(channel.take_query_slots(1u, slots, offset));
  const PublishedSlotLayout layout(PublishedSlotLayout::control_bytes);
  std::memcpy(slots - layout.slots_at(1u) + layout.error_word(), &word, sizeof word);
  CHECK_EQ(channel.host_errors(), word);
  rig.host().nop();
  channel.close(false); }

TEST(host_channel, what_is_batched_reaches_the_host_before_its_context_goes)
{ Rig rig;
  HostChannel channel(rig.platform(), rig.lines(), batching(), rig.lock());
  rig.arena_imports();
  CHECK_STATUS(channel.open(rig.device()), Status::ok);
  const std::uint32_t sent = rig.commands();
  CHECK(rig.send(channel.wire()) == TransportResult::ok);
  CHECK_EQ(rig.commands(), sent);
  rig.host().nop();
  channel.close(false);
  CHECK(rig.commands() > sent + 1u);
  CHECK_EQ(rig.command_id_at(sent), command_id);
  rig.check_nothing_held(channel); }

TEST(host_channel, a_lost_channel_closes_without_asking_the_host_anything)
{ Rig rig;
  HostChannel channel(rig.platform(), rig.lines(), batching(), rig.lock());
  rig.arena_imports();
  CHECK_STATUS(channel.open(rig.device()), Status::ok);
  CHECK(rig.send(channel.wire()) == TransportResult::ok);
  const std::uint32_t sent = rig.commands();
  const std::uint32_t submitted = rig.kmt().calls(MockKmt::Call::submit);
  channel.close(true);
  CHECK_EQ(rig.commands(), sent);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::submit), submitted);
  rig.check_nothing_held(channel);
  CHECK_EQ(rig.log().lines(), 0u); }

TEST(host_channel, an_arena_the_device_refuses_is_asked_again_at_half_the_size)
{ Rig rig;
  HostChannel channel(rig.platform(), rig.lines(), batching(), rig.lock());
  rig.host().blob_import(0u, imported_memory);
  rig.host().blob_import(import_refused, 0u);
  rig.host().blob_import(0u, imported_memory + 1u);
  CHECK_STATUS(channel.open(rig.device()), Status::ok);
  CHECK_EQ(channel.arena_bytes(), PublishedSlotLayout::control_bytes + KmtBlobArena::default_bytes);
  CHECK(channel.has_arena());
  CHECK_EQ(rig.log().holding("no blob arena"), 0u);
  CHECK(channel.invariants_hold());
  rig.host().nop();
  channel.close(false);
  rig.check_nothing_held(channel); }

TEST(host_channel, a_device_that_will_not_import_at_the_least_size_leaves_a_channel_without_an_arena)
{ Rig rig;
  HostChannel channel(rig.platform(), rig.lines(), batching(), rig.lock());
  rig.host().blob_import(0u, imported_memory);
  rig.host().blob_import(import_refused, 0u);
  rig.host().blob_import(import_refused, 0u);
  rig.host().nop();
  CHECK_STATUS(channel.open(rig.device()), Status::ok);
  CHECK(channel.opened());
  CHECK(!channel.has_arena());
  CHECK_EQ(channel.arena_bytes(), 0u);
  CHECK_EQ(channel.host_errors(), 0u);
  CHECK(!channel.transport().blobs().ready());
  CHECK_EQ(rig.log().holding("avtest: no blob arena for the context; bulk data crosses inline"), 1u);
  CHECK(rig.send(channel.wire()) == TransportResult::ok);
  CHECK(channel.invariants_hold());
  channel.close(false);
  rig.check_nothing_held(channel); }

TEST(host_channel, a_transport_that_faults_opening_the_arena_opens_nothing)
{ for (std::uint32_t refusals = 0u; refusals < 2u; refusals++) {
      Rig rig;
      HostChannel channel(rig.platform(), rig.lines(), batching(), rig.lock());
      if (refusals != 0u) rig.host().blob_import(import_refused, 0u);
      CHECK_STATUS(channel.open(rig.device()), Status::invalid_state);
      CHECK_EQ(rig.log().holding("avtest: the context faulted opening its blob arena"), 1u);
      rig.check_nothing_held(channel);
  } }

TEST(host_channel, a_host_with_no_session_left_is_told_to_the_caller_as_no_memory_and_never_as_a_refusal)
{ Rig rig;
  HostChannel channel(rig.platform(), rig.lines(), batching(), rig.lock());
  rig.kmt().time_out_faulted_with(static_cast<std::uint32_t>(av::FaultCode::no_executor));
  CHECK_STATUS(channel.open(rig.device()), Status::no_memory);
  rig.check_nothing_held(channel); }

TEST(host_channel, a_channel_the_host_faulted_for_lack_of_a_session_says_exhausted_and_other_faults_do_not)
{ for (std::uint32_t code = 1u; code <= 9u; code++) {
      Rig rig;
      HostChannel channel(rig.platform(), rig.lines(), plain(), rig.lock());
      CHECK_STATUS(channel.open(rig.device()), Status::ok);
      CHECK(!channel.exhausted());
      rig.kmt().time_out_faulted_with(code);
      WireWriter writer = channel.transport().request_writer();
      WireReader reply(nullptr, 0u);
      CHECK(channel.transport().send_sync(command_id, writer, reply) != TransportResult::ok);
      CHECK_EQ(channel.exhausted(), code == static_cast<std::uint32_t>(av::FaultCode::no_executor));
      channel.close(true);
  } }

TEST(host_channel, no_memory_for_the_staging_or_the_batch_opens_nothing)
{ for (std::uint32_t failing = 1u; failing <= 2u; failing++) {
      Rig rig;
      HostChannel channel(rig.platform(), rig.lines(), batching(), rig.lock());
      rig.platform().fail_allocation_at(failing);
      CHECK_STATUS(channel.open(rig.device()), Status::no_memory);
      rig.check_nothing_held(channel);
  } }

TEST(host_channel, a_driver_that_refuses_the_context_opens_nothing)
{ Rig rig;
  HostChannel channel(rig.platform(), rig.lines(), plain(), rig.lock());
  rig.kmt().fail_at(MockKmt::Call::create_context_virtual, 1u, 0xC0000001u);
  CHECK(channel.open(rig.device()) != Status::ok);
  CHECK_EQ(rig.log().holding("avtest: the driver refused the context"), 1u);
  rig.check_nothing_held(channel); }

TEST(host_channel, a_shape_that_neither_batches_nor_lends_opens_the_transport_alone)
{ Rig rig;
  HostChannel channel(rig.platform(), rig.lines(), plain(), rig.lock());
  CHECK_STATUS(channel.open(rig.device()), Status::ok);
  CHECK(channel.opened());
  CHECK(!channel.has_arena());
  CHECK_EQ(rig.commands(), 0u);
  CHECK_EQ(rig.platform().live_allocations(), 1u);
  CHECK(channel.invariants_hold());
  channel.close(false);
  rig.check_nothing_held(channel); }

TEST(host_channel, a_transport_the_host_failed_is_a_faulted_channel)
{ Rig rig;
  HostChannel channel(rig.platform(), rig.lines(), plain(), rig.lock());
  CHECK_STATUS(channel.open(rig.device()), Status::ok);
  WireWriter writer = channel.transport().request_writer();
  WireReader reply(nullptr, 0u);
  CHECK(channel.transport().send_sync(command_id, writer, reply) != TransportResult::ok);
  CHECK(channel.faulted());
  channel.close(true);
  rig.check_nothing_held(channel); }

TEST(host_channel, an_arena_that_does_not_close_whole_is_logged_and_closed_anyway)
{ Rig rig;
  HostChannel channel(rig.platform(), rig.lines(), batching(), rig.lock());
  rig.arena_imports();
  CHECK_STATUS(channel.open(rig.device()), Status::ok);
  channel.close(false);
  CHECK_EQ(rig.log().holding("avtest: the blob arena did not close whole"), 1u);
  CHECK(!channel.opened());
  CHECK_EQ(rig.platform().live_allocations(), 0u); }

TEST(host_channel, the_invariant_catches_every_state_the_channel_never_builds)
{ Rig rig;
  HostChannel      channel(rig.platform(), rig.lines(), batching(), rig.lock());
  HostChannelProbe probe(channel);
  std::uint8_t     stray = 0u;
  CHECK(probe.swap_staging(&stray) == nullptr);
  CHECK(!channel.invariants_hold());
  CHECK(probe.swap_staging(nullptr) == &stray);
  CHECK(probe.swap_batch_block(&stray) == nullptr);
  CHECK(!channel.invariants_hold());
  CHECK(probe.swap_batch_block(nullptr) == &stray);
  CHECK(channel.invariants_hold());
  rig.arena_imports();
  CHECK_STATUS(channel.open(rig.device()), Status::ok);
  std::uint8_t *const staging = probe.swap_staging(nullptr);
  CHECK(!channel.invariants_hold());
  CHECK(probe.swap_staging(staging) == nullptr);
  CHECK(channel.invariants_hold());
  probe.drop_locked();
  CHECK(!channel.invariants_hold());
  probe.restore_locked();
  CHECK(channel.invariants_hold());
  channel.transport().hold_in(nullptr, 0u);
  CHECK(!channel.invariants_hold());
  probe.restore_holding();
  CHECK(channel.invariants_hold());
  std::uint8_t *const block = probe.swap_batch_block(nullptr);
  CHECK(!channel.invariants_hold());
  CHECK(probe.swap_batch_block(block) == nullptr);
  CHECK(channel.invariants_hold());
  const std::uint32_t name = KmtTransportProbe::forget_name(channel.transport());
  CHECK(!channel.invariants_hold());
  KmtTransportProbe::restore_name(channel.transport(), name);
  CHECK(channel.invariants_hold());
  rig.host().nop();
  channel.close(false);
  rig.check_nothing_held(channel); }

TEST(host_channel, a_transport_no_open_made_is_an_open_channel_with_no_staging)
{ Rig rig;
  HostChannel      channel(rig.platform(), rig.lines(), plain(), rig.lock());
  HostChannelProbe probe(channel);
  probe.build_transport(rig.device());
  CHECK(channel.opened());
  CHECK(!channel.invariants_hold());
  probe.drop_transport();
  rig.check_nothing_held(channel); }

TEST(host_channel, the_window_decides_the_arena_and_none_leaves_bulk_data_inline)
{ const std::uint64_t whole = PublishedSlotLayout::control_bytes + KmtBlobArena::default_bytes;
  { Rig rig;
    rig.kmt().open_host_window(whole);
    HostChannel channel(rig.platform(), rig.lines(), batching(), rig.lock());
    rig.arena_imports();
    CHECK_STATUS(channel.open(rig.device()), Status::ok);
    CHECK_EQ(channel.arena_bytes(), whole);
    CHECK(channel.has_arena());
    rig.host().nop();
    channel.close(false);
    rig.check_nothing_held(channel); }
  const std::uint64_t windows[] = {0u, whole - 4096u};
  for (const std::uint64_t window : windows) {
      Rig rig;
      rig.kmt().open_host_window(window);
      HostChannel channel(rig.platform(), rig.lines(), batching(), rig.lock());
      if (window != 0u) rig.arena_imports();
      CHECK_STATUS(channel.open(rig.device()), Status::ok);
      CHECK(!channel.has_arena());
      CHECK_EQ(rig.log().holding("avtest: no blob arena for the context; bulk data crosses inline"), 1u);
      CHECK(rig.send(channel.wire()) == TransportResult::ok);
      rig.host().nop();
      channel.close(false);
      rig.check_nothing_held(channel);
  } }

namespace {

constexpr std::uint64_t first_memory = 0x0000700000000001ull;
constexpr std::uint64_t second_memory = 0x0000700000000002ull;
const HostMemoryRequest upload(HostMemoryRequest::Kind::upload, 0u);
const int               owner_token = 0;
const void *const       owner = &owner_token;

}

TEST(host_channel_memory, opens_finds_and_closes_an_allocation_per_object)
{ Rig rig;
  HostChannel channel(rig.platform(), rig.lines(), plain(), rig.lock());
  CHECK_STATUS(channel.open(rig.device()), Status::ok);
  CHECK_EQ(channel.memories(), 0u);
  HostAllocation *first = nullptr;
  HostAllocation *second = nullptr;
  rig.host().blob_import(0u, first_memory);
  CHECK_STATUS(channel.open_memory(3u * 4096u, upload, owner, first), Status::ok);
  rig.host().blob_import(0u, second_memory);
  CHECK_STATUS(channel.open_memory(4096u, HostMemoryRequest(HostMemoryRequest::Kind::vulkan, 2u), owner, second),
               Status::ok);
  CHECK(first != nullptr && second != nullptr && first != second);
  CHECK_EQ(channel.memories(), 2u);
  CHECK_EQ(first->bytes(), 3u * 4096u);
  CHECK_EQ(first->handle(), first_memory);
  CHECK(first->view() == rig.kmt().window_base());
  CHECK(second->view() == rig.kmt().window_base() + 3u * 4096u);
  CHECK_EQ(rig.kmt().last_window_request().vulkan_type(), 2u);
  CHECK(channel.find_memory(first_memory, owner) == first);
  CHECK(channel.find_memory(second_memory, owner) == second);
  CHECK(channel.find_memory(first_memory + 7u, owner) == nullptr);
  CHECK(channel.find_memory(0u, owner) == nullptr);
  rig.host().nop();
  CHECK_STATUS(channel.close_memory(*first), Status::ok);
  CHECK_EQ(rig.command_id_at(rig.commands() - 1u), av::CommandHeader::release_blob_id);
  CHECK_EQ(channel.memories(), 1u);
  CHECK(channel.find_memory(first_memory, owner) == nullptr);
  CHECK_EQ(rig.kmt().window_chunks(), 1u);
  HostAllocation stranger(rig.device(), channel.transport());
  CHECK_STATUS(channel.close_memory(stranger), Status::invalid_parameter);
  CHECK_EQ(channel.memories(), 1u);
  rig.host().nop();
  channel.close(false);
  CHECK_EQ(rig.kmt().window_chunks(), 0u);
  CHECK_EQ(rig.log().lines(), 0u);
  rig.check_nothing_held(channel, memory_tables); }

TEST(host_channel_memory, an_allocation_is_found_only_by_its_owner_and_closed_with_it)
{ Rig rig;
  HostChannel channel(rig.platform(), rig.lines(), plain(), rig.lock());
  CHECK_STATUS(channel.open(rig.device()), Status::ok);
  const int        other_token = 0;
  const void *const other = &other_token;
  HostAllocation *mine = nullptr;
  HostAllocation *theirs = nullptr;
  rig.host().blob_import(0u, first_memory);
  CHECK_STATUS(channel.open_memory(4096u, upload, owner, mine), Status::ok);
  rig.host().blob_import(0u, second_memory);
  CHECK_STATUS(channel.open_memory(4096u, upload, other, theirs), Status::ok);
  CHECK(channel.find_memory(first_memory, owner) == mine);
  CHECK(channel.find_memory(first_memory, other) == nullptr);
  CHECK(channel.find_memory(second_memory, other) == theirs);
  CHECK(channel.find_memory(second_memory, owner) == nullptr);
  rig.host().nop();
  CHECK_STATUS(channel.close_owned(owner), Status::ok);
  CHECK_EQ(channel.memories(), 1u);
  CHECK(channel.find_memory(first_memory, owner) == nullptr);
  CHECK(channel.find_memory(second_memory, other) == theirs);
  CHECK(channel.invariants_hold());
  rig.host().nop();
  CHECK_STATUS(channel.close_owned(other), Status::ok);
  CHECK_EQ(channel.memories(), 0u);
  CHECK_STATUS(channel.close_owned(other), Status::ok);
  channel.close(false);
  rig.check_nothing_held(channel, memory_tables); }

TEST(host_channel_memory, every_window_chunk_the_device_can_lend_is_kept_and_released_rows_are_reused)
{ Rig rig;
  HostChannel channel(rig.platform(), rig.lines(), plain(), rig.lock());
  CHECK_STATUS(channel.open(rig.device()), Status::ok);
  constexpr std::uint32_t most = MockKmt::max_window_chunks;
  HostAllocation         *made[most] = {};
  std::uint32_t           lent = 0u;
  std::uint32_t           tables = 0u;
  for (std::uint32_t round = 0u; round < 2u; round++) {
      std::uint32_t kept = 0u;
      for (; kept < most; kept++) {
          rig.host().blob_import(0u, first_memory + 0x1000u * (round + 1u) + kept);
          if (channel.open_memory(4096u, upload, owner, made[kept]) != Status::ok) break;
          CHECK(channel.invariants_hold());
      }
      CHECK(kept >= 8u);
      if (round == 0u) lent = kept;
      CHECK_EQ(kept, lent);
      CHECK_EQ(channel.memories(), kept);
      for (std::uint32_t index = 0u; index < kept; index++)
          CHECK(channel.find_memory(first_memory + 0x1000u * (round + 1u) + index, owner) == made[index]);
      for (std::uint32_t index = 0u; index < kept; index++) {
          rig.host().nop();
          CHECK_STATUS(channel.close_memory(*made[index]), Status::ok);
      }
      CHECK_EQ(channel.memories(), 0u);
      CHECK(channel.invariants_hold());
      if (round == 0u) tables = rig.platform().live_allocations();
  }
  CHECK_EQ(rig.platform().live_allocations(), tables);
  channel.close(false);
  rig.check_nothing_held(channel, memory_tables); }

TEST(host_channel_memory, adopts_the_processs_own_pages_as_an_allocation_it_keeps)
{ Rig rig;
  HostChannel     channel(rig.platform(), rig.lines(), plain(), rig.lock());
  alignas(4096) static std::uint8_t pages[2u * 4096u];
  const HostMemoryRequest readback(HostMemoryRequest::Kind::readback, 0u);
  HostAllocation *made = reinterpret_cast<HostAllocation *>(&rig);
  CHECK_STATUS(channel.adopt_memory(pages, sizeof pages, readback, owner, made), Status::invalid_state);
  CHECK(made == nullptr);
  CHECK_STATUS(channel.open(rig.device()), Status::ok);
  rig.host().blob_import(0u, first_memory);
  CHECK_STATUS(channel.adopt_memory(pages, sizeof pages, readback, owner, made), Status::ok);
  CHECK(made != nullptr);
  CHECK(made->view() == pages);
  CHECK(channel.find_memory(first_memory, owner) == made);
  CHECK_EQ(channel.memories(), 1u);
  rig.host().nop();
  CHECK_STATUS(channel.close_memory(*made), Status::ok);
  CHECK_EQ(channel.memories(), 0u);
  CHECK_STATUS(channel.adopt_memory(pages + 1, 4096u, readback, owner, made), Status::invalid_parameter);
  CHECK(made == nullptr);
  CHECK_EQ(channel.memories(), 0u);
  rig.platform().fail_allocation_at(1u);
  CHECK_STATUS(channel.adopt_memory(pages, sizeof pages, readback, owner, made), Status::no_memory);
  channel.close(true);
  rig.check_nothing_held(channel, memory_tables); }

TEST(host_channel_memory, a_refusal_keeps_nothing_and_a_closed_or_faulted_channel_opens_none)
{ Rig rig;
  HostChannel     channel(rig.platform(), rig.lines(), plain(), rig.lock());
  HostAllocation *made = reinterpret_cast<HostAllocation *>(&rig);
  CHECK_STATUS(channel.open_memory(4096u, upload, owner, made), Status::invalid_state);
  CHECK(made == nullptr);
  CHECK_STATUS(channel.open(rig.device()), Status::ok);
  rig.host().blob_import(av::BlobImportAnswer::executor_refused, 0u);
  CHECK_STATUS(channel.open_memory(4096u, upload, owner, made), Status::invalid_parameter);
  CHECK(made == nullptr);
  CHECK_EQ(channel.memories(), 0u);
  CHECK_EQ(rig.kmt().window_chunks(), 0u);
  CHECK_STATUS(channel.open_memory(4096u, HostMemoryRequest(), owner, made), Status::invalid_parameter);
  CHECK_EQ(channel.memories(), 0u);
  rig.platform().fail_allocation_at(1u);
  CHECK_STATUS(channel.open_memory(4096u, upload, owner, made), Status::no_memory);
  CHECK_EQ(rig.kmt().window_opens(), 1u);
  WireWriter writer = channel.transport().request_writer();
  WireReader reply(nullptr, 0u);
  CHECK(channel.transport().send_sync(command_id, writer, reply) != TransportResult::ok);
  CHECK(channel.faulted());
  CHECK_STATUS(channel.open_memory(4096u, upload, owner, made), Status::invalid_state);
  channel.close(true);
  rig.check_nothing_held(channel, memory_tables); }

TEST(host_channel_memory, a_lost_channel_abandons_its_memory_without_asking_the_host)
{ { Rig rig;
    HostChannel channel(rig.platform(), rig.lines(), plain(), rig.lock());
    CHECK_STATUS(channel.open(rig.device()), Status::ok);
    HostAllocation *made = nullptr;
    rig.host().blob_import(0u, first_memory);
    CHECK_STATUS(channel.open_memory(4096u, upload, owner, made), Status::ok);
    HostAllocation *other = nullptr;
    rig.host().blob_import(0u, second_memory);
    CHECK_STATUS(channel.open_memory(4096u, upload, owner, other), Status::ok);
    WireWriter writer = channel.transport().request_writer();
    WireReader reply(nullptr, 0u);
    CHECK(channel.transport().send_sync(command_id, writer, reply) != TransportResult::ok);
    const std::uint32_t sent = rig.commands();
    CHECK_STATUS(channel.close_memory(*made), Status::ok);
    CHECK_EQ(channel.memories(), 1u);
    channel.close(true);
    CHECK_EQ(rig.commands(), sent);
    CHECK_EQ(rig.log().lines(), 0u);
    rig.check_nothing_held(channel, memory_tables); }
  { Rig rig;
    HostChannel channel(rig.platform(), rig.lines(), plain(), rig.lock());
    CHECK_STATUS(channel.open(rig.device()), Status::ok);
    HostAllocation *made = nullptr;
    rig.host().blob_import(0u, first_memory);
    CHECK_STATUS(channel.open_memory(4096u, upload, owner, made), Status::ok);
    rig.host().nop();
    rig.kmt().fail_at(MockKmt::Call::escape, 1u, 0xC0000001u);
    channel.close(false);
    CHECK_EQ(rig.log().holding("avtest: host memory did not close whole"), 1u);
    CHECK_EQ(channel.memories(), 0u);
    CHECK(!channel.opened()); } }


TEST(host_channel, what_the_channel_sends_goes_under_its_send_lock_behind_what_is_held)
{ Rig rig;
  const ChannelShape shape(SubApi::opengl, 16u, reply_bytes, staging_bytes, 2u * staging_bytes,
                           2u * KmtBlobArena::default_bytes);
  HostChannel channel(rig.platform(), rig.lines(), shape, rig.lock());
  rig.arena_imports();
  CHECK_STATUS(channel.open(rig.device()), Status::ok);
  static_cast<void>(rig.locked());
  const std::uint32_t sent = rig.commands();
  CHECK(rig.send(channel.wire()) == TransportResult::ok);
  CHECK(std::strcmp(rig.locked(), "Ss") == 0);
  CHECK_EQ(rig.commands(), sent);
  CHECK(channel.flush() == TransportResult::ok);
  CHECK(std::strcmp(rig.locked(), "Ss") == 0);
  CHECK_EQ(rig.commands(), sent + 1u);
  CHECK(rig.send(channel.wire()) == TransportResult::ok);
  std::uint8_t framed[av::CommandHeader::encoded_size + 8u] = {};
  CHECK_STATUS(av::CommandHeader(command_id + 1u, sizeof framed, 0u).encode(framed, sizeof framed), Status::ok);
  static_cast<void>(rig.locked());
  CHECK(channel.send_batch(framed, sizeof framed) == TransportResult::ok);
  CHECK(std::strcmp(rig.locked(), "Ss") == 0);
  CHECK_EQ(rig.command_id_at(sent + 1u), command_id);
  CHECK_EQ(rig.command_id_at(sent + 2u), command_id + 1u);
  CHECK(rig.send(channel.wire()) == TransportResult::ok);
  static_cast<void>(rig.locked());
  rig.host().nop();
  CHECK_STATUS(channel.run_held(), Status::ok);
  CHECK(std::strcmp(rig.locked(), "Ss") == 0);
  CHECK_EQ(rig.command_id_at(sent + 3u), command_id);
  CHECK_EQ(rig.command_id_at(sent + 4u), av::CommandHeader::nop_id);
  std::uint8_t spill_room[64] = {};
  av::SpillBlock spill(rig.platform());
  WireWriter long_command(spill_room, sizeof spill_room, spill);
  long_command.write_zeros(3u * staging_bytes);
  CHECK(long_command.spilled());
  CHECK(channel.send_command(command_id + 2u, long_command) == TransportResult::ok);
  CHECK(channel.flush() == TransportResult::ok);
  CHECK(std::strcmp(rig.locked(), "SsSs") == 0);
  const std::uint32_t last = rig.commands() - 1u;
  CHECK_EQ(rig.command_id_at(last), command_id + 2u);
  for (std::uint32_t index = sent + 5u; index < last; index++)
      CHECK_EQ(rig.command_id_at(index), av::CommandHeader::piece_id);
  spill.release();
  HostAllocation *made = nullptr;
  rig.host().blob_import(0u, first_memory_of_lock_test);
  CHECK_STATUS(channel.open_memory(4096u, HostMemoryRequest(HostMemoryRequest::Kind::upload, 0u), owner, made), Status::ok);
  CHECK(channel.find_memory(first_memory_of_lock_test, owner) == made);
  rig.host().nop();
  CHECK_STATUS(channel.close_memory(*made), Status::ok);
  CHECK(std::strcmp(rig.locked(), "SsSsSs") == 0);
  rig.host().nop();
  channel.close(false);
  rig.check_nothing_held(channel, memory_tables); }

TEST(host_channel, a_closed_channel_sends_nothing_of_its_own)
{ Rig rig;
  HostChannel channel(rig.platform(), rig.lines(), batching(), rig.lock());
  std::uint8_t framed[av::CommandHeader::encoded_size] = {};
  CHECK(channel.flush() == TransportResult::failed);
  CHECK(channel.send_batch(framed, sizeof framed) == TransportResult::failed);
  CHECK_STATUS(channel.run_held(), Status::invalid_state);
  std::uint8_t room[16] = {};
  WireWriter   writer(room, sizeof room);
  CHECK(channel.send_command(command_id, writer) == TransportResult::failed);
  CHECK(std::strcmp(rig.locked(), "") == 0);
  CHECK_EQ(rig.commands(), 0u);
  rig.check_nothing_held(channel); }

TEST(host_channel_memory, retired_memory_stays_open_until_the_channel_reclaims_it)
{ Rig rig;
  HostChannel channel(rig.platform(), rig.lines(), plain(), rig.lock());
  CHECK_STATUS(channel.open(rig.device()), Status::ok);
  HostAllocation *first = nullptr;
  HostAllocation *second = nullptr;
  rig.host().blob_import(0u, first_memory);
  CHECK_STATUS(channel.open_memory(4096u, upload, owner, first), Status::ok);
  rig.host().blob_import(0u, second_memory);
  CHECK_STATUS(channel.open_memory(4096u, upload, owner, second), Status::ok);
  channel.retire_memory(*first);
  channel.retire_memory(*second);
  CHECK_EQ(channel.memories(), 2u);
  CHECK(channel.find_memory(first_memory, owner) == first);
  CHECK(channel.invariants_hold());
  rig.host().nop();
  rig.host().nop();
  channel.reclaim_retired();
  CHECK_EQ(channel.memories(), 0u);
  CHECK(channel.find_memory(first_memory, owner) == nullptr);
  CHECK(channel.find_memory(second_memory, owner) == nullptr);
  CHECK_EQ(rig.command_id_at(rig.commands() - 1u), av::CommandHeader::release_blob_id);
  CHECK(channel.invariants_hold());
  channel.reclaim_retired();
  CHECK_EQ(channel.memories(), 0u);
  channel.close(false);
  rig.check_nothing_held(channel, memory_tables); }

TEST(host_channel_memory, memory_retired_after_a_fence_waits_for_that_fence_and_comes_back_oldest_first)
{ Rig rig;
  HostChannel channel(rig.platform(), rig.lines(), plain(), rig.lock());
  CHECK_STATUS(channel.open(rig.device()), Status::ok);
  HostAllocation *early = nullptr;
  HostAllocation *late = nullptr;
  rig.host().blob_import(0u, first_memory);
  CHECK_STATUS(channel.open_memory(4096u, upload, owner, early), Status::ok);
  rig.host().blob_import(0u, second_memory);
  CHECK_STATUS(channel.open_memory(4096u, upload, owner, late), Status::ok);
  CHECK_STATUS(channel.retire_memory_after(*late, 9u), Status::ok);
  CHECK_STATUS(channel.retire_memory_after(*early, 4u), Status::ok);
  channel.reclaim_retired_through(3u);
  CHECK_EQ(channel.memories(), 2u);
  rig.host().nop();
  channel.reclaim_retired_through(4u);
  CHECK_EQ(channel.memories(), 1u);
  CHECK(channel.find_memory(first_memory, owner) == nullptr);
  CHECK(channel.find_memory(second_memory, owner) == late);
  HostAllocation *taken = nullptr;
  std::uint64_t   fence = 0u;
  CHECK(channel.take_oldest_retired(taken, fence));
  CHECK(taken == late);
  CHECK_EQ(fence, 9u);
  CHECK(!channel.take_oldest_retired(taken, fence));
  channel.reclaim_retired_through(100u);
  CHECK_EQ(channel.memories(), 1u);
  CHECK(channel.invariants_hold());
  rig.host().nop();
  CHECK_STATUS(channel.close_memory(*late), Status::ok);
  channel.close(false);
  rig.check_nothing_held(channel, memory_tables); }

void fault_all(void *kmt) { static_cast<MockKmt *>(kmt)->fault_every_page(); }

TEST(host_channel, a_context_the_host_faulted_opening_the_arena_is_logged_with_the_hosts_code)
{ Rig rig;
  HostChannel channel(rig.platform(), rig.lines(), batching(), rig.lock());
  rig.kmt().time_out_faulted();
  CHECK_STATUS(channel.open(rig.device()), Status::invalid_state);
  CHECK_EQ(rig.log().holding("host faulted 1 with code 7"), 1u);
  rig.check_nothing_held(channel); }

TEST(host_channel, a_channel_whose_host_faulted_is_lost_before_its_own_transport_knows)
{ Rig rig;
  HostChannel channel(rig.platform(), rig.lines(), plain(), rig.lock());
  CHECK(!channel.lost());
  CHECK_STATUS(channel.open(rig.device()), Status::ok);
  CHECK(!channel.lost());
  rig.kmt().fault_every_page();
  CHECK(channel.lost());
  CHECK(!channel.faulted());
  channel.close(true);
  CHECK(!channel.lost());
  rig.check_nothing_held(channel); }

TEST(host_channel, query_slots_come_from_the_arena_once_and_never_from_a_channel_without_one)
{ { Rig rig;
    HostChannel channel(rig.platform(), rig.lines(), plain(), rig.lock());
    CHECK_STATUS(channel.open(rig.device()), Status::ok);
    std::uint8_t *slots = nullptr;
    std::uint64_t offset = 0u;
    CHECK(!channel.take_query_slots(1u, slots, offset));
    CHECK(slots == nullptr);
    channel.close(false); }
  { Rig rig;
    HostChannel channel(rig.platform(), rig.lines(), batching(), rig.lock());
    rig.arena_imports();
    CHECK_STATUS(channel.open(rig.device()), Status::ok);
    std::uint8_t *slots = nullptr;
    std::uint64_t offset = 0u;
    CHECK(channel.take_query_slots(1u, slots, offset));
    CHECK(slots != nullptr);
    std::uint8_t *again = nullptr;
    CHECK(!channel.take_query_slots(1u, again, offset));
    CHECK(again == nullptr);
    rig.host().nop();
    channel.close(false); } }

TEST(host_channel_memory, a_refusal_is_counted_by_the_kind_of_its_reason)
{ Rig rig;
  HostChannel channel(rig.platform(), rig.lines(), plain(), rig.lock());
  CHECK_STATUS(channel.open(rig.device()), Status::ok);
  HostAllocation *made = nullptr;
  av::KmtTransport &wire = channel.transport();
  CHECK_EQ(KmtTransportProbe::counted(wire, ChannelCounter::refused_no_space), 0u);
  rig.kmt().refuse_next_window_open(0xC000009Au);
  CHECK_STATUS(channel.open_memory(4096u, upload, owner, made), Status::no_space);
  CHECK_EQ(KmtTransportProbe::counted(wire, ChannelCounter::refused_no_space), 1u);
  rig.kmt().refuse_next_window_open(0xC0000017u);
  CHECK_STATUS(channel.open_memory(4096u, upload, owner, made), Status::no_memory);
  CHECK_EQ(KmtTransportProbe::counted(wire, ChannelCounter::refused_no_memory), 1u);
  rig.kmt().refuse_next_window_open(0xC000000Du);
  CHECK_STATUS(channel.open_memory(4096u, upload, owner, made), Status::invalid_parameter);
  CHECK_EQ(KmtTransportProbe::counted(wire, ChannelCounter::refused_other), 1u);
  CHECK_EQ(KmtTransportProbe::counted(wire, ChannelCounter::refused_no_space), 1u);
  CHECK_EQ(KmtTransportProbe::counted(wire, ChannelCounter::refused_no_memory), 1u);
  channel.close(false); }

TEST(host_channel_memory, a_host_that_names_two_allocations_by_one_handle_gets_the_second_back)
{ Rig rig;
  HostChannel channel(rig.platform(), rig.lines(), plain(), rig.lock());
  CHECK_STATUS(channel.open(rig.device()), Status::ok);
  HostAllocation *first = nullptr;
  HostAllocation *second = nullptr;
  rig.host().blob_import(0u, first_memory);
  CHECK_STATUS(channel.open_memory(4096u, upload, owner, first), Status::ok);
  rig.host().blob_import(0u, first_memory);
  rig.host().nop();
  CHECK_STATUS(channel.open_memory(4096u, upload, owner, second), Status::malformed);
  CHECK(second == nullptr);
  CHECK_EQ(rig.log().holding("avtest: the host named two allocations by one handle"), 1u);
  CHECK_EQ(channel.memories(), 1u);
  CHECK(channel.find_memory(first_memory, owner) == first);
  CHECK(channel.invariants_hold());
  rig.host().nop();
  channel.close(false);
  rig.check_nothing_held(channel, memory_tables); }

TEST(host_channel_memory, an_allocation_the_channel_did_not_make_is_not_closed_by_a_handle_it_shares)
{ Rig rig;
  HostChannel channel(rig.platform(), rig.lines(), plain(), rig.lock());
  CHECK_STATUS(channel.open(rig.device()), Status::ok);
  HostAllocation *kept = nullptr;
  rig.host().blob_import(0u, first_memory);
  CHECK_STATUS(channel.open_memory(4096u, upload, owner, kept), Status::ok);
  HostAllocation stranger(rig.device(), channel.transport());
  rig.host().blob_import(0u, first_memory);
  CHECK_STATUS(stranger.open(4096u, upload), Status::ok);
  CHECK_EQ(stranger.handle(), first_memory);
  CHECK_STATUS(channel.close_memory(stranger), Status::invalid_parameter);
  CHECK_EQ(channel.memories(), 1u);
  CHECK(channel.find_memory(first_memory, owner) == kept);
  rig.host().nop();
  CHECK_STATUS(stranger.close(), Status::ok);
  rig.host().nop();
  channel.close(false);
  rig.check_nothing_held(channel, memory_tables); }

TEST(host_channel_memory, memory_let_go_of_with_no_room_to_wait_is_closed_at_once)
{ Rig rig;
  HostChannel channel(rig.platform(), rig.lines(), plain(), rig.lock());
  CHECK_STATUS(channel.open(rig.device()), Status::ok);
  HostAllocation *made = nullptr;
  rig.host().blob_import(0u, first_memory);
  CHECK_STATUS(channel.open_memory(4096u, upload, owner, made), Status::ok);
  rig.platform().fail_allocation_at(1u);
  rig.host().nop();
  channel.retire_memory(*made);
  CHECK_EQ(rig.log().holding("avtest: more memory let go of than can wait for the host"), 1u);
  CHECK_EQ(channel.memories(), 0u);
  CHECK_EQ(rig.command_id_at(rig.commands() - 1u), av::CommandHeader::release_blob_id);
  CHECK(channel.invariants_hold());
  channel.close(false);
  rig.check_nothing_held(channel, memory_tables); }

TEST(host_channel_memory, the_invariant_catches_a_handle_index_that_lost_an_allocation)
{ Rig rig;
  HostChannel      channel(rig.platform(), rig.lines(), plain(), rig.lock());
  HostChannelProbe probe(channel);
  CHECK_STATUS(channel.open(rig.device()), Status::ok);
  HostAllocation *made = nullptr;
  rig.host().blob_import(0u, first_memory);
  CHECK_STATUS(channel.open_memory(4096u, upload, owner, made), Status::ok);
  CHECK(channel.invariants_hold());
  probe.drop_handle(first_memory);
  CHECK(!channel.invariants_hold());
  channel.close(true);
  CHECK(!channel.opened()); }

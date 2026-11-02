#include "miniport/adapter/capset_query.hpp"
#include "miniport/av_context/guest_context.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/loopback/loopback_rig.hpp"
#include "harness/context/guest_ring.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/submission/fence_page.hpp"

#include <cstdint>
#include <cstring>

using av::CapsetQuery;
using av::CommandHeader;
using av::FencePage;
using av::GuestContext;
using av::GuestRing;
using av::LoopbackRig;
using av::Status;

namespace {

constexpr std::uint64_t head_offset = 16u;

std::uint8_t *page_of(LoopbackRig &rig, std::uint32_t index)
{
    return const_cast<std::uint8_t *>(rig.context(index).fence_page());
}

Status forge_page(LoopbackRig &rig, std::uint32_t index, const FencePage &page)
{
    std::uint8_t encoded[FencePage::encoded_size] = {};
    const Status status = page.encode(encoded, sizeof encoded);
    if (status == Status::ok) std::memcpy(page_of(rig, index), encoded, FencePage::beat_at);
    return status;
}

std::uint64_t completed_once(LoopbackRig &rig, std::uint32_t index)
{
    CHECK(rig.establish(index));
    GuestRing ring(rig.context(index).ring(), LoopbackRig::ring_order);
    ring.write_batch(1u, 64u);
    CHECK_STATUS(rig.submit(index, 1u, 0u, ring.tail()), Status::ok);
    CHECK(rig.driver().serve_until(rig.context(index), rig.line(index), 1u));
    return ring.tail();
}

}

TEST(loopback_hostile_guest, the_driver_refuses_what_it_can_see_and_the_device_faults_the_rest)
{ LoopbackRig rig;
  CHECK(rig.read_capset());
  CHECK_STATUS(rig.set_up(0u), Status::ok);
  CHECK_STATUS(rig.submit(0u, 1u, 0u, 24u), Status::invalid_state);
  CHECK_EQ(rig.driver().converse(rig.context(0u)), 5u);
  GuestRing ring(rig.context(0u).ring(), LoopbackRig::ring_order);
  const std::uint32_t stores = rig.device().doorbells().stores();
  CHECK_STATUS(rig.submit(0u, 1u, 8u, 32u), Status::invalid_parameter);
  ring.write_batch(1u, 64u);
  const std::uint64_t first_end = ring.tail();
  CHECK_STATUS(rig.submit(0u, 1u, 0u, first_end), Status::ok);
  CHECK_STATUS(rig.submit(0u, 2u, 0u, first_end), Status::invalid_parameter);
  CHECK_STATUS(rig.submit(0u, 1u, first_end, first_end + 24u), Status::invalid_parameter);
  CHECK_EQ(rig.device().doorbells().stores(), stores + 1u);
  CHECK(rig.driver().serve_until(rig.context(0u), rig.line(0u), 1u));
  std::uint8_t misframed[16] = {};
  CHECK_STATUS(CommandHeader(GuestRing::filler_command, 64u, 0u).encode(misframed, sizeof misframed), Status::ok);
  ring.write_batch(2u, misframed, sizeof misframed);
  CHECK_STATUS(rig.submit(0u, 2u, first_end, ring.tail()), Status::ok);
  CHECK(!rig.driver().serve_until(rig.context(0u), rig.line(0u), 2u));
  CHECK(rig.context(0u).faulted());
  CHECK_EQ(rig.context(0u).fault_code(), 5u);
  CHECK(rig.context(0u).invariants_hold());
  CHECK(rig.retire(0u)); }

TEST(loopback_hostile_device, a_malformed_answer_fails_the_query_or_the_setup_and_teardown_releases_what_was_made)
{ LoopbackRig rig;
  CapsetQuery cut(rig.platform());
  rig.driver().truncate_answer(1u, 8u);
  CHECK(rig.driver().query(cut) != Status::ok);
  CHECK(cut.failed());
  CHECK(rig.read_capset());
  CHECK_STATUS(rig.set_up(0u), Status::ok);
  rig.driver().truncate_answer(3u, 8u);
  CHECK_EQ(rig.driver().converse(rig.context(0u)), 3u);
  CHECK(rig.context(0u).state() == GuestContext::State::failed);
  CHECK_EQ(rig.device().device().contexts(), 1u);
  CHECK_EQ(rig.device().device().blobs(), 2u);
  CHECK(rig.retire(0u));
  CHECK_EQ(rig.device().device().contexts(), 0u);
  CHECK_EQ(rig.device().device().blobs(), 0u);
  CHECK(rig.context(0u).invariants_hold()); }

TEST(loopback_hostile_device, a_refused_blob_fails_the_setup_and_teardown_releases_only_the_context)
{ LoopbackRig rig;
  CHECK(rig.read_capset());
  CHECK(rig.establish(1u));
  CHECK_STATUS(rig.set_up(0u), Status::ok);
  rig.driver().tamper_answer(2u, 1u, 0x12u);
  CHECK_EQ(rig.driver().converse(rig.context(0u)), 2u);
  CHECK(rig.context(0u).state() == GuestContext::State::failed);
  CHECK_EQ(rig.device().device().contexts(), 2u);
  CHECK(rig.retire(0u));
  CHECK_EQ(rig.device().device().contexts(), 1u);
  CHECK_EQ(rig.device().device().blobs(), 4u);
  CHECK(rig.context(1u).state() == GuestContext::State::established); }

TEST(loopback_hostile_device, a_fence_page_that_lies_fails_the_context)
{ LoopbackRig rig;
  CHECK(rig.read_capset());
  static_cast<void>(completed_once(rig, 0u));
  CHECK_STATUS(forge_page(rig, 0u, FencePage(0u, 0u)), Status::ok);
  CHECK(rig.serve(0u) != Status::ok);
  CHECK(rig.context(0u).state() == GuestContext::State::failed);
  CHECK(rig.retire(0u));
  static_cast<void>(completed_once(rig, 0u));
  CHECK_STATUS(forge_page(rig, 0u, FencePage(5u, 3u)), Status::ok);
  CHECK(rig.serve(0u) != Status::ok);
  CHECK(rig.context(0u).state() == GuestContext::State::failed);
  CHECK(rig.retire(0u));
  static_cast<void>(completed_once(rig, 0u));
  CHECK_STATUS(forge_page(rig, 0u, FencePage(1u, 2u, 3u)), Status::ok);
  CHECK_STATUS(rig.serve(0u), Status::ok);
  CHECK(rig.context(0u).faulted());
  CHECK_STATUS(forge_page(rig, 0u, FencePage(1u, 2u)), Status::ok);
  CHECK(rig.serve(0u) != Status::ok);
  CHECK(rig.context(0u).state() == GuestContext::State::failed);
  CHECK(rig.context(0u).invariants_hold());
  CHECK(rig.retire(0u)); }

TEST(loopback_hostile_device, a_head_past_the_tail_fails_the_context_without_ringing)
{ LoopbackRig rig;
  CHECK(rig.read_capset());
  const std::uint64_t tail = completed_once(rig, 0u);
  GuestRing ring(rig.context(0u).ring(), LoopbackRig::ring_order);
  const std::uint32_t stores = rig.device().doorbells().stores();
  ring.store(head_offset, tail + 64u);
  CHECK_STATUS(rig.submit(0u, 2u, tail, tail + 24u), Status::invalid_state);
  CHECK(rig.context(0u).state() == GuestContext::State::failed);
  CHECK_EQ(rig.device().doorbells().stores(), stores);
  CHECK(rig.context(0u).invariants_hold());
  CHECK(rig.retire(0u)); }

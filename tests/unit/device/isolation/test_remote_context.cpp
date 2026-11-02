#include "core/support/status.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "device/isolation/remote_context.hpp"
#include "device/isolation/renderer_link.hpp"
#include "device/memory/blob_table.hpp"
#include "device/control/control_request.hpp"
#include "device/memory/blob_window.hpp"
#include "harness/host/gpu_request_builder.hpp"
#include "harness/host/mock_request.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/scripted_renderer_peer.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/context_setup.hpp"
#include "proto/context/gl_context_options.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/renderer/bind_context_message.hpp"
#include "proto/submission/fault_code.hpp"
#include "proto/submission/fence_page.hpp"

#include <cstdint>
#include <cstring>

using av::BlobTable;
using av::MockHost;
using av::RemoteContext;
using av::ScriptedRendererPeer;
using av::Status;
using Kind = av::RendererMessageHeader::Kind;

namespace {

constexpr std::uint32_t ring = 1u, fence = 2u, context_id = 9u, doorbell = 5u;
constexpr std::uint64_t ram = 0x100000u;
constexpr std::uint8_t  uuid[av::GpuIdentity::uuid_bytes] = {7u};

av::CapsetInfo capset()
{
    return av::CapsetInfo(av::ProtocolVersion(1u, 1u),
                          static_cast<std::uint32_t>(av::SubApi::vulkan) | static_cast<std::uint32_t>(av::SubApi::opengl),
                          64u, 12u, 24u, 0u, av::GpuIdentity(0u, 0u, 0u, uuid, 0u, 0u, ""));
}

av::ContextSetup setup(std::uint32_t ring_id = ring)
{
    return av::ContextSetup(av::ProtocolVersion(1u, 0u), av::SubApi::vulkan, ring_id, 12u, fence, doorbell, 0u);
}

class Fixture {
public:
    Fixture() : blobs_(host_), interrupts_(host_), peer_(host_), link_(host_), context_(host_, blobs_, interrupts_, context_id)
    {
        host_.add_guest_ram(ram, 0x3000u);
        static_cast<void>(blobs_.initialize());
        guest_blob(ring, 0u, 0x2000u);
        guest_blob(fence, 0x2000u, 0x1000u);
        static_cast<void>(interrupts_.initialize());
        static_cast<void>(interrupts_.connect());
        host_.connect_renderer(peer_);
        static_cast<void>(host_.start_renderer("av-renderer"));
        events_ = host_.live_events();
    }

    MockHost             &host() { return host_; }
    BlobTable            &blobs() { return blobs_; }
    void guest_blob(std::uint32_t id, std::uint64_t offset, std::uint64_t size)
    {
        av::GpuRequestBuilder entries(0u);
        entries.u64(ram + offset).u32(static_cast<std::uint32_t>(size)).u32(0u);
        av::MockRequest    mock(entries.bytes(), entries.size());
        av::ControlRequest request(host_, &mock);
        CHECK_STATUS(blobs_.create_guest(id, size, request, 24u, 1u), Status::ok);
    }
    ScriptedRendererPeer &peer() { return peer_; }
    av::RendererLink     &link() { return link_; }
    RemoteContext        &context() { return context_; }
    av::FenceInterrupts  &interrupts() { return interrupts_; }
    std::uint32_t         events() const { return events_; }

    Status bind(Status &outcome, const av::ContextSetup &with = setup())
    {
        return context_.bind(with, capset(), link_, outcome);
    }
    bool pinned(std::uint32_t id)
    {
        av::BlobWindow window;
        return blobs_.window(id, window) == Status::ok;
    }
    void publish(const av::FencePage &page)
    {
        std::uint8_t bytes[av::FencePage::encoded_size] = {};
        static_cast<void>(page.encode(bytes, sizeof bytes));
        av::BlobWindow window;
        static_cast<void>(blobs_.window(fence, window));
        static_cast<void>(window.write(0u, bytes, sizeof bytes));
    }
    av::FencePage page()
    {
        std::uint8_t   bytes[av::FencePage::encoded_size] = {};
        av::BlobWindow window;
        av::FencePage  read;
        if (blobs_.window(fence, window) == Status::ok && window.read(0u, bytes, sizeof bytes) == Status::ok)
            static_cast<void>(read.decode(bytes, sizeof bytes));
        return read;
    }

private:
    MockHost             host_;
    BlobTable            blobs_;
    av::FenceInterrupts  interrupts_;
    ScriptedRendererPeer peer_;
    av::RendererLink     link_;
    RemoteContext        context_;
    std::uint32_t        events_ = 0u;
};

}

TEST(remote_context_bind, pins_registers_the_doorbell_and_sends_its_descriptor_with_the_setup)
{ Fixture fixture;
  CHECK(!fixture.context().bound());
  CHECK_EQ(fixture.context().id(), context_id);
  Status outcome = Status::invalid_state;
  CHECK_STATUS(fixture.bind(outcome), Status::ok);
  CHECK_STATUS(outcome, Status::ok);
  CHECK(fixture.context().bound());
  CHECK(!fixture.context().lost());
  CHECK(fixture.context().sub_api() == av::SubApi::vulkan);
  CHECK_EQ(fixture.context().doorbell(), doorbell);
  CHECK(fixture.pinned(ring));
  CHECK(fixture.pinned(fence));
  CHECK_EQ(fixture.host().live_doorbells(), 1u);
  CHECK_EQ(fixture.host().live_events(), fixture.events() + 1u);
  CHECK(fixture.host().ring_doorbell(doorbell));
  CHECK_EQ(fixture.peer().delivered(), 1u);
  CHECK(fixture.peer().kind_at(0u) == Kind::bind_context);
  CHECK_EQ(fixture.peer().descriptors_at(0u), 1u);
  CHECK_EQ(fixture.peer().open_descriptors_at(0u), 1u);
  av::BindContextMessage message;
  CHECK_STATUS(message.decode(fixture.peer().body_at(0u), fixture.peer().body_length_at(0u), 1u), Status::ok);
  CHECK_EQ(message.context(), context_id);
  CHECK(message.has_doorbell());
  CHECK_EQ(message.doorbell(), doorbell);
  std::uint8_t expected[av::ContextSetup::encoded_size] = {};
  CHECK_STATUS(setup().encode(expected, sizeof expected), Status::ok);
  CHECK_EQ(message.setup_length(), static_cast<std::uint32_t>(setup().size()));
  CHECK(std::memcmp(message.setup(), expected, setup().size()) == 0);
  CHECK(fixture.context().invariants_hold());
  CHECK_STATUS(fixture.bind(outcome), Status::ok);
  CHECK_STATUS(outcome, Status::invalid_state);
  CHECK_EQ(fixture.peer().delivered(), 1u);
  CHECK_EQ(fixture.host().live_doorbells(), 1u);
  CHECK(fixture.context().invariants_hold()); }

TEST(remote_context_bind, every_refusal_unwinds_pins_doorbell_and_event)
{ struct Case {
      const char *name;
      int         script;
      Status      link;
      Status      outcome;
  };
  const Case cases[] = {
      {"refused", 0, Status::ok, Status::no_memory},
      {"silent", 1, Status::invalid_state, Status::invalid_state},
      {"gone", 2, Status::not_found, Status::invalid_state},
      {"dies", 3, Status::not_found, Status::invalid_state},
  };
  for (const Case &test : cases) {
      Fixture fixture;
      if (test.script == 0) fixture.peer().answer_requests_with(Status::no_memory);
      if (test.script == 1) fixture.peer().fall_silent();
      if (test.script == 2) fixture.host().renderer_gone();
      if (test.script == 3) fixture.peer().die_on_next_delivery();
      Status outcome = Status::ok;
      CHECK_STATUS(fixture.bind(outcome), test.link);
      CHECK_STATUS(outcome, test.outcome);
      CHECK(!fixture.context().bound());
      CHECK(!fixture.pinned(ring));
      CHECK(!fixture.pinned(fence));
      CHECK_EQ(fixture.host().live_doorbells(), 0u);
      CHECK_EQ(fixture.host().live_events(), fixture.events());
      CHECK(!fixture.host().ring_doorbell(doorbell));
      CHECK(fixture.context().invariants_hold());
  } }

TEST(remote_context_bind, qemus_own_refusals_send_nothing)
{ { Fixture fixture;
    Status outcome = Status::ok;
    CHECK_STATUS(fixture.bind(outcome, setup(77u)), Status::ok);
    CHECK_STATUS(outcome, Status::not_found);
    CHECK(!fixture.pinned(fence));
    CHECK_EQ(fixture.peer().delivered(), 0u); }
  { Fixture fixture;
    Status outcome = Status::ok;
    const av::ContextSetup ring_too_small(av::ProtocolVersion(1u, 0u), av::SubApi::vulkan, ring, 30u, fence, doorbell, 0u);
    CHECK_STATUS(fixture.bind(outcome, ring_too_small), Status::ok);
    CHECK_STATUS(outcome, Status::not_supported);
    CHECK_EQ(fixture.peer().delivered(), 0u); }
  { Fixture fixture;
    Status outcome = Status::ok;
    const av::ContextSetup vulkan_with_gl(
        av::ProtocolVersion(1u, 1u), av::SubApi::vulkan, ring, 12u, fence, doorbell, 0u,
        av::GlContextOptions(4u, 6u, av::GlContextOptions::Profile::core, 0u, 0u));
    CHECK_STATUS(fixture.bind(outcome, vulkan_with_gl), Status::ok);
    CHECK_STATUS(outcome, Status::invalid_parameter);
    CHECK(!fixture.pinned(ring));
    CHECK_EQ(fixture.peer().delivered(), 0u); }
  { Fixture fixture;
    fixture.host().fail_event_descriptor_at(1u);
    Status outcome = Status::ok;
    CHECK_STATUS(fixture.bind(outcome), Status::ok);
    CHECK_STATUS(outcome, Status::not_supported);
    CHECK(!fixture.pinned(ring));
    CHECK_EQ(fixture.host().live_events(), fixture.events());
    CHECK_EQ(fixture.host().live_doorbells(), 0u);
    CHECK_EQ(fixture.peer().delivered(), 0u); }
  { Fixture fixture;
    RemoteContext nameless(fixture.host(), fixture.blobs(), fixture.interrupts(), 0u);
    Status outcome = Status::ok;
    CHECK_STATUS(nameless.bind(setup(), capset(), fixture.link(), outcome), Status::ok);
    CHECK_STATUS(outcome, Status::invalid_parameter);
    CHECK(!nameless.bound());
    CHECK(!fixture.pinned(ring));
    CHECK_EQ(fixture.host().live_doorbells(), 0u);
    CHECK_EQ(fixture.host().live_events(), fixture.events());
    CHECK_EQ(fixture.peer().delivered(), 0u); }
  Fixture fixture;
  fixture.host().fail_event_at(1u);
  Status outcome = Status::ok;
  CHECK_STATUS(fixture.bind(outcome), Status::ok);
  CHECK_STATUS(outcome, Status::no_memory);
  CHECK(!fixture.pinned(ring));
  CHECK_EQ(fixture.host().live_events(), fixture.events());
  CHECK_EQ(fixture.peer().delivered(), 0u); }

TEST(remote_context_lose, republishes_the_fence_page_as_device_lost_and_raises_the_interrupt)
{ Fixture fixture;
  Status outcome = Status::invalid_state;
  CHECK_STATUS(fixture.bind(outcome), Status::ok);
  fixture.publish(av::FencePage(41u, 90u));
  const std::uint32_t events = fixture.host().live_events();
  fixture.context().lose();
  CHECK(fixture.context().lost());
  CHECK(fixture.context().bound());
  const av::FencePage page = fixture.page();
  CHECK(page.faulted());
  CHECK_EQ(page.completed_fence(), 41u);
  CHECK_EQ(page.progress(), 91u);
  CHECK_EQ(page.fault_code(), static_cast<std::uint32_t>(av::FaultCode::device_lost));
  CHECK_EQ(page.fault_code(), 7u);
  CHECK_EQ(fixture.host().raised(doorbell), 1u);
  CHECK_EQ(fixture.host().live_doorbells(), 0u);
  CHECK_EQ(fixture.host().live_events(), events - 1u);
  CHECK(!fixture.host().ring_doorbell(doorbell));
  CHECK(fixture.pinned(ring));
  CHECK(fixture.context().invariants_hold());
  fixture.context().lose();
  CHECK_EQ(fixture.page().progress(), 91u);
  CHECK_EQ(fixture.host().raised(doorbell), 1u);
  CHECK_STATUS(fixture.bind(outcome), Status::ok);
  CHECK_STATUS(outcome, Status::invalid_state);
  CHECK_EQ(fixture.peer().delivered(), 1u);
  fixture.context().unbind();
  CHECK(!fixture.pinned(ring));
  CHECK(fixture.context().invariants_hold());
  CHECK_STATUS(fixture.bind(outcome), Status::ok);
  CHECK_STATUS(outcome, Status::invalid_state);
  CHECK(!fixture.pinned(ring));
  CHECK_EQ(fixture.peer().delivered(), 1u); }

TEST(remote_context_lose, a_page_that_does_not_decode_is_reported_from_nothing)
{ Fixture fixture;
  Status outcome = Status::invalid_state;
  CHECK_STATUS(fixture.bind(outcome), Status::ok);
  fixture.context().lose();
  const av::FencePage page = fixture.page();
  CHECK(page.faulted());
  CHECK_EQ(page.completed_fence(), 0u);
  CHECK_EQ(page.progress(), 1u);
  CHECK_EQ(page.fault_code(), 7u); }

TEST(remote_context_lose, an_unbound_context_has_nothing_to_lose)
{ Fixture fixture;
  fixture.context().lose();
  CHECK(!fixture.context().lost());
  CHECK_EQ(fixture.host().raised(doorbell), 0u);
  Status outcome = Status::invalid_state;
  CHECK_STATUS(fixture.bind(outcome), Status::ok);
  CHECK_STATUS(outcome, Status::ok); }

TEST(remote_context_unbind, releases_pins_and_doorbell_and_sends_nothing)
{ Fixture fixture;
  Status outcome = Status::invalid_state;
  CHECK_STATUS(fixture.bind(outcome), Status::ok);
  fixture.context().unbind();
  CHECK(!fixture.context().bound());
  CHECK(!fixture.pinned(ring));
  CHECK_EQ(fixture.host().live_doorbells(), 0u);
  CHECK_EQ(fixture.host().live_events(), fixture.events());
  CHECK_EQ(fixture.peer().delivered(), 1u);
  CHECK(fixture.context().invariants_hold());
  { RemoteContext other(fixture.host(), fixture.blobs(), fixture.interrupts(), 3u);
    CHECK_STATUS(other.bind(setup(), capset(), fixture.link(), outcome), Status::ok);
    CHECK_STATUS(outcome, Status::ok);
    CHECK(fixture.pinned(ring));
    CHECK_EQ(fixture.host().live_doorbells(), 1u); }
  CHECK(!fixture.pinned(ring));
  CHECK_EQ(fixture.host().live_doorbells(), 0u);
  CHECK_EQ(fixture.host().live_events(), fixture.events());
  CHECK_EQ(fixture.peer().delivered(), 2u); }

#include "core/platform/platform.hpp"
#include "miniport/av_context/context_memory.hpp"
#include "miniport/av_context/context_pages.hpp"
#include "miniport/av_context/guest_context.hpp"
#include "core/support/status.hpp"
#include "harness/virtio/fake_doorbell_pages.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/context_setup.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/submission/fence_page.hpp"
#include "proto/submission/submission_descriptor.hpp"
#include "proto/virtio_gpu/gpu_response.hpp"
#include "proto/virtio_gpu/av_request_writer.hpp"

#include <cstdint>
#include <cstring>

using av::CapsetInfo;
using av::ContextMemory;
using av::ContextPages;
using av::ContextSetup;
using av::FakeDoorbellPages;
using av::FencePage;
using av::GpuIdentity;
using av::GpuResponse;
using av::GuestContext;
using av::Irql;
using av::MockPlatform;
using av::ProtocolVersion;
using av::AvRequestWriter;
using av::Status;
using av::SubApi;
using av::SubmissionDescriptor;
using av::Violation;

namespace av {

struct GuestContextProbe {
    static void establish_unattached(GuestContext &context) { context.state_ = GuestContext::State::established; }
    static void attach_ring_only(GuestContext &context)
    {
        static_cast<void>(context.submitter_.attach(context.pages_.ring_view(), 12u, 0u));
    }
    static void hold_while_idle(GuestContext &context, const ContextPages &pages)
    {
        context.pages_ = pages;
        context.state_ = GuestContext::State::idle;
    }
};

}

namespace {

constexpr std::uint8_t uuid[GpuIdentity::uuid_bytes] = {};

CapsetInfo capset(std::uint32_t sub_apis = 1u)
{
    return CapsetInfo(ProtocolVersion(1u, 0u), sub_apis, 64u, 12u, 20u, 0u,
                      GpuIdentity(0u, 0u, 0u, uuid, 0u, 0u, "gpu"));
}

ContextSetup setup(std::uint8_t order = 12u, SubApi api = SubApi::vulkan)
{
    return ContextSetup(ProtocolVersion(1u, 0u), api, 10u, order, 11u, 2u, 12u);
}

class Fixture {
public:
    Fixture() : memory_(platform_), context_(platform_, doorbells_) { CHECK_STATUS(memory_.allocate(12u, 4096u), Status::ok); }
    ~Fixture() { platform_.set_irql(Irql::passive); }

    MockPlatform      &platform() { return platform_; }
    FakeDoorbellPages &doorbells() { return doorbells_; }
    ContextMemory     &memory() { return memory_; }
    GuestContext      &context() { return context_; }
    ContextPages       pages() const { return memory_.pages(); }

    std::uint32_t answer_all(std::uint32_t type = GpuResponse::ok_nodata)
    {
        std::uint32_t answered = 0u;
        std::uint8_t  bytes[AvRequestWriter::max_request_size] = {};
        std::size_t   length = 0u;
        platform_.set_irql(Irql::dispatch);
        while (context_.next_request(bytes, sizeof bytes, length) == Status::ok) {
            GpuResponse response;
            response.set(type);
            static_cast<void>(context_.accept(response.bytes(), response.length()));
            answered++;
        }
        platform_.set_irql(Irql::passive);
        CHECK(context_.invariants_hold());
        return answered;
    }
    void established()
    {
        CHECK_STATUS(context_.set_up(capset(), 3u, setup(), memory_.pages()), Status::ok);
        CHECK_EQ(answer_all(), 5u);
        CHECK(context_.state() == GuestContext::State::established);
        static_cast<void>(FencePage(0u, 0u).encode(page(), av::FencePage::encoded_size));
    }
    std::uint8_t *page() { return const_cast<std::uint8_t *>(context_.fence_page()); }
    Status at_dispatch(Status (GuestContext::*call)())
    {
        platform_.set_irql(Irql::dispatch);
        const Status status = (context_.*call)();
        platform_.set_irql(Irql::passive);
        CHECK(context_.invariants_hold());
        return status;
    }
    Status submit(std::uint64_t fence, std::uint64_t begin, std::uint64_t end)
    {
        platform_.set_irql(Irql::dispatch);
        const Status status = context_.submit(SubmissionDescriptor(fence, begin, end));
        platform_.set_irql(Irql::passive);
        return status;
    }

private:
    MockPlatform      platform_;
    FakeDoorbellPages doorbells_;
    ContextMemory     memory_;
    GuestContext      context_;
};

}

TEST(guest_context, sets_up_submits_serves_and_tears_down)
{ Fixture fixture;
  CHECK(fixture.context().state() == GuestContext::State::idle);
  CHECK(fixture.context().ring() == nullptr && fixture.context().fence_page() == nullptr);
  fixture.established();
  CHECK(fixture.context().ring() == fixture.memory().pages().ring_view());
  CHECK(fixture.context().fence_page() == fixture.memory().pages().fence_view());
  CHECK_STATUS(fixture.submit(1u, 0u, 24u), Status::ok);
  CHECK_EQ(fixture.doorbells().rings(2u), 1u);
  CHECK(fixture.context().tail() == 24u);
  static_cast<void>(FencePage(1u, 2u).encode(fixture.page(), av::FencePage::encoded_size));
  CHECK_STATUS(fixture.at_dispatch(&GuestContext::serve), Status::ok);
  CHECK_EQ(fixture.context().completed(), 1u);
  CHECK(!fixture.context().faulted());
  CHECK_STATUS(fixture.at_dispatch(&GuestContext::tear_down), Status::ok);
  CHECK(fixture.context().state() == GuestContext::State::tearing_down);
  CHECK_STATUS(fixture.submit(2u, 24u, 48u), Status::invalid_state);
  CHECK_EQ(fixture.answer_all(), 4u);
  CHECK(fixture.context().state() == GuestContext::State::torn_down);
  CHECK_STATUS(fixture.context().release(), Status::ok);
  CHECK(fixture.context().state() == GuestContext::State::idle);
  CHECK(fixture.context().ring() == nullptr);
  CHECK(fixture.context().fence_page() == nullptr);
  fixture.established(); }

TEST(guest_context, a_fault_is_reported_and_the_context_still_tears_down)
{ Fixture fixture;
  fixture.established();
  CHECK_STATUS(fixture.submit(1u, 0u, 24u), Status::ok);
  static_cast<void>(FencePage(0u, 1u, 6u).encode(fixture.page(), av::FencePage::encoded_size));
  CHECK_STATUS(fixture.at_dispatch(&GuestContext::serve), Status::ok);
  CHECK(fixture.context().faulted());
  CHECK_EQ(fixture.context().fault_code(), 6u);
  CHECK(fixture.context().state() == GuestContext::State::established);
  CHECK_STATUS(fixture.at_dispatch(&GuestContext::tear_down), Status::ok);
  CHECK_EQ(fixture.answer_all(), 4u); }

TEST(guest_context, a_device_that_lies_fails_the_context)
{ Fixture ring_lie;
  ring_lie.established();
  CHECK_STATUS(ring_lie.submit(1u, 0u, 24u), Status::ok);
  const std::uint64_t head = 999u;
  std::memcpy(ring_lie.context().ring() + 16u, &head, 8u);
  CHECK_STATUS(ring_lie.submit(2u, 24u, 48u), Status::invalid_state);
  CHECK(ring_lie.context().state() == GuestContext::State::failed);
  CHECK_STATUS(ring_lie.submit(2u, 24u, 48u), Status::invalid_state);
  CHECK_STATUS(ring_lie.at_dispatch(&GuestContext::serve), Status::invalid_state);
  Fixture page_lie;
  page_lie.established();
  static_cast<void>(FencePage(5u, 1u).encode(page_lie.page(), av::FencePage::encoded_size));
  CHECK_STATUS(page_lie.at_dispatch(&GuestContext::serve), Status::invalid_state);
  CHECK(page_lie.context().state() == GuestContext::State::failed);
  CHECK_STATUS(page_lie.at_dispatch(&GuestContext::tear_down), Status::ok);
  CHECK_EQ(page_lie.answer_all(), 4u);
  CHECK_STATUS(page_lie.context().release(), Status::ok); }

TEST(guest_context, a_refused_setup_fails_the_context_and_releases_what_was_created)
{ Fixture fixture;
  CHECK_STATUS(fixture.context().set_up(capset(), 3u, setup(), fixture.pages()), Status::ok);
  std::uint8_t bytes[AvRequestWriter::max_request_size] = {};
  std::size_t  length = 0u;
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_STATUS(fixture.context().next_request(bytes, sizeof bytes, length), Status::ok);
  GpuResponse ok;
  ok.set(GpuResponse::ok_nodata);
  CHECK_STATUS(fixture.context().accept(ok.bytes(), ok.length()), Status::ok);
  CHECK_STATUS(fixture.context().submit(SubmissionDescriptor(1u, 0u, 24u)), Status::invalid_state);
  CHECK_STATUS(fixture.context().tear_down(), Status::invalid_state);
  CHECK_STATUS(fixture.context().next_request(bytes, sizeof bytes, length), Status::ok);
  GpuResponse refused;
  refused.set(GpuResponse::err_out_of_memory);
  CHECK_STATUS(fixture.context().accept(refused.bytes(), refused.length()), Status::not_supported);
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.context().state() == GuestContext::State::failed);
  CHECK_STATUS(fixture.context().release(), Status::invalid_state);
  CHECK_STATUS(fixture.at_dispatch(&GuestContext::tear_down), Status::ok);
  CHECK_EQ(fixture.answer_all(), 1u);
  CHECK_STATUS(fixture.context().release(), Status::ok); }

TEST(guest_context, a_setup_refused_before_anything_exists_needs_no_teardown)
{ Fixture fixture;
  CHECK_STATUS(fixture.context().set_up(capset(), 3u, setup(), fixture.pages()), Status::ok);
  CHECK_EQ(fixture.answer_all(GpuResponse::err_invalid_context_id), 1u);
  CHECK_STATUS(fixture.at_dispatch(&GuestContext::tear_down), Status::ok);
  CHECK(fixture.context().state() == GuestContext::State::torn_down);
  CHECK_EQ(fixture.answer_all(), 0u);
  CHECK_STATUS(fixture.context().release(), Status::ok);
  CHECK(fixture.context().invariants_hold());
  fixture.established(); }

TEST(guest_context_set_up, a_setup_the_capset_does_not_allow_sets_nothing_up)
{ Fixture fixture;
  CHECK_STATUS(fixture.context().set_up(capset(2u), 3u, setup(), fixture.pages()), Status::invalid_parameter);
  CHECK_STATUS(fixture.context().set_up(capset(), 3u, setup(21u), fixture.pages()), Status::invalid_parameter);
  CHECK(fixture.context().state() == GuestContext::State::idle);
  CHECK(fixture.context().invariants_hold());
  CHECK_STATUS(fixture.context().set_up(capset(), 3u, setup(), fixture.pages()), Status::ok);
  CHECK_STATUS(fixture.context().set_up(capset(), 3u, setup(), fixture.pages()), Status::invalid_state); }

TEST(guest_context_set_up, pages_not_complete_set_nothing_up)
{ Fixture fixture;
  ContextMemory unallocated(fixture.platform());
  CHECK_STATUS(fixture.context().set_up(capset(), 3u, setup(), unallocated.pages()), Status::invalid_parameter);
  CHECK_STATUS(fixture.context().set_up(capset(), 3u, setup(), ContextPages()), Status::invalid_parameter);
  CHECK(fixture.context().state() == GuestContext::State::idle);
  CHECK(fixture.context().ring() == nullptr);
  CHECK_EQ(fixture.answer_all(), 0u);
  fixture.established(); }

TEST(guest_context_set_up, publishes_into_the_ring_it_was_handed)
{ Fixture fixture;
  fixture.established();
  CHECK_STATUS(fixture.submit(1u, 0u, 24u), Status::ok);
  std::uint64_t tail = 0u;
  std::memcpy(&tail, fixture.memory().pages().ring_view() + 8u, 8u);
  CHECK(tail == 24u); }

TEST(guest_context_abandon, a_device_reset_releases_everything_the_device_held)
{ Fixture fixture;
  fixture.established();
  fixture.platform().set_irql(Irql::dispatch);
  fixture.context().abandon();
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.context().state() == GuestContext::State::torn_down);
  CHECK_STATUS(fixture.submit(1u, 0u, 24u), Status::invalid_state);
  CHECK_STATUS(fixture.context().release(), Status::ok);
  CHECK_STATUS(fixture.context().release(), Status::ok);   }

TEST(guest_context, runs_at_the_levels_it_declares)
{ Fixture fixture;
  fixture.platform().set_irql(Irql::isr);
  std::uint8_t bytes[8] = {};
  std::size_t  length = 0u;
  static_cast<void>(fixture.context().next_request(bytes, sizeof bytes, length));
  static_cast<void>(fixture.context().accept(bytes, 0u));
  static_cast<void>(fixture.context().submit(SubmissionDescriptor(1u, 0u, 1u)));
  static_cast<void>(fixture.context().serve());
  std::uint64_t progress = 0u;
  fixture.context().user_mode_submits(&progress);
  static_cast<void>(fixture.context().forget_progress(&progress));
  fixture.context().driver_submits();
  static_cast<void>(fixture.context().tear_down());
  fixture.context().abandon();
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.context().release());
  static_cast<void>(fixture.context().set_up(capset(), 3u, setup(), fixture.pages()));
  fixture.platform().set_irql(Irql::passive);
  while (fixture.platform().claim(Violation::irql_ceiling)) {}
  while (fixture.platform().claim(Violation::lock_order)) {}
  while (fixture.platform().claim(Violation::release_order)) {}
  fixture.platform().claim(Violation::allocation_at_dispatch); }

TEST(guest_context_invariants, an_established_context_with_nothing_attached_is_detected)
{ Fixture fixture;
  CHECK(fixture.context().invariants_hold());
  CHECK_STATUS(fixture.context().set_up(capset(), 3u, setup(), fixture.pages()), Status::ok);
  av::GuestContextProbe::establish_unattached(fixture.context());
  CHECK(!fixture.context().invariants_hold());
  av::GuestContextProbe::attach_ring_only(fixture.context());
  CHECK(!fixture.context().invariants_hold());
  Fixture idle;
  av::GuestContextProbe::hold_while_idle(idle.context(), idle.pages());
  CHECK(!idle.context().invariants_hold());
  Fixture released;
  av::GuestContextProbe::establish_unattached(released.context());
  CHECK(!released.context().invariants_hold()); }

TEST(guest_context, a_context_user_mode_submits_on_reports_its_progress_where_the_process_reads_it)
{ Fixture      fixture;
  std::uint64_t progress = 0u;
  fixture.established();
  fixture.platform().set_irql(Irql::dispatch);
  fixture.context().user_mode_submits(&progress);
  fixture.platform().set_irql(Irql::passive);

  std::uint64_t tail = 96u;
  std::memcpy(fixture.memory().pages().ring_view() + 8u, &tail, 8u);
  static_cast<void>(FencePage(3u, 96u).encode(fixture.page(), av::FencePage::encoded_size));
  CHECK_STATUS(fixture.at_dispatch(&GuestContext::serve), Status::ok);
  CHECK_EQ(progress, 3u);

  tail = 128u;
  std::memcpy(fixture.memory().pages().ring_view() + 8u, &tail, 8u);
  CHECK_STATUS(fixture.submit(4u, 96u, 128u), Status::ok);

  fixture.platform().set_irql(Irql::dispatch);
  fixture.context().driver_submits();
  fixture.platform().set_irql(Irql::passive);
  static_cast<void>(FencePage(4u, 128u).encode(fixture.page(), av::FencePage::encoded_size));
  CHECK_STATUS(fixture.at_dispatch(&GuestContext::serve), Status::ok);
  CHECK_EQ(progress, 3u);
  CHECK_EQ(fixture.context().completed(), 4u); }

TEST(guest_context, a_faulted_or_failed_context_reports_its_progress_finished)
{ Fixture      fixture;
  std::uint64_t progress = 0u;
  fixture.established();
  fixture.platform().set_irql(Irql::dispatch);
  fixture.context().user_mode_submits(&progress);
  fixture.platform().set_irql(Irql::passive);
  std::uint64_t tail = 64u;
  std::memcpy(fixture.memory().pages().ring_view() + 8u, &tail, 8u);
  static_cast<void>(FencePage(1u, 64u).encode(fixture.page(), av::FencePage::encoded_size));
  CHECK_STATUS(fixture.at_dispatch(&GuestContext::serve), Status::ok);
  CHECK_EQ(progress, 1u);
  static_cast<void>(FencePage(1u, 65u, 7u).encode(fixture.page(), av::FencePage::encoded_size));
  static_cast<void>(fixture.at_dispatch(&GuestContext::serve));
  CHECK(fixture.context().faulted());
  CHECK_EQ(fixture.context().fault_code(), 7u);
  CHECK_EQ(progress, UINT64_MAX);
  Fixture       lied_to;
  std::uint64_t lied_progress = 0u;
  lied_to.established();
  lied_to.platform().set_irql(Irql::dispatch);
  lied_to.context().user_mode_submits(&lied_progress);
  lied_to.platform().set_irql(Irql::passive);
  std::memset(lied_to.page(), 0xFF, av::FencePage::encoded_size);
  static_cast<void>(lied_to.at_dispatch(&GuestContext::serve));
  CHECK(lied_to.context().state() == GuestContext::State::failed);
  CHECK_EQ(lied_progress, UINT64_MAX); }

TEST(guest_context, forget_progress_lets_go_of_the_going_queues_progress_only)
{ Fixture      fixture;
  std::uint64_t progress = 0u;
  std::uint64_t other = 0u;
  fixture.established();
  fixture.platform().set_irql(Irql::dispatch);
  CHECK(!fixture.context().forget_progress(&progress));
  fixture.context().user_mode_submits(&progress);
  CHECK(!fixture.context().forget_progress(nullptr));
  CHECK(!fixture.context().forget_progress(&other));
  fixture.platform().set_irql(Irql::passive);
  static_cast<void>(FencePage(2u, 64u).encode(fixture.page(), av::FencePage::encoded_size));
  CHECK_STATUS(fixture.at_dispatch(&GuestContext::serve), Status::ok);
  CHECK_EQ(progress, 2u);

  fixture.platform().set_irql(Irql::dispatch);
  CHECK(fixture.context().forget_progress(&progress));
  CHECK(!fixture.context().forget_progress(&progress));
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.context().invariants_hold());
  static_cast<void>(FencePage(3u, 96u).encode(fixture.page(), av::FencePage::encoded_size));
  static_cast<void>(fixture.at_dispatch(&GuestContext::serve));
  CHECK_EQ(progress, 2u);
  CHECK_EQ(other, 0u); }

TEST(guest_context_kick, rings_an_established_context_only)
{ Fixture fixture;
  fixture.platform().set_irql(Irql::dispatch);
  fixture.context().kick();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.doorbells().rings(2u), 0u);
  fixture.established();
  fixture.platform().set_irql(Irql::dispatch);
  fixture.context().kick();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.doorbells().rings(2u), 1u);
  CHECK(fixture.context().tail() == 0u);
  CHECK_STATUS(fixture.at_dispatch(&GuestContext::tear_down), Status::ok);
  fixture.platform().set_irql(Irql::dispatch);
  fixture.context().kick();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.doorbells().rings(2u), 1u);
  CHECK_EQ(fixture.answer_all(), 4u); }

TEST(guest_context_next_begin, is_the_later_of_the_tail_it_kept_and_the_one_the_ring_header_shows)
{ Fixture fixture;
  CHECK_EQ(fixture.context().next_begin(), 0u);
  fixture.established();
  CHECK_EQ(fixture.context().next_begin(), 0u);
  CHECK_STATUS(fixture.submit(1u, 0u, 24u), Status::ok);
  CHECK_EQ(fixture.context().next_begin(), 24u);
  const std::uint64_t ahead = 100u;
  std::memcpy(fixture.memory().pages().ring_view() + 8u, &ahead, sizeof ahead);
  CHECK_EQ(fixture.context().next_begin(), 100u);
  const std::uint64_t behind = 8u;
  std::memcpy(fixture.memory().pages().ring_view() + 8u, &behind, sizeof behind);
  CHECK_EQ(fixture.context().next_begin(), 24u);
  CHECK(fixture.context().invariants_hold()); }

TEST(guest_context_fail, fails_an_established_context_only)
{ Fixture fixture;
  fixture.platform().set_irql(Irql::dispatch);
  fixture.context().fail();
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.context().state() == GuestContext::State::idle);
  fixture.established();
  fixture.platform().set_irql(Irql::dispatch);
  fixture.context().fail();
  fixture.context().fail();
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.context().state() == GuestContext::State::failed);
  CHECK(fixture.context().invariants_hold()); }

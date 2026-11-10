#include "miniport/av_context/context_memory.hpp"
#include "miniport/av_context/context_pages.hpp"
#include "miniport/av_context/ring_submitter.hpp"
#include "miniport/av_context/av_context_binding.hpp"
#include "core/support/status.hpp"
#include "miniport/virtio/virtio_gpu_device.hpp"
#include "harness/virtio/fake_doorbell_pages.hpp"
#include "harness/virtio/fake_virtqueue_device.hpp"
#include "harness/virtio/mapped_virtio_function.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/resources/host_blob_id.hpp"
#include "harness/platform/violation.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/context_capacity.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/escape/av_context_record.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/resources/guest_page_list.hpp"
#include "proto/submission/fence_page.hpp"
#include "proto/submission/submission_descriptor.hpp"
#include "proto/virtio_gpu/gpu_response.hpp"

#include <cstdint>
#include <cstring>
#include <optional>

using av::CapsetInfo;
using av::ContextMemory;
using av::ContextPages;
using av::FakeDoorbellPages;
using av::FakeVirtqueueDevice;
using av::FencePage;
using av::GpuIdentity;
using av::GpuResponse;
using av::GuestContext;
using av::GuestPageList;
using av::Irql;
using av::MappedVirtioFunction;
using av::ProtocolVersion;
using av::AvContextBinding;
using av::AvContextRecord;
using av::Status;
using av::SubApi;
using av::SubmissionDescriptor;
using av::Violation;

namespace {

constexpr std::uint16_t control_queue = 0u;
constexpr std::uint32_t ctx_create_type = 0x0200u;
constexpr std::uint32_t ctx_destroy_type = 0x0201u;
constexpr std::uint32_t create_blob_type = 0x010Cu;
constexpr std::uint32_t unref_type = 0x0102u;
constexpr std::uint32_t submit_3d_type = 0x0207u;
constexpr std::uint32_t slot = 2u;
constexpr std::uint32_t first_resource = 0x00100000u + 2u * 3u;
std::uint8_t lent[AvContextBinding::request_bytes];
constexpr std::uint8_t  uuid[GpuIdentity::uuid_bytes] = {};

CapsetInfo capset()
{
    return CapsetInfo(ProtocolVersion(1u, 0u), 0x7u, av::ContextCapacity::contexts, 12u, 20u, 0u,
                      GpuIdentity(0u, 0u, 0u, uuid, 0u, 0u, "gpu"));
}

AvContextRecord record()
{
    return AvContextRecord(SubApi::vulkan, 12u, 10u, 4096u);
}

class Host {
public:
    explicit Host(MappedVirtioFunction &mapped) : queue_(mapped.queue_device(control_queue))
    { mapped.platform().on_stall(&Host::stall, this); }

    void refuse_at(int index) { refuse_at_ = index; }
    void answer_nothing() { silent_ = true; }
    void answer_short() { short_ = true; }
    FakeVirtqueueDevice &queue() { return queue_; }
    std::uint32_t count_of(std::uint32_t type) const
    {
        std::uint32_t seen = 0u;
        for (std::uint32_t i = 0u; i < queue_.requests(); i++)
            if (queue_.request_type(i) == type) seen++;
        return seen;
    }
    std::uint32_t field(std::uint32_t type, std::uint32_t nth, std::size_t offset) const
    {
        for (std::uint32_t i = 0u; i < queue_.requests(); i++) {
            if (queue_.request_type(i) != type || nth-- != 0u) continue;
            std::uint32_t value = 0u;
            std::memcpy(&value, queue_.request(i) + offset, sizeof value);
            return value;
        }
        return 0u;
    }

private:
    static void stall(void *context)
    {
        auto *const host = static_cast<Host *>(context);
        if (host->queue_.pending_type() == 0u || host->silent_) return;
        if (host->short_) {
            const std::uint8_t truncated[4] = {0x00u, 0x11u, 0u, 0u};
            static_cast<void>(host->queue_.respond_all(truncated, sizeof truncated));
            return;
        }
        GpuResponse answer;
        if (static_cast<int>(host->answered_) == host->refuse_at_)
            answer.set(GpuResponse::err_invalid_parameter);
        else
            answer.set(GpuResponse::ok_nodata);
        host->answered_++;
        static_cast<void>(host->queue_.respond_all(answer.bytes(),
                                                   static_cast<std::uint32_t>(answer.length())));
    }

    FakeVirtqueueDevice queue_;
    std::uint32_t       answered_ = 0u;
    int                 refuse_at_ = -1;
    bool                silent_ = false;
    bool                short_ = false;
};

class Fixture {
public:
    Fixture() : memory_(mapped_.platform()) {}

    Status start()
    {
        const Status mapped = mapped_.map();
        if (mapped != Status::ok) return mapped;
        const Status allocated = memory_.allocate(12u, 4096u);
        if (allocated != Status::ok) return allocated;
        device_.emplace(mapped_.platform(), mapped_.mappings(), mapped_.layout());
        return device_->start(3u);
    }
    MappedVirtioFunction &mapped() { return mapped_; }
    av::VirtioGpuDevice  &device() { return *device_; }
    FakeDoorbellPages    &doorbells() { return doorbells_; }
    ContextPages          pages() const { return memory_.pages(); }

private:
    MappedVirtioFunction               mapped_;
    FakeDoorbellPages                  doorbells_;
    ContextMemory                      memory_;
    std::optional<av::VirtioGpuDevice> device_;
};

}

TEST(av_context_binding, is_idle_before_it_is_set_up)
{ MappedVirtioFunction mapped;
  FakeDoorbellPages doorbells;
  AvContextBinding binding(mapped.platform(), doorbells, record(), slot, 0u, lent, sizeof lent);
  CHECK(binding.state() == GuestContext::State::idle);
  CHECK(!binding.established());
  CHECK(binding.invariants_hold()); }

TEST(av_context_binding, drives_the_sequence_to_established)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  AvContextBinding binding(fixture.mapped().platform(), fixture.doorbells(), record(), slot, 0u, lent, sizeof lent);
  CHECK_STATUS(binding.set_up(fixture.device(), capset(), fixture.pages()), Status::ok);
  CHECK(binding.established());
  CHECK_EQ(host.count_of(ctx_create_type), 1u);
  CHECK_EQ(host.count_of(create_blob_type), 3u);
  CHECK(binding.invariants_hold()); }

TEST(av_context_binding, names_the_blobs_by_the_ids_it_was_given)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  AvContextBinding binding(fixture.mapped().platform(), fixture.doorbells(), record(), slot, 0u, lent, sizeof lent);
  CHECK_STATUS(binding.set_up(fixture.device(), capset(), fixture.pages()), Status::ok);
  constexpr std::size_t blob_resource = 24u;
  constexpr std::size_t blob_first_address = 56u;
  for (std::uint32_t blob = 0u; blob < AvContextBinding::transport_resources; blob++)
      CHECK_EQ(host.field(create_blob_type, blob, blob_resource), first_resource + blob);
  CHECK(host.field(create_blob_type, 0u, blob_first_address) ==
        static_cast<std::uint32_t>(fixture.pages().ring().frame(0u)));
  CHECK(host.field(create_blob_type, 1u, blob_first_address) ==
        static_cast<std::uint32_t>(fixture.pages().fence().frame(0u)));
  CHECK_EQ(host.field(submit_3d_type, 0u, 32u + 12u), first_resource);
  CHECK_EQ(host.field(submit_3d_type, 0u, 32u + 20u), first_resource + 1u);
  CHECK_EQ(host.field(submit_3d_type, 0u, 32u + 24u), 2u);
  CHECK_EQ(host.field(submit_3d_type, 0u, 32u + 28u), first_resource + 2u);
  CHECK_STATUS(binding.tear_down(fixture.device()), Status::ok);
  CHECK_EQ(host.field(unref_type, 0u, blob_resource), first_resource);
  CHECK_EQ(host.field(unref_type, 2u, blob_resource), first_resource + 2u); }

TEST(av_context_binding, a_scattered_ring_reaches_the_device_as_one_wide_blob)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  constexpr std::uint32_t pages = 8u;
  std::uint64_t ring_frames[pages] = {};
  for (std::uint32_t page = 0u; page < pages; page++)
      ring_frames[page] = (0x400u + 2u * page) * GuestPageList::page_bytes;
  const ContextPages memory = fixture.pages();
  const ContextPages scattered(GuestPageList(ring_frames, pages), memory.fence(), memory.reply(), memory.ring_view(),
                               memory.fence_view());
  AvContextBinding binding(fixture.mapped().platform(), fixture.doorbells(), record(), slot, 0u, lent, sizeof lent);
  CHECK_STATUS(binding.set_up(fixture.device(), capset(), scattered), Status::ok);
  CHECK(binding.established());
  constexpr std::size_t blob_entries = 36u;
  CHECK_EQ(host.field(create_blob_type, 0u, blob_entries), pages);
  CHECK_EQ(host.field(create_blob_type, 1u, blob_entries), 1u); }

TEST(av_context_binding, pages_not_complete_are_refused_locally)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  AvContextBinding binding(fixture.mapped().platform(), fixture.doorbells(), record(), slot, 0u, lent, sizeof lent);
  CHECK_STATUS(binding.set_up(fixture.device(), capset(), ContextPages()), Status::invalid_parameter);
  CHECK(binding.state() == GuestContext::State::idle);
  CHECK_EQ(host.count_of(ctx_create_type), 0u); }

TEST(av_context_binding, a_setup_the_device_refuses_leaves_the_binding_unestablished)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  host.refuse_at(4);
  AvContextBinding binding(fixture.mapped().platform(), fixture.doorbells(), record(), slot, 0u, lent, sizeof lent);
  CHECK(binding.set_up(fixture.device(), capset(), fixture.pages()) != Status::ok);
  CHECK(!binding.established());
  CHECK(binding.state() == GuestContext::State::failed); }

TEST(av_context_binding, a_setup_the_capset_forbids_is_refused_locally)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  const CapsetInfo strict(ProtocolVersion(1u, 0u), 0x7u, 64u, 16u, 20u, 0u,
                          GpuIdentity(0u, 0u, 0u, uuid, 0u, 0u, "gpu"));
  AvContextBinding binding(fixture.mapped().platform(), fixture.doorbells(), record(), slot, 0u, lent, sizeof lent);
  CHECK(binding.set_up(fixture.device(), strict, fixture.pages()) != Status::ok);
  CHECK(!binding.established());
  CHECK_EQ(host.count_of(ctx_create_type), 0u); }

TEST(av_context_binding, teardown_reverses_setup)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  AvContextBinding binding(fixture.mapped().platform(), fixture.doorbells(), record(), slot, 0u, lent, sizeof lent);
  CHECK_STATUS(binding.set_up(fixture.device(), capset(), fixture.pages()), Status::ok);
  CHECK_STATUS(binding.tear_down(fixture.device()), Status::ok);
  CHECK(binding.state() == GuestContext::State::idle);
  CHECK_EQ(host.count_of(ctx_destroy_type), 1u);
  CHECK(binding.invariants_hold()); }

TEST(av_context_binding, forwards_a_submission_and_a_serve_once_established)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  AvContextBinding binding(fixture.mapped().platform(), fixture.doorbells(), record(), slot, 0u, lent, sizeof lent);
  CHECK_STATUS(binding.set_up(fixture.device(), capset(), fixture.pages()), Status::ok);
  static_cast<void>(FencePage(0u, 0u).encode(const_cast<std::uint8_t *>(fixture.pages().fence_view()), av::FencePage::encoded_size));
  fixture.mapped().platform().set_irql(Irql::dispatch);
  CHECK_STATUS(binding.submit(SubmissionDescriptor(1u, 0u, 16u)), Status::ok);
  CHECK_STATUS(binding.serve(), Status::ok);
  fixture.mapped().platform().set_irql(Irql::passive);
  CHECK_EQ(binding.context_id(), 3u);
  CHECK_EQ(binding.completed(), 0u);
  CHECK(!binding.faulted());
  CHECK(binding.invariants_hold()); }

TEST(av_context_binding, service_before_setup_is_refused)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  AvContextBinding binding(fixture.mapped().platform(), fixture.doorbells(), record(), slot, 0u, lent, sizeof lent);
  CHECK_STATUS(binding.tear_down(fixture.device()), Status::invalid_state);
  fixture.mapped().platform().set_irql(Irql::dispatch);
  CHECK_STATUS(binding.submit(SubmissionDescriptor(1u, 0u, 16u)), Status::invalid_state);
  CHECK_STATUS(binding.serve(), Status::invalid_state);
  fixture.mapped().platform().set_irql(Irql::passive); }

TEST(av_context_binding, a_silent_device_does_not_establish)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  host.answer_nothing();
  AvContextBinding binding(fixture.mapped().platform(), fixture.doorbells(), record(), slot, 0u, lent, sizeof lent);
  CHECK(binding.set_up(fixture.device(), capset(), fixture.pages()) != Status::ok);
  CHECK(!binding.established()); }

TEST(av_context_binding_abandon, lets_go_without_a_request_and_sets_up_again)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  AvContextBinding binding(fixture.mapped().platform(), fixture.doorbells(), record(), slot, 0u, lent, sizeof lent);
  CHECK_STATUS(binding.set_up(fixture.device(), capset(), fixture.pages()), Status::ok);
  const std::uint32_t asked = host.queue().requests();
  binding.abandon();
  CHECK(binding.state() == GuestContext::State::idle);
  CHECK_EQ(host.queue().requests(), asked);
  CHECK(binding.invariants_hold());
  binding.abandon();
  CHECK_STATUS(binding.set_up(fixture.device(), capset(), fixture.pages()), Status::ok);
  CHECK(binding.established()); }

TEST(av_context_binding_resources, each_slot_and_generation_takes_transport_ids_of_its_own)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  const std::uint32_t last_slot = av::RingSubmitter::doorbell_pages - 1u;
  struct Case { std::uint32_t slot; std::uint32_t generation; std::uint32_t first; };
  const Case cases[] = {
      {slot, 0u, first_resource},
      {slot + 1u, 0u, first_resource + AvContextBinding::resources_per_context},
      {slot, 1u, first_resource + AvContextBinding::resources_per_context * av::RingSubmitter::doorbell_pages},
      {slot, AvContextBinding::generations, first_resource},
      {last_slot, AvContextBinding::generations - 1u,
       AvContextBinding::first_resource_id +
           AvContextBinding::resources_per_context *
               (last_slot + av::RingSubmitter::doorbell_pages * (AvContextBinding::generations - 1u))},
  };
  for (const Case &c : cases) {
      Host             host(fixture.mapped());
      AvContextBinding binding(fixture.mapped().platform(), fixture.doorbells(), record(), c.slot, c.generation, lent,
                               sizeof lent);
      CHECK_STATUS(binding.set_up(fixture.device(), capset(), fixture.pages()), Status::ok);
      CHECK_EQ(host.field(create_blob_type, 0u, 24u), c.first);
      CHECK_EQ(host.field(create_blob_type, 2u, 24u), c.first + 2u);
      CHECK(c.first + AvContextBinding::transport_resources <= av::HostBlobId::first);
      CHECK_STATUS(binding.tear_down(fixture.device()), Status::ok);
  } }

TEST(av_context_binding, hands_submissions_to_user_mode_and_takes_them_back)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host              host(fixture.mapped());
  AvContextBinding binding(fixture.mapped().platform(), fixture.doorbells(), record(), slot, 0u, lent, sizeof lent);
  CHECK_STATUS(binding.set_up(fixture.device(), capset(), fixture.pages()), Status::ok);
  CHECK(binding.established());
  std::uint64_t progress = 0u;
  fixture.mapped().platform().set_irql(av::Irql::dispatch);
  binding.user_mode_submits(&progress);
  static_cast<void>(av::FencePage(2u, 64u).encode(const_cast<std::uint8_t *>(fixture.pages().fence_view()), av::FencePage::encoded_size));
  CHECK_STATUS(binding.serve(), Status::ok);
  CHECK_EQ(progress, 2u);
  binding.driver_submits();
  static_cast<void>(av::FencePage(3u, 96u).encode(const_cast<std::uint8_t *>(fixture.pages().fence_view()), av::FencePage::encoded_size));
  CHECK_STATUS(binding.serve(), Status::invalid_state);
  CHECK_EQ(progress, 2u);
  fixture.mapped().platform().set_irql(av::Irql::passive);
  CHECK(binding.invariants_hold()); }

TEST(av_context_binding, forget_progress_reaches_the_context_for_its_own_location_only)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host              host(fixture.mapped());
  AvContextBinding binding(fixture.mapped().platform(), fixture.doorbells(), record(), slot, 0u, lent, sizeof lent);
  CHECK_STATUS(binding.set_up(fixture.device(), capset(), fixture.pages()), Status::ok);
  std::uint64_t progress = 0u;
  std::uint64_t other = 0u;
  fixture.mapped().platform().set_irql(av::Irql::dispatch);
  binding.user_mode_submits(&progress);
  CHECK(!binding.forget_progress(&other));
  CHECK(binding.forget_progress(&progress));
  CHECK(!binding.forget_progress(&progress));
  static_cast<void>(av::FencePage(2u, 64u).encode(const_cast<std::uint8_t *>(fixture.pages().fence_view()), av::FencePage::encoded_size));
  static_cast<void>(binding.serve());
  CHECK_EQ(progress, 0u);
  fixture.mapped().platform().set_irql(av::Irql::passive);
  CHECK(binding.invariants_hold()); }

TEST(av_context_binding, begins_its_next_batch_where_the_context_left_the_ring)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  AvContextBinding binding(fixture.mapped().platform(), fixture.doorbells(), record(), slot, 0u, lent, sizeof lent);
  CHECK_EQ(binding.next_begin(), 0u);
  CHECK_STATUS(binding.set_up(fixture.device(), capset(), fixture.pages()), Status::ok);
  static_cast<void>(FencePage(0u, 0u).encode(const_cast<std::uint8_t *>(fixture.pages().fence_view()), av::FencePage::encoded_size));
  CHECK_EQ(binding.next_begin(), 0u);
  fixture.mapped().platform().set_irql(Irql::dispatch);
  CHECK_STATUS(binding.submit(SubmissionDescriptor(1u, 0u, 16u)), Status::ok);
  fixture.mapped().platform().set_irql(Irql::passive);
  CHECK_EQ(binding.next_begin(), 16u);
  CHECK(binding.invariants_hold()); }

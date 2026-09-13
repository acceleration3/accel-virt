/* One dxgkrnl context bound to an rvk context. The binding builds the setup
 * from the record it was created with and drives the context's request
 * sequence - CTX_CREATE, three blobs and the setup - against the device, all
 * the way to established. A setup the device refuses leaves the binding failed,
 * not established; teardown reverses what setup created and frees the memory. */
#include "core/rvk/rvk_context_binding.hpp"
#include "core/support/status.hpp"
#include "core/virtio/virtio_gpu_device.hpp"
#include "harness/device/fake_doorbell_pages.hpp"
#include "harness/device/fake_virtqueue_device.hpp"
#include "harness/device/mapped_virtio_function.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/violation.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/context/rvk_context_record.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/submission/fence_page.hpp"
#include "proto/submission/submission_descriptor.hpp"
#include "proto/virtio_gpu/gpu_response.hpp"

#include <cstdint>
#include <optional>

using av::CapsetInfo;
using av::FakeDoorbellPages;
using av::FakeVirtqueueDevice;
using av::FencePage;
using av::GpuIdentity;
using av::GpuResponse;
using av::GuestContext;
using av::Irql;
using av::MappedVirtioFunction;
using av::ProtocolVersion;
using av::RvkContextBinding;
using av::RvkContextRecord;
using av::Status;
using av::SubApi;
using av::SubmissionDescriptor;
using av::Violation;

namespace {

constexpr std::uint16_t control_queue = 0u;
constexpr std::uint32_t ctx_create_type = 0x0200u;
constexpr std::uint32_t ctx_destroy_type = 0x0201u;
constexpr std::uint8_t  uuid[GpuIdentity::uuid_bytes] = {};

CapsetInfo capset()
{
    return CapsetInfo(ProtocolVersion(1u, 0u), 0x7u, 64u, 12u, 20u, 0u,
                      GpuIdentity(0u, 0u, 0u, uuid, 0u, 0u, "gpu"));
}

/* Ring 10, fence 11, reply 12 - distinct, order the capset allows, Vulkan. */
RvkContextRecord record()
{
    return RvkContextRecord(SubApi::vulkan, 12u, 10u, 11u, 12u);
}

/* A device answering control requests from the stall between the driver's
 * polls, OK_NODATA by default, refusing the request at a chosen index. */
class Host {
public:
    explicit Host(MappedVirtioFunction &mapped) : queue_(mapped.queue_device(control_queue))
    { mapped.platform().on_stall(&Host::stall, this); }

    void refuse_at(int index) { refuse_at_ = index; }
    void answer_nothing() { silent_ = true; }
    FakeVirtqueueDevice &queue() { return queue_; }
    std::uint32_t count_of(std::uint32_t type) const
    {
        std::uint32_t seen = 0u;
        for (std::uint32_t i = 0u; i < queue_.requests(); i++)
            if (queue_.request_type(i) == type) seen++;
        return seen;
    }

private:
    static void stall(void *context)
    {
        auto *const host = static_cast<Host *>(context);
        if (host->queue_.pending_type() == 0u || host->silent_) return;
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
};

class Fixture {
public:
    Status start()
    {
        const Status mapped = mapped_.map();
        if (mapped != Status::ok) return mapped;
        device_.emplace(mapped_.platform(), mapped_.mappings(), mapped_.layout());
        return device_->start(3u);
    }
    MappedVirtioFunction &mapped() { return mapped_; }
    av::VirtioGpuDevice  &device() { return *device_; }
    FakeDoorbellPages    &doorbells() { return doorbells_; }

private:
    MappedVirtioFunction               mapped_;
    FakeDoorbellPages                  doorbells_;
    std::optional<av::VirtioGpuDevice> device_;
};

}  // namespace

TEST(rvk_context_binding, is_idle_before_it_is_set_up)
{ MappedVirtioFunction mapped;
  FakeDoorbellPages doorbells;
  RvkContextBinding binding(mapped.platform(), doorbells, record(), 3u, 2u);
  CHECK(binding.state() == GuestContext::State::idle);
  CHECK(!binding.established());
  CHECK(binding.invariants_hold()); }

TEST(rvk_context_binding, drives_the_sequence_to_established)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  RvkContextBinding binding(fixture.mapped().platform(), fixture.doorbells(), record(), 3u, 2u);
  CHECK_STATUS(binding.set_up(fixture.device(), capset()), Status::ok);
  CHECK(binding.established());
  /* CTX_CREATE, three blobs and the setup: one context command and three blob
   * creates on the wire. */
  CHECK_EQ(host.count_of(ctx_create_type), 1u);
  CHECK_EQ(host.count_of(0x010Cu), 3u);
  CHECK(binding.invariants_hold()); }

TEST(rvk_context_binding, a_setup_the_device_refuses_leaves_the_binding_unestablished)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  host.refuse_at(4);   /* the setup step, after CTX_CREATE and three blobs */
  RvkContextBinding binding(fixture.mapped().platform(), fixture.doorbells(), record(), 3u, 2u);
  CHECK(binding.set_up(fixture.device(), capset()) != Status::ok);
  CHECK(!binding.established());
  CHECK(binding.state() == GuestContext::State::failed); }

/* A record whose fields the capset does not allow is refused before any request
 * reaches the device. */
TEST(rvk_context_binding, a_setup_the_capset_forbids_is_refused_locally)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  /* Order 12 is fine, but a capset whose range starts at 16 forbids it. */
  const CapsetInfo strict(ProtocolVersion(1u, 0u), 0x7u, 64u, 16u, 20u, 0u,
                          GpuIdentity(0u, 0u, 0u, uuid, 0u, 0u, "gpu"));
  RvkContextBinding binding(fixture.mapped().platform(), fixture.doorbells(), record(), 3u, 2u);
  CHECK(binding.set_up(fixture.device(), strict) != Status::ok);
  CHECK(!binding.established());
  CHECK_EQ(host.count_of(ctx_create_type), 0u); }

TEST(rvk_context_binding, teardown_reverses_setup)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  RvkContextBinding binding(fixture.mapped().platform(), fixture.doorbells(), record(), 3u, 2u);
  CHECK_STATUS(binding.set_up(fixture.device(), capset()), Status::ok);
  CHECK_STATUS(binding.tear_down(fixture.device()), Status::ok);
  CHECK(binding.state() == GuestContext::State::idle);
  /* CTX_DESTROY and an unref for each blob. */
  CHECK_EQ(host.count_of(ctx_destroy_type), 1u);
  CHECK(binding.invariants_hold()); }

TEST(rvk_context_binding, forwards_a_submission_and_a_serve_once_established)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  RvkContextBinding binding(fixture.mapped().platform(), fixture.doorbells(), record(), 3u, 2u);
  CHECK_STATUS(binding.set_up(fixture.device(), capset()), Status::ok);
  /* The device writes the fence page; a no-progress page is what serve reads
   * before any batch completes. */
  static_cast<void>(FencePage(0u, 0u).encode(const_cast<std::uint8_t *>(binding.fence_page()), 32u));
  fixture.mapped().platform().set_irql(Irql::dispatch);
  CHECK_STATUS(binding.submit(SubmissionDescriptor(1u, 0u, 16u)), Status::ok);
  CHECK_STATUS(binding.serve(), Status::ok);
  fixture.mapped().platform().set_irql(Irql::passive);
  CHECK_EQ(binding.context_id(), 3u);
  CHECK_EQ(binding.completed(), 0u);
  CHECK(!binding.faulted());
  CHECK(binding.invariants_hold()); }

/* Before setup there is nothing to tear down, submit to, or serve. */
TEST(rvk_context_binding, service_before_setup_is_refused)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  RvkContextBinding binding(fixture.mapped().platform(), fixture.doorbells(), record(), 3u, 2u);
  CHECK_STATUS(binding.tear_down(fixture.device()), Status::invalid_state);
  fixture.mapped().platform().set_irql(Irql::dispatch);
  CHECK_STATUS(binding.submit(SubmissionDescriptor(1u, 0u, 16u)), Status::invalid_state);
  CHECK_STATUS(binding.serve(), Status::invalid_state);
  fixture.mapped().platform().set_irql(Irql::passive); }

/* A device that stops answering mid-sequence leaves the binding unestablished
 * rather than hanging. */
TEST(rvk_context_binding, a_silent_device_does_not_establish)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  host.answer_nothing();
  RvkContextBinding binding(fixture.mapped().platform(), fixture.doorbells(), record(), 3u, 2u);
  CHECK(binding.set_up(fixture.device(), capset()) != Status::ok);
  CHECK(!binding.established()); }

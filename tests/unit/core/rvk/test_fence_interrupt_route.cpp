/* Which context a fence-queue interrupt serves. The route wakes the binding its
 * vector points to and no other; a vector whose context has torn down, or that
 * none claims, is a no-op. Two contexts are established over a fake device so a
 * served binding shows the fence progress its page holds. */
#include "core/rvk/context_memory.hpp"
#include "core/rvk/context_pages.hpp"
#include "core/rvk/fence_interrupt_route.hpp"
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
using av::ContextMemory;
using av::ContextPages;
using av::FakeDoorbellPages;
using av::FakeVirtqueueDevice;
using av::FenceInterruptRoute;
using av::FencePage;
using av::GpuIdentity;
using av::GpuResponse;
using av::Irql;
using av::MappedVirtioFunction;
using av::ProtocolVersion;
using av::RvkContextBinding;
using av::RvkContextRecord;
using av::Status;
using av::SubApi;
using av::SubmissionDescriptor;

namespace {

constexpr std::uint16_t control_queue = 0u;
constexpr std::uint8_t  uuid[GpuIdentity::uuid_bytes] = {};

CapsetInfo capset()
{
    return CapsetInfo(ProtocolVersion(1u, 0u), 0x7u, 64u, 12u, 20u, 0u,
                      GpuIdentity(0u, 0u, 0u, uuid, 0u, 0u, "gpu"));
}

/* A device that answers every control request OK_NODATA from the stall. */
class Host {
public:
    explicit Host(MappedVirtioFunction &mapped) : queue_(mapped.queue_device(control_queue))
    { mapped.platform().on_stall(&Host::stall, this); }

private:
    static void stall(void *context)
    {
        auto *const host = static_cast<Host *>(context);
        if (host->queue_.pending_type() == 0u) return;
        GpuResponse answer;
        answer.set(GpuResponse::ok_nodata);
        static_cast<void>(host->queue_.respond_all(answer.bytes(),
                                                   static_cast<std::uint32_t>(answer.length())));
    }
    FakeVirtqueueDevice queue_;
};

/* Each context's memory stands in for its process's allocations. */
class Fixture {
public:
    Fixture() : first_(mapped_.platform()), second_(mapped_.platform()) {}

    Status start()
    {
        const Status mapped = mapped_.map();
        if (mapped != Status::ok) return mapped;
        if (first_.allocate(12u) != Status::ok || second_.allocate(12u) != Status::ok) return Status::no_memory;
        device_.emplace(mapped_.platform(), mapped_.mappings(), mapped_.layout());
        return device_->start(3u);
    }
    MappedVirtioFunction &mapped() { return mapped_; }
    av::VirtioGpuDevice  &device() { return *device_; }
    FakeDoorbellPages    &doorbells() { return doorbells_; }
    ContextPages          pages(std::uint32_t index) const { return index == 0u ? first_.pages() : second_.pages(); }

private:
    MappedVirtioFunction               mapped_;
    FakeDoorbellPages                  doorbells_;
    ContextMemory                      first_;
    ContextMemory                      second_;
    std::optional<av::VirtioGpuDevice> device_;
};

constexpr std::uint32_t first_resource = 0x00100000u;

/* Submits one batch on the binding (so a completed fence is legitimate) and
 * seeds its fence page as the device would when that batch finishes. */
void prime(av::MockPlatform &platform, RvkContextBinding &binding, std::uint64_t fence, std::uint64_t bytes)
{
    platform.set_irql(Irql::dispatch);
    static_cast<void>(binding.submit(SubmissionDescriptor(fence, 0u, bytes)));
    platform.set_irql(Irql::passive);
    static_cast<void>(FencePage(fence, bytes)
                          .encode(const_cast<std::uint8_t *>(binding.fence_page()), FencePage::encoded_size));
}

}  // namespace

TEST(fence_interrupt_route, routes_a_vector_to_its_context_and_no_other)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  RvkContextBinding first(fixture.mapped().platform(), fixture.doorbells(),
                          RvkContextRecord(SubApi::vulkan, 12u, 10u, 11u, 12u), 3u, 2u, first_resource);
  RvkContextBinding second(fixture.mapped().platform(), fixture.doorbells(),
                           RvkContextRecord(SubApi::vulkan, 12u, 20u, 21u, 22u), 4u, 3u,
                           first_resource + RvkContextBinding::resources_per_context);
  CHECK_STATUS(first.set_up(fixture.device(), capset(), fixture.pages(0u)), Status::ok);
  CHECK_STATUS(second.set_up(fixture.device(), capset(), fixture.pages(1u)), Status::ok);
  prime(fixture.mapped().platform(), first, 5u, 64u);
  prime(fixture.mapped().platform(), second, 7u, 64u);

  FenceInterruptRoute route(fixture.mapped().platform());
  CHECK_STATUS(route.attach(2u, first), Status::ok);
  CHECK_STATUS(route.attach(3u, second), Status::ok);
  CHECK(route.routed(2u) && route.routed(3u) && !route.routed(4u));

  fixture.mapped().platform().set_irql(Irql::dispatch);
  route.serve(2u);
  fixture.mapped().platform().set_irql(Irql::passive);
  CHECK_EQ(first.completed(), 5u);
  CHECK_EQ(second.completed(), 0u);   /* only the first was woken */

  fixture.mapped().platform().set_irql(Irql::dispatch);
  route.serve(3u);
  fixture.mapped().platform().set_irql(Irql::passive);
  CHECK_EQ(second.completed(), 7u);
  CHECK(route.invariants_hold()); }

TEST(fence_interrupt_route, a_detached_or_unclaimed_vector_is_a_no_op)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  RvkContextBinding binding(fixture.mapped().platform(), fixture.doorbells(),
                            RvkContextRecord(SubApi::vulkan, 12u, 10u, 11u, 12u), 3u, 2u, first_resource);
  CHECK_STATUS(binding.set_up(fixture.device(), capset(), fixture.pages(0u)), Status::ok);

  FenceInterruptRoute route(fixture.mapped().platform());
  CHECK_STATUS(route.attach(2u, binding), Status::ok);
  /* Attached, a serve wakes it. */
  prime(fixture.mapped().platform(), binding, 1u, 64u);
  fixture.mapped().platform().set_irql(Irql::dispatch);
  route.serve(2u);
  fixture.mapped().platform().set_irql(Irql::passive);
  CHECK_EQ(binding.completed(), 1u);
  /* Detached, a later serve does nothing, even with more to report. */
  route.detach(2u);
  CHECK(!route.routed(2u));
  prime(fixture.mapped().platform(), binding, 2u, 128u);
  fixture.mapped().platform().set_irql(Irql::dispatch);
  route.serve(2u);    /* detached: nothing served */
  route.serve(9u);    /* never claimed: nothing served */
  fixture.mapped().platform().set_irql(Irql::passive);
  CHECK_EQ(binding.completed(), 1u); }

TEST(fence_interrupt_route, refuses_a_vector_out_of_range_or_already_claimed)
{ MappedVirtioFunction mapped;
  FakeDoorbellPages doorbells;
  RvkContextBinding binding(mapped.platform(), doorbells,
                            RvkContextRecord(SubApi::vulkan, 12u, 10u, 11u, 12u), 3u, 2u, first_resource);
  FenceInterruptRoute route(mapped.platform());
  CHECK_STATUS(route.attach(FenceInterruptRoute::fence_queues, binding), Status::invalid_parameter);
  CHECK_STATUS(route.attach(1u, binding), Status::ok);
  CHECK_STATUS(route.attach(1u, binding), Status::invalid_state);
  /* Detaching, serving or querying a vector out of range is harmless. */
  route.detach(FenceInterruptRoute::fence_queues);
  CHECK(!route.routed(FenceInterruptRoute::fence_queues));
  mapped.platform().set_irql(Irql::dispatch);
  route.serve(FenceInterruptRoute::fence_queues);
  mapped.platform().set_irql(Irql::passive);
  CHECK(route.invariants_hold()); }

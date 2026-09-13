/* One host-visible allocation's memory on the device: created, mapped where the
 * driver placed it, and released in reverse. The device answers over a real
 * control queue, as it does at StartDevice (feature 041, US1). */
#include "core/memory/host_visible_blob.hpp"
#include "core/support/status.hpp"
#include "core/virtio/virtio_gpu_device.hpp"
#include "harness/device/fake_virtqueue_device.hpp"
#include "harness/device/mapped_virtio_function.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/violation.hpp"
#include "proto/virtio_gpu/gpu_response.hpp"

#include <cstdint>
#include <optional>

using av::FakeVirtqueueDevice;
using av::GpuResponse;
using av::HostVisibleBlob;
using av::Irql;
using av::MappedVirtioFunction;
using av::Status;
using av::VirtioGpuDevice;
using av::Violation;

namespace {

constexpr std::uint16_t control_queue = 0u;
constexpr std::uint64_t page = 0x1000u;
constexpr std::uint32_t resource = 0x1000u;

/* A device that answers each control request from the stall between the
 * driver's polls, with what the test told it to say. */
class Host {
public:
    explicit Host(MappedVirtioFunction &mapped) : queue_(mapped.queue_device(control_queue))
    { mapped.platform().on_stall(&Host::stall, this); }

    void answer_map_with(std::uint32_t map_info) { map_info_ = map_info; }
    void refuse_next() { refuse_ = true; }
    void answer_nothing() { silent_ = true; }
    std::uint32_t answered() const { return answered_; }
    FakeVirtqueueDevice &queue() { return queue_; }

private:
    static void stall(void *context)
    {
        auto *const host = static_cast<Host *>(context);
        if (host->silent_) return;
        const std::uint32_t asked = host->queue_.pending_type();
        if (asked == 0u) return;
        GpuResponse answer;
        if (host->refuse_) {
            host->refuse_ = false;
            answer.set(GpuResponse::err_invalid_parameter);
        } else if (asked == 0x0208u) {
            answer.set_map_info(host->map_info_);
        } else {
            answer.set(GpuResponse::ok_nodata);
        }
        host->answered_++;
        static_cast<void>(host->queue_.respond_all(answer.bytes(),
                                                   static_cast<std::uint32_t>(answer.length())));
    }

    FakeVirtqueueDevice queue_;
    std::uint32_t       map_info_ = GpuResponse::map_cache_cached;
    std::uint32_t       answered_ = 0u;
    bool                refuse_ = false;
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
    VirtioGpuDevice      &device() { return *device_; }

private:
    MappedVirtioFunction           mapped_;
    std::optional<VirtioGpuDevice> device_;
};

}  // namespace

TEST(host_visible_blob, holds_nothing_until_it_is_created)
{ MappedVirtioFunction mapped;
  HostVisibleBlob blob(mapped.platform());
  CHECK(!blob.created());
  CHECK(!blob.mapped());
  CHECK_EQ(blob.resource(), 0u);
  CHECK(blob.invariants_hold()); }

TEST(host_visible_blob, is_created_mapped_and_released_in_reverse)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  HostVisibleBlob blob(fixture.mapped().platform());
  CHECK_STATUS(blob.create(fixture.device(), resource, 2u * page), Status::ok);
  CHECK(blob.created());
  CHECK_EQ(blob.resource(), resource);
  CHECK(!blob.mapped());
  CHECK_STATUS(blob.map(fixture.device(), 4u * page), Status::ok);
  CHECK(blob.mapped());
  CHECK_EQ(blob.offset(), 4u * page);
  CHECK_STATUS(blob.unmap(fixture.device()), Status::ok);
  CHECK(!blob.mapped());
  CHECK(blob.created());                 /* the device keeps what it holds */
  CHECK_STATUS(blob.destroy(fixture.device()), Status::ok);
  CHECK(!blob.created());
  CHECK_EQ(host.answered(), 4u);
  CHECK(blob.invariants_hold()); }

/* The answer to a map is read, not assumed: a blob the driver thinks is mapped
 * and is not would hand a process whatever else is at that offset. */
TEST(host_visible_blob_map, a_refused_map_leaves_it_unmapped)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  HostVisibleBlob blob(fixture.mapped().platform());
  CHECK_STATUS(blob.create(fixture.device(), resource, page), Status::ok);
  host.refuse_next();
  CHECK_STATUS(blob.map(fixture.device(), 4u * page), Status::not_supported);
  CHECK(!blob.mapped());
  CHECK_EQ(blob.offset(), 0u);
  CHECK(blob.invariants_hold()); }

/* An OK_MAP_INFO that is not the caching the device promised is not a mapping
 * this driver knows how to hand out. */
TEST(host_visible_blob_map, an_answer_of_another_kind_is_refused)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  host.answer_map_with(0u);
  HostVisibleBlob blob(fixture.mapped().platform());
  CHECK_STATUS(blob.create(fixture.device(), resource, page), Status::ok);
  CHECK_STATUS(blob.map(fixture.device(), page), Status::malformed);
  CHECK(!blob.mapped()); }

TEST(host_visible_blob_map, a_device_that_answers_nothing_leaves_it_unmapped)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  HostVisibleBlob blob(fixture.mapped().platform());
  CHECK_STATUS(blob.create(fixture.device(), resource, page), Status::ok);
  host.answer_nothing();
  CHECK_STATUS(blob.map(fixture.device(), page), Status::not_found);
  CHECK(!blob.mapped()); }

/* Order is the whole contract: nothing is mapped before it exists, nothing is
 * mapped twice, and nothing is unmapped or destroyed that was not. */
TEST(host_visible_blob, refuses_every_step_out_of_order)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  HostVisibleBlob blob(fixture.mapped().platform());
  CHECK_STATUS(blob.map(fixture.device(), page), Status::invalid_state);
  CHECK_STATUS(blob.unmap(fixture.device()), Status::invalid_state);
  CHECK_STATUS(blob.destroy(fixture.device()), Status::invalid_state);
  CHECK_STATUS(blob.create(fixture.device(), resource, page), Status::ok);
  CHECK_STATUS(blob.create(fixture.device(), resource, page), Status::invalid_state);
  CHECK_STATUS(blob.unmap(fixture.device()), Status::invalid_state);
  CHECK_STATUS(blob.map(fixture.device(), page), Status::ok);
  CHECK_STATUS(blob.map(fixture.device(), 2u * page), Status::invalid_state);
  CHECK_EQ(blob.offset(), page);
  /* Destroying a mapped blob unmaps it first: the device refuses an unref of
   * something still placed in its region. */
  CHECK_STATUS(blob.destroy(fixture.device()), Status::ok);
  CHECK(!blob.mapped());
  CHECK(!blob.created());
  CHECK(blob.invariants_hold()); }

TEST(host_visible_blob_create, refuses_what_is_not_a_page_multiple)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  HostVisibleBlob blob(fixture.mapped().platform());
  CHECK_STATUS(blob.create(fixture.device(), resource, page + 1u), Status::invalid_parameter);
  CHECK_STATUS(blob.create(fixture.device(), resource, 0u), Status::invalid_parameter);
  CHECK_STATUS(blob.create(fixture.device(), 0u, page), Status::invalid_parameter);
  CHECK(!blob.created());
  CHECK_EQ(host.answered(), 0u); }

TEST(host_visible_blob, is_passive_only)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  HostVisibleBlob blob(fixture.mapped().platform());
  fixture.mapped().platform().set_irql(Irql::dispatch);
  static_cast<void>(blob.create(fixture.device(), resource, page));
  fixture.mapped().platform().set_irql(Irql::passive);
  CHECK(fixture.mapped().platform().claim(Violation::irql_ceiling));
  while (fixture.mapped().platform().claim(Violation::irql_ceiling)) {} }

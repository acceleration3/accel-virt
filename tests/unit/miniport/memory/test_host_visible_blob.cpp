/* One window chunk's memory on the device: a host blob of host GPU memory made
 * for one context, mapped where the driver placed it with the caching the
 * device answers, and released in reverse (docs/host-memory.md). The device
 * answers over a real control queue, as it does at StartDevice. */
#include "miniport/memory/host_visible_blob.hpp"
#include "core/support/status.hpp"
#include "miniport/virtio/virtio_gpu_device.hpp"
#include "harness/virtio/fake_virtqueue_device.hpp"
#include "harness/virtio/mapped_virtio_function.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/violation.hpp"
#include "proto/resources/host_memory_request.hpp"
#include "proto/virtio_gpu/create_blob_request.hpp"
#include "proto/virtio_gpu/gpu_response.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstdint>
#include <optional>

using av::FakeVirtqueueDevice;
using av::GpuResponse;
using av::CreateBlobRequest;
using av::HostMemoryRequest;
using av::HostVisibleBlob;
using av::PageCaching;
using av::WireReader;
using av::Irql;
using av::MappedVirtioFunction;
using av::Status;
using av::VirtioGpuDevice;
using av::Violation;

namespace av {

struct HostVisibleBlobProbe {
    /* A blob that says it is mapped with no resource behind it: what the class
     * never builds, and what its invariant is there to catch. */
    static void map_without_a_resource(HostVisibleBlob &blob)
    {
        blob.resource_ = 0u;
        blob.mapped_ = true;
    }
};

}  // namespace av

namespace {

constexpr std::uint16_t control_queue = 0u;
constexpr std::uint64_t page = 0x1000u;
constexpr std::uint32_t resource = 0x01000000u;
constexpr std::uint32_t context_id = 3u;
const HostMemoryRequest upload(HostMemoryRequest::Kind::upload, 0u);

/* A device that answers each control request from the stall between the
 * driver's polls, with what the test told it to say. */
class Host {
public:
    explicit Host(MappedVirtioFunction &mapped) : queue_(mapped.queue_device(control_queue))
    { mapped.platform().on_stall(&Host::stall, this); }

    void answer_map_with(std::uint32_t map_info) { map_info_ = map_info; }
    void refuse_next() { refuse_ = true; }
    void answer_nothing() { silent_ = true; }
    /* Bytes no device sends: too short to be a response header. */
    void answer_garbage() { garbage_ = true; }
    std::uint32_t answered() const { return answered_; }
    FakeVirtqueueDevice &queue() { return queue_; }

private:
    static void stall(void *context)
    {
        auto *const host = static_cast<Host *>(context);
        if (host->silent_) return;
        const std::uint32_t asked = host->queue_.pending_type();
        if (asked == 0u) return;
        if (host->garbage_) {
            const std::uint8_t rubbish[8] = {0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE};
            host->answered_++;
            static_cast<void>(host->queue_.respond_all(rubbish, sizeof rubbish));
            return;
        }
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
    bool                garbage_ = false;
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
  CHECK_STATUS(blob.create(fixture.device(), context_id, resource, upload, 2u * page), Status::ok);
  CHECK(blob.created());
  CHECK_EQ(blob.resource(), resource);
  CHECK(!blob.mapped());
  CHECK_STATUS(blob.map(fixture.device(), 4u * page), Status::ok);
  CHECK(blob.mapped());
  CHECK(blob.invariants_hold());   /* mapped, with the resource behind it */
  CHECK_EQ(blob.offset(), 4u * page);
  CHECK_STATUS(blob.unmap(fixture.device()), Status::ok);
  CHECK(!blob.mapped());
  CHECK(blob.created());                 /* the device keeps what it holds */
  CHECK_STATUS(blob.destroy(fixture.device()), Status::ok);
  CHECK(!blob.created());
  CHECK_EQ(host.answered(), 4u);
  CHECK(blob.invariants_hold()); }

/* A window chunk remembers only its resource and where its blob was placed:
 * the blob rebuilt from those is created and mapped there, and comes down with
 * an unmap and then an unref. */
TEST(host_visible_blob, a_blob_rebuilt_from_what_a_chunk_remembers_comes_down_in_reverse)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  HostVisibleBlob placed(fixture.mapped().platform(), resource, 6u * page);
  CHECK(placed.created());
  CHECK(placed.mapped());
  CHECK_EQ(placed.resource(), resource);
  CHECK_EQ(placed.offset(), 6u * page);
  CHECK(placed.invariants_hold());
  CHECK_STATUS(placed.create(fixture.device(), context_id, resource, upload, page), Status::invalid_state);
  CHECK_STATUS(placed.destroy(fixture.device()), Status::ok);
  CHECK(!placed.created());
  CHECK(!placed.mapped());
  CHECK_EQ(host.answered(), 2u);
  CHECK_EQ(host.queue().request_type(0u), 0x0209u);   /* RESOURCE_UNMAP_BLOB */
  CHECK_EQ(host.queue().request_type(1u), 0x0102u);   /* RESOURCE_UNREF */ }

/* The answer to a map is read, not assumed: a blob the driver thinks is mapped
 * and is not would hand a process whatever else is at that offset. */
TEST(host_visible_blob_map, a_refused_map_leaves_it_unmapped)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  HostVisibleBlob blob(fixture.mapped().platform());
  CHECK_STATUS(blob.create(fixture.device(), context_id, resource, upload, page), Status::ok);
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
  CHECK_STATUS(blob.create(fixture.device(), context_id, resource, upload, page), Status::ok);
  CHECK_STATUS(blob.map(fixture.device(), page), Status::malformed);
  CHECK(!blob.mapped()); }

TEST(host_visible_blob_map, a_device_that_answers_nothing_leaves_it_unmapped)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  HostVisibleBlob blob(fixture.mapped().platform());
  CHECK_STATUS(blob.create(fixture.device(), context_id, resource, upload, page), Status::ok);
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
  CHECK_STATUS(blob.create(fixture.device(), context_id, resource, upload, page), Status::ok);
  CHECK_STATUS(blob.create(fixture.device(), context_id, resource, upload, page), Status::invalid_state);
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
  CHECK_STATUS(blob.create(fixture.device(), context_id, resource, upload, page + 1u), Status::invalid_parameter);
  CHECK_STATUS(blob.create(fixture.device(), context_id, resource, upload, 0u), Status::invalid_parameter);
  CHECK_STATUS(blob.create(fixture.device(), context_id, 0u, upload, page), Status::invalid_parameter);
  CHECK(!blob.created());
  CHECK_EQ(host.answered(), 0u); }

/* A blob is made for a context, of memory a host GPU has: neither may be
 * missing, and nothing is asked of the device when one is. */
TEST(host_visible_blob_create, refuses_no_context_and_a_request_no_driver_makes)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  HostVisibleBlob blob(fixture.mapped().platform());
  CHECK_STATUS(blob.create(fixture.device(), 0u, resource, upload, page), Status::invalid_parameter);
  CHECK_STATUS(blob.create(fixture.device(), context_id, resource, HostMemoryRequest(), page),
               Status::invalid_parameter);
  CHECK_STATUS(blob.create(fixture.device(), context_id, resource, HostMemoryRequest(HostMemoryRequest::Kind::vulkan, 32u),
                           page),
               Status::invalid_parameter);
  CHECK(!blob.created());
  CHECK_EQ(host.answered(), 0u); }

/* What the device is asked to make is the context's, of the memory the
 * process named, carried as the blob id. */
TEST(host_visible_blob_create, asks_for_the_contexts_memory_as_the_blob_id)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  HostVisibleBlob         blob(fixture.mapped().platform());
  const HostMemoryRequest vulkan(HostMemoryRequest::Kind::vulkan, 5u);
  CHECK_STATUS(blob.create(fixture.device(), context_id, resource, vulkan, 3u * page), Status::ok);
  CHECK_EQ(host.queue().requests(), 1u);
  WireReader        reader(host.queue().request(0u), 56u);
  CreateBlobRequest asked;
  CHECK_STATUS(asked.decode(reader), Status::ok);
  CHECK_EQ(asked.context_id(), context_id);
  CHECK_EQ(asked.resource_id(), resource);
  CHECK_EQ(asked.memory(), CreateBlobRequest::memory_host3d);
  CHECK(asked.blob_id() == vulkan.encoded());
  CHECK(asked.size() == 3u * page);
  CHECK_STATUS(blob.destroy(fixture.device()), Status::ok); }

/* The device says how the host maps the memory, and a process must map it
 * the same way: cached memory is write-back, anything else the device may
 * say is write-combined. Until it is mapped there is nothing to say. */
TEST(host_visible_blob_map, takes_the_caching_the_device_answers)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  HostVisibleBlob cached(fixture.mapped().platform());
  CHECK(cached.caching() == PageCaching::write_back);
  CHECK_STATUS(cached.create(fixture.device(), context_id, resource, upload, page), Status::ok);
  CHECK_STATUS(cached.map(fixture.device(), 0u), Status::ok);
  CHECK(cached.caching() == PageCaching::write_back);
  host.answer_map_with(GpuResponse::map_cache_wc);
  HostVisibleBlob combined(fixture.mapped().platform());
  CHECK_STATUS(combined.create(fixture.device(), context_id, resource + 1u, upload, page), Status::ok);
  CHECK_STATUS(combined.map(fixture.device(), page), Status::ok);
  CHECK(combined.caching() == PageCaching::write_combined);
  CHECK_STATUS(cached.destroy(fixture.device()), Status::ok);
  CHECK_STATUS(combined.destroy(fixture.device()), Status::ok); }

TEST(host_visible_blob, is_passive_only)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  HostVisibleBlob blob(fixture.mapped().platform());
  fixture.mapped().platform().set_irql(Irql::dispatch);
  static_cast<void>(blob.create(fixture.device(), context_id, resource, upload, page));
  fixture.mapped().platform().set_irql(Irql::passive);
  CHECK(fixture.mapped().platform().claim(Violation::irql_ceiling));
  while (fixture.mapped().platform().claim(Violation::irql_ceiling)) {}
  /* The wait for its turn at the queue, which blocks. */
  CHECK(fixture.mapped().platform().claim(Violation::blocking_at_dispatch)); }

/* The device refusing each step in turn: a create it will not make, an unmap it
 * will not take, and an unref it refuses. Each leaves the blob where it was,
 * because what the driver believes about the device's memory has to be what the
 * device said. */
TEST(host_visible_blob, a_device_that_refuses_a_step_leaves_it_where_it_was)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  HostVisibleBlob blob(fixture.mapped().platform());
  host.refuse_next();
  CHECK_STATUS(blob.create(fixture.device(), context_id, resource, upload, page), Status::not_supported);
  CHECK(!blob.created());

  CHECK_STATUS(blob.create(fixture.device(), context_id, resource, upload, page), Status::ok);
  CHECK_STATUS(blob.map(fixture.device(), page), Status::ok);
  host.refuse_next();
  CHECK_STATUS(blob.unmap(fixture.device()), Status::not_supported);
  CHECK(blob.mapped());                        /* still the device's to hold */
  CHECK_EQ(blob.offset(), page);

  /* Destroying it unmaps first, so a refused unmap stops the destroy before it
   * unrefs something the device still has placed. */
  host.refuse_next();
  CHECK_STATUS(blob.destroy(fixture.device()), Status::not_supported);
  CHECK(blob.created());
  CHECK(blob.mapped());

  CHECK_STATUS(blob.unmap(fixture.device()), Status::ok);
  host.refuse_next();
  CHECK_STATUS(blob.destroy(fixture.device()), Status::not_supported);
  CHECK(blob.created());
  CHECK_STATUS(blob.destroy(fixture.device()), Status::ok);
  CHECK(!blob.created()); }

/* A device that stops answering: the blob is left as it was, not as the driver
 * hoped. */
TEST(host_visible_blob, a_silent_device_leaves_every_step_undone)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  HostVisibleBlob blob(fixture.mapped().platform());
  host.answer_nothing();
  CHECK_STATUS(blob.create(fixture.device(), context_id, resource, upload, page), Status::not_found);
  CHECK(!blob.created()); }

TEST(host_visible_blob_invariants, a_mapping_with_no_resource_is_caught)
{ MappedVirtioFunction mapped;
  HostVisibleBlob blob(mapped.platform());
  CHECK(blob.invariants_hold());
  av::HostVisibleBlobProbe::map_without_a_resource(blob);
  CHECK(!blob.invariants_hold()); }

/* The device that held it is gone - a power transition destroyed it - so there
 * is nothing to tell and nothing left to hold. */
TEST(host_visible_blob_abandon, forgets_a_blob_whose_device_went_away)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  HostVisibleBlob blob(fixture.mapped().platform());
  CHECK_STATUS(blob.create(fixture.device(), context_id, resource, upload, page), Status::ok);
  CHECK_STATUS(blob.map(fixture.device(), page), Status::ok);
  const std::uint32_t answered = host.answered();
  blob.abandon();
  CHECK(!blob.created());
  CHECK(!blob.mapped());
  CHECK_EQ(blob.offset(), 0u);
  CHECK_EQ(host.answered(), answered);   /* nothing was said to a device that is gone */
  CHECK(blob.invariants_hold()); }

/* A device that answers bytes no device sends is not a device to believe: the
 * blob is left as it was rather than recorded from a reply that did not
 * decode. */
TEST(host_visible_blob, an_answer_that_does_not_decode_is_malformed)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  host.answer_garbage();
  HostVisibleBlob blob(fixture.mapped().platform());
  CHECK_STATUS(blob.create(fixture.device(), context_id, resource, upload, page), Status::malformed);
  CHECK(!blob.created()); }

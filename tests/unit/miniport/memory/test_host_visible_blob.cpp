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
#include <cstring>
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
    static void map_without_a_resource(HostVisibleBlob &blob)
    {
        blob.resource_ = 0u;
        blob.mapped_ = true;
    }
};

}

namespace {

constexpr std::uint16_t control_queue = 0u;
constexpr std::uint64_t page = 0x1000u;
constexpr std::uint32_t resource = 0x01000000u;
constexpr std::uint32_t context_id = 3u;
const HostMemoryRequest upload(HostMemoryRequest::Kind::upload, 0u);

class Host {
public:
    explicit Host(MappedVirtioFunction &mapped) : queue_(mapped.queue_device(control_queue))
    { mapped.platform().on_stall(&Host::stall, this); }

    void answer_map_with(std::uint32_t map_info) { map_info_ = map_info; }
    void refuse_next() { refuse_ = true; }
    void refuse_attaches() { refuse_attach_ = true; }
    void answer_nothing() { silent_ = true; }
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
        } else if (host->refuse_attach_ && asked == 0x0106u) {
            answer.set(GpuResponse::err_out_of_memory);
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
    bool                refuse_attach_ = false;
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

}

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
  CHECK(blob.invariants_hold());
  CHECK_EQ(blob.offset(), 4u * page);
  CHECK_STATUS(blob.unmap(fixture.device()), Status::ok);
  CHECK(!blob.mapped());
  CHECK(blob.created());
  CHECK_STATUS(blob.destroy(fixture.device()), Status::ok);
  CHECK(!blob.created());
  CHECK_EQ(host.answered(), 4u);
  CHECK(blob.invariants_hold()); }

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
  CHECK_EQ(host.queue().request_type(0u), 0x0209u);
  CHECK_EQ(host.queue().request_type(1u), 0x0102u);   }

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
  CHECK(fixture.mapped().platform().claim(Violation::blocking_at_dispatch)); }

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
  CHECK(blob.mapped());
  CHECK_EQ(blob.offset(), page);

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

TEST(host_visible_blob, a_create_the_device_did_not_answer_is_unrefed_before_its_id_goes_back)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  HostVisibleBlob blob(fixture.mapped().platform());
  host.answer_nothing();
  CHECK_STATUS(blob.create(fixture.device(), context_id, resource, upload, page), Status::not_found);
  CHECK(blob.created());
  CHECK(!blob.mapped());
  CHECK_STATUS(blob.destroy(fixture.device()), Status::not_found);
  CHECK(blob.created()); }

TEST(host_visible_blob_invariants, a_mapping_with_no_resource_is_caught)
{ MappedVirtioFunction mapped;
  HostVisibleBlob blob(mapped.platform());
  CHECK(blob.invariants_hold());
  av::HostVisibleBlobProbe::map_without_a_resource(blob);
  CHECK(!blob.invariants_hold()); }

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
  CHECK_EQ(host.answered(), answered);
  CHECK(blob.invariants_hold()); }

TEST(host_visible_blob, an_answer_that_does_not_decode_is_malformed)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  host.answer_garbage();
  HostVisibleBlob blob(fixture.mapped().platform());
  CHECK_STATUS(blob.create(fixture.device(), context_id, resource, upload, page), Status::malformed);
  CHECK(!blob.created()); }

namespace {

const HostMemoryRequest readback(HostMemoryRequest::Kind::readback, 0u);
constexpr std::uint64_t lent_at = 0x7FF000000000ull;

std::uint32_t u32_at(const std::uint8_t *bytes, std::size_t at)
{
    std::uint32_t value = 0u;
    std::memcpy(&value, bytes + at, sizeof value);
    return value;
}

std::uint64_t u64_at(const std::uint8_t *bytes, std::size_t at)
{
    std::uint64_t value = 0u;
    std::memcpy(&value, bytes + at, sizeof value);
    return value;
}

}

TEST(host_visible_blob_create_over, makes_the_blob_then_names_its_pages_run_by_run)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  const std::uint64_t frames[3] = {0x40000u, 0x41000u, 0x90000u};
  fixture.mapped().platform().lend_process_range(lent_at, frames, 3u);
  void *locked = nullptr;
  CHECK_STATUS(fixture.mapped().platform().lock_process_pages(lent_at, 3u * page, locked), Status::ok);
  HostVisibleBlob blob(fixture.mapped().platform());
  CHECK_STATUS(blob.create_over(fixture.device(), context_id, resource, readback, locked, 3u * page), Status::ok);
  CHECK(blob.created());
  CHECK(!blob.mapped());
  CHECK_EQ(host.answered(), 2u);
  const std::uint8_t *const create = host.queue().request(0u);
  CHECK_EQ(u32_at(create, 0u), 0x010Cu);
  CHECK_EQ(u32_at(create, 16u), context_id);
  CHECK_EQ(u32_at(create, 24u), resource);
  CHECK_EQ(u32_at(create, 28u), CreateBlobRequest::memory_host3d_guest);
  CHECK_EQ(u32_at(create, 32u), 0u);
  CHECK_EQ(u32_at(create, 36u), 0u);
  CHECK_EQ(u64_at(create, 40u), std::uint64_t{readback.encoded()});
  CHECK_EQ(u64_at(create, 48u), 3u * page);
  const std::uint8_t *const attach = host.queue().request(1u);
  CHECK_EQ(u32_at(attach, 0u), 0x0106u);
  CHECK_EQ(u32_at(attach, 24u), resource);
  CHECK_EQ(u32_at(attach, 28u), 2u);
  CHECK_EQ(u64_at(attach, 32u), 0x40000u);
  CHECK_EQ(u32_at(attach, 40u), 2u * page);
  CHECK_EQ(u64_at(attach, 48u), 0x90000u);
  CHECK_EQ(u32_at(attach, 56u), page);
  HostVisibleBlob lent(fixture.mapped().platform(), resource);
  CHECK(lent.created());
  CHECK(!lent.mapped());
  CHECK_STATUS(lent.destroy(fixture.device()), Status::ok);
  CHECK_EQ(host.queue().request_type(2u), 0x0102u);
  fixture.mapped().platform().unlock_process_pages(locked); }

TEST(host_visible_blob_create_over, a_range_past_one_piece_is_named_in_pieces)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  constexpr std::uint32_t pages = HostVisibleBlob::pages_per_piece + 1u;
  static std::uint64_t frames[pages];
  for (std::uint32_t index = 0u; index < pages; index++) frames[index] = 0x100000u + std::uint64_t{index} * 2u * page;
  fixture.mapped().platform().lend_process_range(lent_at, frames, pages);
  void *locked = nullptr;
  CHECK_STATUS(fixture.mapped().platform().lock_process_pages(lent_at, pages * page, locked), Status::ok);
  HostVisibleBlob blob(fixture.mapped().platform());
  CHECK_STATUS(blob.create_over(fixture.device(), context_id, resource, readback, locked, pages * page), Status::ok);
  CHECK_EQ(host.answered(), 3u);
  CHECK_EQ(u32_at(host.queue().request(1u), 28u), HostVisibleBlob::pages_per_piece);
  CHECK_EQ(u32_at(host.queue().request(2u), 28u), 1u);
  CHECK_EQ(u64_at(host.queue().request(2u), 32u), frames[pages - 1u]);
  fixture.mapped().platform().unlock_process_pages(locked); }

TEST(host_visible_blob_create_over, refuses_what_it_cannot_name_and_counts_a_blob_the_device_made)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  const std::uint64_t frames[1] = {0x40000u};
  fixture.mapped().platform().lend_process_range(lent_at, frames, 1u);
  void *locked = nullptr;
  CHECK_STATUS(fixture.mapped().platform().lock_process_pages(lent_at, page, locked), Status::ok);
  VirtioGpuDevice &device = fixture.device();
  HostVisibleBlob  blob(fixture.mapped().platform());
  CHECK_STATUS(blob.create_over(device, 0u, resource, readback, locked, page), Status::invalid_parameter);
  CHECK_STATUS(blob.create_over(device, context_id, 0u, readback, locked, page), Status::invalid_parameter);
  CHECK_STATUS(blob.create_over(device, context_id, resource, HostMemoryRequest(), locked, page),
               Status::invalid_parameter);
  CHECK_STATUS(blob.create_over(device, context_id, resource, readback, nullptr, page), Status::invalid_parameter);
  CHECK_STATUS(blob.create_over(device, context_id, resource, readback, locked, 0u), Status::invalid_parameter);
  CHECK_STATUS(blob.create_over(device, context_id, resource, readback, locked, page + 1u),
               Status::invalid_parameter);
  CHECK_STATUS(blob.create_over(device, context_id, resource, readback, locked, (std::uint64_t{1} << 32u) * page),
               Status::invalid_parameter);
  CHECK_EQ(host.answered(), 0u);
  fixture.mapped().platform().fail_allocation_at(1u);
  CHECK_STATUS(blob.create_over(device, context_id, resource, readback, locked, page), Status::no_memory);
  fixture.mapped().platform().fail_allocation_at(2u);
  CHECK_STATUS(blob.create_over(device, context_id, resource, readback, locked, page), Status::no_memory);
  CHECK(!blob.created());
  host.refuse_next();
  CHECK_STATUS(blob.create_over(device, context_id, resource, readback, locked, page), Status::not_supported);
  CHECK(!blob.created());
  host.refuse_attaches();
  CHECK_STATUS(blob.create_over(device, context_id, resource, readback, locked, page), Status::not_supported);
  CHECK(blob.created());
  CHECK_STATUS(blob.create_over(device, context_id, resource, readback, locked, page), Status::invalid_state);
  CHECK_STATUS(blob.destroy(device), Status::ok);
  std::uint64_t not_a_lock = 0u;
  CHECK_STATUS(blob.create_over(device, context_id, resource, readback, &not_a_lock, page), Status::invalid_parameter);
  CHECK(blob.created());
  fixture.mapped().platform().unlock_process_pages(locked); }

TEST(host_visible_blob_create_over, a_page_the_pager_cannot_name_or_a_device_that_does_not_answer_is_said)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  Host host(fixture.mapped());
  const std::uint64_t frames[2] = {0x40000u, 0u};
  fixture.mapped().platform().lend_process_range(lent_at, frames, 2u);
  void *locked = nullptr;
  CHECK_STATUS(fixture.mapped().platform().lock_process_pages(lent_at, 2u * page, locked), Status::ok);
  VirtioGpuDevice &device = fixture.device();
  HostVisibleBlob  blob(fixture.mapped().platform());
  CHECK_STATUS(blob.create_over(device, context_id, resource, readback, locked, 2u * page), Status::invalid_parameter);
  CHECK(blob.created());
  CHECK_STATUS(blob.destroy(device), Status::ok);
  host.answer_nothing();
  CHECK_STATUS(blob.create_over(device, context_id, resource, readback, locked, 2u * page), Status::not_found);
  CHECK(blob.created());
  fixture.mapped().platform().unlock_process_pages(locked); }

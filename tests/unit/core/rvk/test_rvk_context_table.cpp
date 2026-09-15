/* The adapter's rvk contexts, over the pieces a started adapter has: a mapped
 * virtio-gpu-rvk-pci with its doorbell pages, dxgkrnl's handle table, the
 * allocation table with a process's ring, fence page and reply slot mapped into
 * the aperture, and a device the test answers. A record binds the context to
 * those allocations; a submission rings its doorbell; the DPC's serve finishes on
 * the node only what the fence page says the host completed. */
#include "core/ddi/dxgk_allocation_info_record.hpp"
#include "core/ddi/dxgk_build_paging_buffer_map_aperture_segment_record.hpp"
#include "core/ddi/dxgk_build_paging_buffer_unmap_aperture_segment_record.hpp"
#include "core/ddi/dxgkarg_create_allocation_record.hpp"
#include "core/memory/allocation.hpp"
#include "core/memory/allocation_table.hpp"
#include "core/memory/aperture_segment.hpp"
#include "core/memory/host_visible_segment.hpp"
#include "core/memory/memory_segments.hpp"
#include "core/miniport/interrupt_notification.hpp"
#include "core/rvk/rvk_context_table.hpp"
#include "core/scheduling/execution_node.hpp"
#include "core/support/status.hpp"
#include "core/virtio/virtio_gpu_device.hpp"
#include "harness/device/fake_register_bytes.hpp"
#include "harness/device/fake_virtqueue_device.hpp"
#include "harness/device/mapped_virtio_function.hpp"
#include "harness/dxgkrnl/fake_handle_table.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/violation.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/context/rvk_context_record.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/host_visible_bind_record.hpp"
#include "proto/submission/fence_page.hpp"
#include "proto/submission/ring_header.hpp"
#include "proto/submission/submission_descriptor.hpp"
#include "proto/virtio_gpu/gpu_response.hpp"

#include <cstdint>
#include <cstring>
#include <optional>

namespace av {

struct RvkContextTableProbe {
    static void set_live(RvkContextTable &table, std::uint32_t slot, bool live) { table.live_[slot] = live; }
    static void set_context(RvkContextTable &table, std::uint32_t slot, void *context) { table.contexts_[slot] = context; }
    static HostVisibleBinding &chunk(RvkContextTable &table, std::uint32_t slot, std::uint32_t index)
    {
        return table.chunks_[slot][index];
    }
    static const Allocation *swap_use(RvkContextTable &table, std::uint32_t slot, std::uint32_t role,
                                      const Allocation *use)
    {
        const Allocation *const was = table.uses_[slot][role];
        table.uses_[slot][role] = use;
        return was;
    }
    static std::uint8_t *swap_view(RvkContextTable &table, std::uint32_t slot, bool ring, std::uint8_t *view)
    {
        std::uint8_t *&held = ring ? table.ring_views_[slot] : table.fence_views_[slot];
        std::uint8_t *const was = held;
        held = view;
        return was;
    }
};

}  // namespace av

using av::Allocation;
using av::AllocationDescriptor;
using av::AllocationKind;
using av::CapsetInfo;
using av::ExecutionNode;
using av::FencePage;
using av::GpuResponse;
using av::Irql;
using av::RvkContextRecord;
using av::RvkContextTable;
using av::RvkContextTableProbe;
using av::ScanoutTarget;
using av::Status;
using av::SubApi;
using av::SubmissionDescriptor;
using av::SurfaceLayout;
using av::Violation;

namespace {

constexpr std::uint64_t page = 0x1000u;
constexpr std::uint32_t ctx_create_type = 0x0200u;
constexpr std::uint32_t ctx_destroy_type = 0x0201u;
constexpr std::uint32_t create_blob_type = 0x010Cu;
constexpr std::uint32_t unref_type = 0x0102u;
constexpr std::uint32_t submit_3d_type = 0x0207u;
constexpr std::uint8_t  uuid[av::GpuIdentity::uuid_bytes] = {};

CapsetInfo capset()
{
    return CapsetInfo(av::ProtocolVersion(1u, 0u), 0x7u, 64u, 12u, 20u, 0u,
                      av::GpuIdentity(0u, 0u, 0u, uuid, 0u, 0u, "gpu"));
}

/* A device answering every control request with OK_NODATA from the stall
 * between the driver's polls, or refusing one at a chosen index. */
class Host {
public:
    explicit Host(av::MappedVirtioFunction &mapped) : queue_(mapped.queue_device(0u))
    { mapped.platform().on_stall(&Host::stall, this); }

    void                 refuse_at(int index) { refuse_at_ = index; }
    av::FakeVirtqueueDevice &queue() { return queue_; }
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
        if (host->queue_.pending_type() == 0u) return;
        GpuResponse answer;
        answer.set(static_cast<int>(host->answered_) == host->refuse_at_ ? GpuResponse::err_invalid_parameter
                                                                         : GpuResponse::ok_nodata);
        host->answered_++;
        static_cast<void>(host->queue_.respond_all(answer.bytes(), static_cast<std::uint32_t>(answer.length())));
    }

    av::FakeVirtqueueDevice queue_;
    std::uint32_t           answered_ = 0u;
    int                     refuse_at_ = -1;
};

/* One allocation of a process: created in the table, given a D3DKMT handle,
 * and mapped into the aperture over guest memory the platform can map back. */
class ProcessAllocation {
public:
    ProcessAllocation(av::MockPlatform &platform, av::AllocationTable &table, av::FakeHandleTable &handles,
                      AllocationKind kind, std::uint64_t bytes)
        : platform_(platform), bytes_(bytes), block_(nullptr), frames_{}, encoded_{}, allocation_(nullptr), handle_(0u)
    {
        const AllocationDescriptor descriptor(kind, true, SurfaceLayout(), bytes, ScanoutTarget());
        CHECK_STATUS(descriptor.encode(encoded_, sizeof encoded_), Status::ok);
        av::DxgkAllocationInfoRecord info = {};
        info.pPrivateDriverData = encoded_;
        info.PrivateDriverDataSize = AllocationDescriptor::encoded_size;
        av::DxgkargCreateAllocationRecord create = {};
        create.NumAllocations = 1u;
        create.pAllocationInfo = &info;
        CHECK_STATUS(table.create(create), Status::ok);
        allocation_ = table.find(info.hAllocation);
        handle_ = handles.insert(info.hAllocation);
        block_ = static_cast<std::uint8_t *>(platform.allocate_contiguous(bytes));
        std::memset(block_, 0, bytes);
        for (std::uint64_t index = 0u; index < bytes / page; index++)
            frames_[index] = platform.physical_address(block_) + index * page;
    }
    ~ProcessAllocation() { platform_.release_contiguous(block_, bytes_); }

    /* What VidMm does when it makes the allocation resident. */
    void map_into_aperture(std::uint64_t offset_in_pages)
    {
        av::DxgkBuildPagingBufferMapApertureSegmentRecord mapping = {};
        mapping.SegmentId = av::ApertureSegment::segment_id;
        mapping.OffsetInPages = offset_in_pages;
        mapping.NumberOfPages = bytes_ / page;
        mapping.pMdl = platform_.attach_page_frames(frames_, static_cast<std::uint32_t>(bytes_ / page));
        CHECK_STATUS(allocation_->map_aperture(mapping), Status::ok);
    }
    void evict(std::uint64_t offset_in_pages)
    {
        av::DxgkBuildPagingBufferUnmapApertureSegmentRecord unmapping = {};
        unmapping.SegmentId = av::ApertureSegment::segment_id;
        unmapping.OffsetInPages = offset_in_pages;
        unmapping.NumberOfPages = bytes_ / page;
        allocation_->unmap_aperture(unmapping);
    }

    std::uint32_t handle() const { return handle_; }
    Allocation   *allocation() const { return allocation_; }
    std::uint8_t *bytes() const { return block_; }

private:
    av::MockPlatform &platform_;
    std::uint64_t     bytes_;
    std::uint8_t     *block_;
    std::uint64_t     frames_[4];
    std::uint8_t      encoded_[AllocationDescriptor::encoded_size];
    Allocation       *allocation_;
    std::uint32_t     handle_;
};

class Fixture {
public:
    Fixture()
        : aperture_(mapped_.platform()), host_visible_(mapped_.platform()),
          segments_(mapped_.platform(), aperture_, host_visible_),
          allocations_(mapped_.platform(), aperture_, segments_),
          table_(mapped_.platform(), mapped_.interface(), allocations_),
          notification_(mapped_.platform(), mapped_.interface()), node_(mapped_.platform())
    {
        mapped_.function().add_doorbell_region();
        CHECK_STATUS(mapped_.map(), Status::ok);
        CHECK_STATUS(aperture_.initialize(std::int64_t{1} << 30u), Status::ok);
        device_.emplace(mapped_.platform(), mapped_.mappings(), mapped_.layout());
        CHECK_STATUS(device_->start(6u), Status::ok);
        ring_.emplace(mapped_.platform(), allocations_, handles_, AllocationKind::context_ring, 2u * page);
        fence_.emplace(mapped_.platform(), allocations_, handles_, AllocationKind::fence_page, page);
        reply_.emplace(mapped_.platform(), allocations_, handles_, AllocationKind::reply_slot, page);
        ring_->map_into_aperture(10u);
        fence_->map_into_aperture(20u);
        reply_->map_into_aperture(30u);
    }
    ~Fixture()
    {
        mapped_.platform().set_irql(Irql::passive);
        mapped_.platform().on_stall(nullptr, nullptr);
    }

    RvkContextRecord record(std::uint8_t order = 12u) const
    {
        return RvkContextRecord(SubApi::vulkan, order, ring_->handle(), fence_->handle(), reply_->handle());
    }
    Status bind(void *context) { return bind(context, record()); }
    Status bind(void *context, const RvkContextRecord &asked)
    {
        return table_.bind(context, asked, *device_, mapped_.function().doorbells(), capset());
    }
    Status submit(void *context, const SubmissionDescriptor &descriptor, std::uint64_t &awaited)
    {
        std::uint8_t bytes[SubmissionDescriptor::encoded_size] = {};
        static_cast<void>(descriptor.encode(bytes, sizeof bytes));
        mapped_.platform().set_irql(Irql::dispatch);
        const Status status = table_.submit(context, bytes, sizeof bytes, awaited);
        mapped_.platform().set_irql(Irql::passive);
        return status;
    }
    /* What the host writes when it finishes the batches through `completed`. */
    void complete_through(std::uint64_t completed, std::uint32_t fault = 0u)
    {
        const FencePage written = fault == 0u ? FencePage(completed, completed) : FencePage(completed, completed, fault);
        static_cast<void>(written.encode(fence_->bytes(), 32u));
    }
    void serve()
    {
        mapped_.platform().set_irql(Irql::dispatch);
        table_.serve(node_);
        CHECK_STATUS(node_.complete(notification_), Status::ok);
        mapped_.platform().set_irql(Irql::passive);
    }

    av::MockPlatform         &platform() { return mapped_.platform(); }
    av::MappedVirtioFunction &mapped() { return mapped_; }
    av::VirtioGpuDevice      &device() { return *device_; }
    av::FakeHandleTable      &handles() { return handles_; }
    av::AllocationTable      &allocations() { return allocations_; }
    RvkContextTable          &table() { return table_; }
    ExecutionNode            &node() { return node_; }
    ProcessAllocation        &ring() { return *ring_; }
    ProcessAllocation        &fence() { return *fence_; }
    ProcessAllocation        &reply() { return *reply_; }

private:
    av::MappedVirtioFunction                mapped_;
    av::FakeHandleTable                     handles_;
    av::ApertureSegment                     aperture_;
    av::HostVisibleSegment                  host_visible_;
    av::MemorySegments                      segments_;
    av::AllocationTable                     allocations_;
    RvkContextTable                         table_;
    av::InterruptNotification               notification_;
    ExecutionNode                           node_;
    std::optional<av::VirtioGpuDevice>      device_;
    std::optional<ProcessAllocation>        ring_;
    std::optional<ProcessAllocation>        fence_;
    std::optional<ProcessAllocation>        reply_;
};

int first_context = 0;
int second_context = 0;
constexpr bool both_views[] = {true, false};   /* the ring header's, then the fence page's */

}  // namespace

TEST(rvk_context_table_bind, sets_the_host_context_up_on_the_process_s_own_pages)
{ Fixture fixture;
  Host host(fixture.mapped());
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  CHECK(fixture.table().invariants_hold());
  CHECK_EQ(fixture.table().bound(), 1u);
  CHECK_EQ(host.count_of(ctx_create_type), 1u);
  CHECK_EQ(host.count_of(create_blob_type), 3u);
  CHECK_EQ(host.count_of(submit_3d_type), 1u);
  /* Slot 0: host context 1, blobs from the driver's first id, whose one memory
   * entry is the ring's first page - the process's memory, not the driver's. */
  CHECK_EQ(host.field(ctx_create_type, 0u, 16u), 1u);
  CHECK_EQ(host.field(create_blob_type, 0u, 24u), av::RvkContextBinding::first_resource_id);
  CHECK_EQ(host.field(create_blob_type, 0u, 36u), 1u);   /* one entry: the ring is contiguous here */
  CHECK_EQ(host.field(create_blob_type, 0u, 56u),
           static_cast<std::uint32_t>(fixture.platform().physical_address(fixture.ring().bytes())));
  CHECK_EQ(host.field(create_blob_type, 1u, 56u),
           static_cast<std::uint32_t>(fixture.platform().physical_address(fixture.fence().bytes())));
  /* The doorbell's fence queue got the first message past the sources. */
  CHECK_EQ(fixture.mapped().common().queue(2u).vector, 3u);
  /* The driver maps the ring header and the fence page, and nothing else. */
  CHECK_EQ(fixture.platform().mapped_frames(), 2u);
  CHECK(fixture.platform().logged_line("rvk context on doorbell 0 message 3 status 0x00000000"));
  CHECK_EQ(fixture.handles().outstanding(), 0u);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok);
  CHECK_EQ(fixture.platform().mapped_frames(), 0u); }

/* A submission the process describes lands in its ring and rings its doorbell,
 * and the node completes it only when the host's fence page says so. */
TEST(rvk_context_table_submit, rings_the_doorbell_and_the_node_waits_for_the_host)
{ Fixture fixture;
  Host host(fixture.mapped());
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  std::uint64_t awaited = 99u;
  CHECK_STATUS(fixture.submit(&first_context, SubmissionDescriptor(1u, 0u, 32u), awaited), Status::ok);
  CHECK_EQ(awaited, 1u);
  CHECK_EQ(fixture.mapped().function().doorbells().rings(0u), 1u);
  std::uint64_t tail = 0u;
  std::memcpy(&tail, fixture.ring().bytes() + 8u, sizeof tail);
  CHECK_EQ(tail, 32u);
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_STATUS(fixture.node().submit(5u, &first_context, awaited), Status::ok);
  fixture.platform().set_irql(Irql::passive);
  fixture.complete_through(0u);
  fixture.serve();
  CHECK_EQ(fixture.node().current_fence(), 0u);   /* the host has finished nothing */
  fixture.complete_through(1u);
  fixture.serve();
  CHECK_EQ(fixture.node().current_fence(), 5u);
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

/* What a process hands the driver is judged before anything is published. */
TEST(rvk_context_table_submit, refuses_what_is_not_a_descriptor_of_a_live_context_s_ring)
{ Fixture fixture;
  Host host(fixture.mapped());
  std::uint64_t awaited = 7u;
  CHECK_STATUS(fixture.submit(&first_context, SubmissionDescriptor(1u, 0u, 32u), awaited), Status::invalid_parameter);
  CHECK_EQ(awaited, 0u);
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  CHECK_STATUS(fixture.submit(&first_context, SubmissionDescriptor(1u, 0u, 5000u), awaited),
               Status::invalid_parameter);   /* longer than an order-12 ring */
  std::uint8_t bytes[SubmissionDescriptor::encoded_size] = {};
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_STATUS(fixture.table().submit(&first_context, bytes, sizeof bytes, awaited), Status::invalid_parameter);
  CHECK_STATUS(fixture.table().submit(&first_context, nullptr, 0u, awaited), Status::invalid_parameter);
  CHECK_STATUS(fixture.table().submit(&first_context, bytes, 8u, awaited), Status::invalid_parameter);
  fixture.platform().set_irql(Irql::passive);
  /* Not at the tail the ring submitter published: the binding's own refusal. */
  CHECK_STATUS(fixture.submit(&first_context, SubmissionDescriptor(1u, 8u, 32u), awaited), Status::invalid_parameter);
  CHECK_EQ(fixture.mapped().function().doorbells().rings(0u), 0u);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

TEST(rvk_context_table_bind, refuses_a_record_that_does_not_name_the_process_s_transport_memory)
{ Fixture fixture;
  Host host(fixture.mapped());
  const std::uint32_t ring = fixture.ring().handle();
  const std::uint32_t fence = fixture.fence().handle();
  const std::uint32_t reply = fixture.reply().handle();
  const RvkContextRecord cases[] = {
      RvkContextRecord(SubApi::vulkan, 12u, 0x7777u, fence, reply),   /* no such handle */
      RvkContextRecord(SubApi::vulkan, 12u, fence, ring, reply),      /* roles swapped */
      RvkContextRecord(SubApi::vulkan, 12u, ring, fence, 0x7777u),
      RvkContextRecord(SubApi::vulkan, 13u, ring, fence, reply),      /* 32 + 8192 bytes do not fit two pages */
  };
  for (const RvkContextRecord &asked : cases) {
      CHECK_STATUS(fixture.bind(&first_context, asked), Status::invalid_parameter);
      CHECK_EQ(fixture.table().bound(), 0u);
      CHECK_EQ(fixture.handles().outstanding(), 0u);
      CHECK(fixture.table().invariants_hold());
  }
  CHECK_EQ(host.queue().requests(), 0u);
  CHECK_STATUS(fixture.bind(nullptr), Status::invalid_state);
  /* Pages VidMm has taken away are pages the device may not be given. */
  fixture.reply().evict(30u);
  CHECK_STATUS(fixture.bind(&first_context), Status::invalid_parameter);
  CHECK_EQ(fixture.platform().mapped_frames(), 0u); }

TEST(rvk_context_table_bind, a_context_bound_twice_or_memory_already_in_use_is_refused)
{ Fixture fixture;
  Host host(fixture.mapped());
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  CHECK_STATUS(fixture.bind(&first_context), Status::invalid_state);
  /* Another context naming the same ring would publish one process's
   * submissions twice. */
  CHECK_STATUS(fixture.bind(&second_context), Status::invalid_parameter);
  /* A device with fewer doorbell pages than the slot needs. */
  av::FakeRegisterBytes one_page(0x800u);
  CHECK_STATUS(fixture.table().bind(&second_context, fixture.record(), fixture.device(), one_page, capset()),
               Status::no_space);
  CHECK_EQ(fixture.table().bound(), 1u);
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

/* Every doorbell page is a context; the one past them is refused, and the
 * first slot given back is the next one taken. */
TEST(rvk_context_table_bind, every_doorbell_taken_refuses_the_next_context)
{ Fixture fixture;
  Host host(fixture.mapped());
  static int contexts[RvkContextTable::max_contexts + 1u];
  std::optional<ProcessAllocation> memory[RvkContextTable::max_contexts + 1u][3];
  for (std::uint32_t index = 0u; index <= RvkContextTable::max_contexts; index++) {
      memory[index][0].emplace(fixture.platform(), fixture.allocations(), fixture.handles(),
                               AllocationKind::context_ring, 2u * page);
      memory[index][1].emplace(fixture.platform(), fixture.allocations(), fixture.handles(),
                               AllocationKind::fence_page, page);
      memory[index][2].emplace(fixture.platform(), fixture.allocations(), fixture.handles(),
                               AllocationKind::reply_slot, page);
      for (std::uint32_t role = 0u; role < 3u; role++) memory[index][role]->map_into_aperture(100u + index * 4u + role);
  }
  const auto record_of = [&](std::uint32_t index) {
      return RvkContextRecord(SubApi::vulkan, 12u, memory[index][0]->handle(), memory[index][1]->handle(),
                              memory[index][2]->handle());
  };
  for (std::uint32_t index = 0u; index < RvkContextTable::max_contexts; index++)
      CHECK_STATUS(fixture.bind(&contexts[index], record_of(index)), Status::ok);
  CHECK_EQ(fixture.table().bound(), RvkContextTable::max_contexts);
  const std::uint32_t last = RvkContextTable::max_contexts;
  CHECK_STATUS(fixture.bind(&contexts[last], record_of(last)), Status::no_space);
  CHECK_STATUS(fixture.table().unbind(&contexts[7], &fixture.device()), Status::ok);
  CHECK_STATUS(fixture.bind(&contexts[last], record_of(last)), Status::ok);
  CHECK(fixture.platform().logged_line("rvk context on doorbell 7 "));
  CHECK(fixture.table().invariants_hold());
  for (std::uint32_t index = 0u; index <= last; index++)
      if (index != 7u) CHECK_STATUS(fixture.table().unbind(&contexts[index], &fixture.device()), Status::ok);
  CHECK_EQ(fixture.platform().mapped_frames(), 0u); }

TEST(rvk_context_table_bind, a_page_that_cannot_be_mapped_or_a_refused_vector_binds_nothing)
{ Fixture fixture;
  Host host(fixture.mapped());
  for (std::uint32_t nth = 1u; nth <= 2u; nth++) {
      fixture.platform().fail_frame_map_at(nth);
      CHECK_STATUS(fixture.bind(&first_context), Status::no_memory);
      CHECK_EQ(fixture.platform().mapped_frames(), 0u);
      CHECK_EQ(fixture.table().bound(), 0u);
  }
  fixture.platform().fail_frame_map_at(0u);
  fixture.mapped().common().refuse_vectors();
  CHECK_STATUS(fixture.bind(&first_context), Status::no_space);
  CHECK_EQ(fixture.platform().mapped_frames(), 0u);
  CHECK_EQ(host.queue().requests(), 0u);
  CHECK(fixture.table().invariants_hold()); }

/* A setup the device refuses releases what it created and leaves nothing bound;
 * the same record binds once the device agrees. */
TEST(rvk_context_table_bind, a_setup_the_device_refuses_is_undone)
{ Fixture fixture;
  Host host(fixture.mapped());
  host.refuse_at(2);   /* the second blob */
  CHECK_STATUS(fixture.bind(&first_context), Status::not_supported);
  CHECK_EQ(fixture.table().bound(), 0u);
  CHECK_EQ(host.count_of(ctx_destroy_type), 1u);
  CHECK_EQ(host.count_of(unref_type), 1u);
  CHECK_EQ(fixture.platform().mapped_frames(), 0u);
  CHECK(fixture.platform().logged_line("status 0xC00000BB"));
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

TEST(rvk_context_table_bind, a_record_the_capset_forbids_is_refused_before_the_device_is_asked)
{ Fixture fixture;
  Host host(fixture.mapped());
  const RvkContextRecord opengl(SubApi::opengl, 12u, fixture.ring().handle(), fixture.fence().handle(),
                                fixture.reply().handle());
  const CapsetInfo vulkan_only(av::ProtocolVersion(1u, 0u), 0x1u, 64u, 12u, 20u, 0u,
                               av::GpuIdentity(0u, 0u, 0u, uuid, 0u, 0u, "gpu"));
  CHECK_STATUS(fixture.table().bind(&first_context, opengl, fixture.device(),
                                    fixture.mapped().function().doorbells(), vulkan_only),
               Status::invalid_parameter);
  CHECK_EQ(host.count_of(ctx_create_type), 0u);
  CHECK_EQ(fixture.table().bound(), 0u); }

TEST(rvk_context_table_unbind, tears_down_on_the_device_or_lets_go_without_one)
{ Fixture fixture;
  Host host(fixture.mapped());
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::invalid_parameter);
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok);
  CHECK_EQ(host.count_of(ctx_destroy_type), 1u);
  CHECK_EQ(host.count_of(unref_type), 3u);
  std::uint64_t awaited = 0u;
  CHECK_STATUS(fixture.submit(&first_context, SubmissionDescriptor(1u, 0u, 32u), awaited), Status::invalid_parameter);
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  const std::uint32_t asked = host.queue().requests();
  CHECK_STATUS(fixture.table().unbind(&first_context, nullptr), Status::ok);   /* no device: nothing sent */
  CHECK_EQ(host.queue().requests(), asked);
  CHECK_EQ(fixture.platform().mapped_frames(), 0u);
  CHECK(fixture.table().invariants_hold()); }

/* A device that will not answer a teardown still has its context let go of. */
TEST(rvk_context_table_unbind, a_device_that_stopped_answering_still_releases_the_slot)
{ Fixture fixture;
  { Host host(fixture.mapped());
    CHECK_STATUS(fixture.bind(&first_context), Status::ok); }
  fixture.platform().on_stall(nullptr, nullptr);
  CHECK_STATUS(fixture.device().stop(), Status::ok);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok);
  CHECK_EQ(fixture.table().bound(), 0u);
  CHECK_EQ(fixture.platform().mapped_frames(), 0u); }

TEST(rvk_context_table_abandon_all, a_reset_device_serves_and_accepts_nothing_until_unbound)
{ Fixture fixture;
  Host host(fixture.mapped());
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  fixture.table().abandon_all();
  CHECK(fixture.table().invariants_hold());
  std::uint64_t awaited = 0u;
  CHECK_STATUS(fixture.submit(&first_context, SubmissionDescriptor(1u, 0u, 32u), awaited), Status::invalid_parameter);
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_STATUS(fixture.node().submit(1u, &first_context, 1u), Status::ok);
  fixture.platform().set_irql(Irql::passive);
  fixture.complete_through(1u);
  fixture.serve();
  CHECK_EQ(fixture.node().current_fence(), 0u);   /* nothing is served on a context that is gone */
  const std::uint32_t asked = host.queue().requests();
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok);
  CHECK_EQ(host.queue().requests(), asked);
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.node().abort_pending());
  fixture.platform().set_irql(Irql::passive); }

TEST(rvk_context_table_user_of, names_the_context_an_allocation_serves)
{ Fixture fixture;
  Host host(fixture.mapped());
  CHECK(fixture.table().user_of(fixture.ring().allocation()) == nullptr);
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  CHECK(fixture.table().user_of(fixture.ring().allocation()) == &first_context);
  CHECK(fixture.table().user_of(fixture.fence().allocation()) == &first_context);
  CHECK(fixture.table().user_of(fixture.reply().allocation()) == &first_context);
  CHECK(fixture.table().user_of(nullptr) == nullptr);
  int unrelated = 0;
  CHECK(fixture.table().user_of(static_cast<const Allocation *>(static_cast<void *>(&unrelated))) == nullptr);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

TEST(rvk_context_table, runs_at_the_levels_it_declares)
{ Fixture fixture;
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.table().bind(&first_context, fixture.record(), fixture.device(),
                                         fixture.mapped().function().doorbells(), capset()));
  static_cast<void>(fixture.table().unbind(&first_context, nullptr));
  av::HostVisibleBindRecord chunk(av::HostVisibleBindRecord::Operation::unbind, 0u, 1u);
  static_cast<void>(fixture.table().bind_chunk(&first_context, chunk, fixture.device()));
  CHECK(fixture.platform().claim(Violation::irql_ceiling));
  fixture.table().abandon_all();
  static_cast<void>(fixture.table().user_of(nullptr));
  fixture.platform().set_irql(Irql::isr);
  std::uint64_t awaited = 0u;
  static_cast<void>(fixture.table().submit(&first_context, nullptr, 0u, awaited));
  fixture.table().serve(fixture.node());
  fixture.platform().set_irql(Irql::passive);
  while (fixture.platform().claim(Violation::irql_ceiling)) {}
  while (fixture.platform().claim(Violation::blocking_at_dispatch)) {}
  while (fixture.platform().claim(Violation::lock_order)) {}
  while (fixture.platform().claim(Violation::release_order)) {} }

TEST(rvk_context_table_invariants, detect_a_slot_live_unbound_or_bound_without_its_allocations)
{ Fixture fixture;
  Host host(fixture.mapped());
  RvkContextTable &table = fixture.table();
  std::uint8_t stray = 0u;
  CHECK(table.invariants_hold());
  /* Free slots: live, holding a view, or naming an allocation. */
  RvkContextTableProbe::set_live(table, 5u, true);
  CHECK(!table.invariants_hold());
  RvkContextTableProbe::set_live(table, 5u, false);
  for (const bool ring : both_views) {
      RvkContextTableProbe::swap_view(table, 6u, ring, &stray);
      CHECK(!table.invariants_hold());
      RvkContextTableProbe::swap_view(table, 6u, ring, nullptr);
  }
  RvkContextTableProbe::swap_use(table, 6u, 0u, fixture.ring().allocation());
  CHECK(!table.invariants_hold());
  RvkContextTableProbe::swap_use(table, 6u, 0u, nullptr);
  /* A slot claimed but not yet set up is bound without a binding; live, it is broken. */
  RvkContextTableProbe::set_context(table, 7u, &second_context);
  for (std::uint32_t role = 0u; role < 3u; role++)
      RvkContextTableProbe::swap_use(table, 7u, role, fixture.ring().allocation());
  CHECK(table.invariants_hold());
  RvkContextTableProbe::set_live(table, 7u, true);
  CHECK(!table.invariants_hold());
  RvkContextTableProbe::swap_view(table, 7u, true, &stray);
  RvkContextTableProbe::swap_view(table, 7u, false, &stray);
  CHECK(!table.invariants_hold());   /* mapped, live, and still no binding */
  RvkContextTableProbe::swap_view(table, 7u, true, nullptr);
  RvkContextTableProbe::swap_view(table, 7u, false, nullptr);
  RvkContextTableProbe::set_live(table, 7u, false);
  for (std::uint32_t role = 0u; role < 3u; role++) RvkContextTableProbe::swap_use(table, 7u, role, nullptr);
  RvkContextTableProbe::set_context(table, 7u, nullptr);
  /* A bound, live slot: every allocation named, both views mapped. */
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  CHECK(table.invariants_hold());
  for (std::uint32_t role = 0u; role < 3u; role++) {
      const Allocation *const was = RvkContextTableProbe::swap_use(table, 0u, role, nullptr);
      CHECK(!table.invariants_hold());
      RvkContextTableProbe::swap_use(table, 0u, role, was);
  }
  for (const bool ring : both_views) {
      std::uint8_t *const was = RvkContextTableProbe::swap_view(table, 0u, ring, nullptr);
      CHECK(!table.invariants_hold());
      RvkContextTableProbe::swap_view(table, 0u, ring, was);
  }
  CHECK(table.invariants_hold());
  /* A free slot still holding a binding. */
  RvkContextTableProbe::set_live(table, 0u, false);
  RvkContextTableProbe::set_context(table, 0u, nullptr);
  const Allocation *const ring_use = RvkContextTableProbe::swap_use(table, 0u, 0u, nullptr);
  std::uint8_t *const ring_view = RvkContextTableProbe::swap_view(table, 0u, true, nullptr);
  std::uint8_t *const fence_view = RvkContextTableProbe::swap_view(table, 0u, false, nullptr);
  CHECK(!table.invariants_hold());
  RvkContextTableProbe::set_context(table, 0u, &first_context);
  RvkContextTableProbe::swap_use(table, 0u, 0u, ring_use);
  RvkContextTableProbe::swap_view(table, 0u, true, ring_view);
  RvkContextTableProbe::swap_view(table, 0u, false, fence_view);
  CHECK_STATUS(table.unbind(&first_context, &fixture.device()), Status::ok); }

/* A handle whose data is not one of this adapter's allocations names no memory. */
TEST(rvk_context_table_bind, a_handle_to_something_that_is_not_an_allocation_is_refused)
{ Fixture fixture;
  Host host(fixture.mapped());
  int not_an_allocation = 0;
  const std::uint32_t handle = fixture.handles().insert(&not_an_allocation);
  const RvkContextRecord record(SubApi::vulkan, 12u, handle, fixture.fence().handle(), fixture.reply().handle());
  CHECK_STATUS(fixture.bind(&first_context, record), Status::invalid_parameter);
  CHECK_EQ(fixture.handles().outstanding(), 0u);
  CHECK_STATUS(fixture.table().unbind(nullptr, &fixture.device()), Status::invalid_parameter); }

/* A context its device lied to has failed, but the device still holds it: the
 * teardown is still sent. */
TEST(rvk_context_table_unbind, a_context_failed_by_its_fence_page_is_still_destroyed_on_the_device)
{ Fixture fixture;
  Host host(fixture.mapped());
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  fixture.complete_through(5u);   /* nothing was ever submitted */
  fixture.serve();
  std::uint64_t awaited = 0u;
  CHECK_STATUS(fixture.submit(&first_context, SubmissionDescriptor(1u, 0u, 32u), awaited), Status::invalid_parameter);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok);
  CHECK_EQ(host.count_of(ctx_destroy_type), 1u);
  CHECK_EQ(host.count_of(unref_type), 3u); }

/* An adapter destroyed with contexts still bound lets go of their pages. */
TEST(rvk_context_table, destruction_releases_what_is_still_bound)
{ Fixture fixture;
  Host host(fixture.mapped());
  { RvkContextTable table(fixture.platform(), fixture.mapped().interface(), fixture.allocations());
    CHECK_STATUS(table.bind(&first_context, fixture.record(), fixture.device(),
                            fixture.mapped().function().doorbells(), capset()),
                 Status::ok);
    CHECK_EQ(fixture.platform().mapped_frames(), 2u); }
  CHECK_EQ(fixture.platform().mapped_frames(), 0u); }

/* FR-006: a context the host faulted has everything it queued completed, in
 * order, so the process waiting on it wakes; its next submission is refused
 * and publishes nothing; a packet of another context between its packets is
 * still held for that context's own host. */
TEST(rvk_context_table_serve, a_fault_the_host_writes_completes_that_context_s_work_and_refuses_more)
{ Fixture fixture;
  Host host(fixture.mapped());
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  std::uint64_t awaited = 0u;
  CHECK_STATUS(fixture.submit(&first_context, SubmissionDescriptor(1u, 0u, 32u), awaited), Status::ok);
  CHECK_STATUS(fixture.submit(&first_context, SubmissionDescriptor(2u, 32u, 64u), awaited), Status::ok);
  CHECK_STATUS(fixture.submit(&first_context, SubmissionDescriptor(3u, 64u, 96u), awaited), Status::ok);
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_STATUS(fixture.node().submit(10u, &first_context, 1u), Status::ok);
  CHECK_STATUS(fixture.node().submit(11u, &first_context, 2u), Status::ok);
  CHECK_STATUS(fixture.node().submit(12u, &second_context, 1u), Status::ok);
  CHECK_STATUS(fixture.node().submit(13u, &first_context, 3u), Status::ok);
  fixture.platform().set_irql(Irql::passive);
  const std::uint32_t rings = fixture.mapped().function().doorbells().rings(0u);
  fixture.complete_through(1u, 6u);   /* the first completed, then the context faulted with code 6 */
  fixture.serve();
  CHECK(fixture.table().invariants_hold());
  CHECK_EQ(fixture.node().current_fence(), 11u);
  CHECK_EQ(fixture.node().pending(), 2u);   /* 12 waits for its own host, and holds 13 */
  CHECK_STATUS(fixture.submit(&first_context, SubmissionDescriptor(4u, 96u, 128u), awaited), Status::invalid_parameter);
  CHECK_EQ(awaited, 0u);
  CHECK_EQ(fixture.mapped().function().doorbells().rings(0u), rings);
  fixture.platform().set_irql(Irql::dispatch);
  fixture.node().finish(&second_context, 1u);
  fixture.platform().set_irql(Irql::passive);
  fixture.serve();
  CHECK_EQ(fixture.node().current_fence(), 13u);
  CHECK_EQ(fixture.node().pending(), 0u);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

/* A packet whose batch was published before the fault was seen reaches the
 * node after the serve that saw it; the next serve finishes it too. */
TEST(rvk_context_table_serve, a_packet_queued_after_the_fault_was_seen_is_finished_by_the_next_serve)
{ Fixture fixture;
  Host host(fixture.mapped());
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  std::uint64_t awaited = 0u;
  CHECK_STATUS(fixture.submit(&first_context, SubmissionDescriptor(1u, 0u, 32u), awaited), Status::ok);
  fixture.complete_through(0u, 6u);
  fixture.serve();
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_STATUS(fixture.node().submit(20u, &first_context, awaited), Status::ok);
  fixture.platform().set_irql(Irql::passive);
  fixture.serve();
  CHECK_EQ(fixture.node().current_fence(), 20u);
  CHECK_EQ(fixture.node().pending(), 0u);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

/* A page completing a fence never submitted fails the context: it is lost the
 * same way, its work completed and its next submission refused. */
TEST(rvk_context_table_serve, a_page_no_device_could_write_loses_the_context_too)
{ Fixture fixture;
  Host host(fixture.mapped());
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  std::uint64_t awaited = 0u;
  CHECK_STATUS(fixture.submit(&first_context, SubmissionDescriptor(1u, 0u, 32u), awaited), Status::ok);
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_STATUS(fixture.node().submit(3u, &first_context, 1u), Status::ok);
  fixture.platform().set_irql(Irql::passive);
  fixture.complete_through(9u);   /* completes a fence never submitted */
  fixture.serve();
  CHECK_EQ(fixture.node().pending(), 0u);
  CHECK_EQ(fixture.node().current_fence(), 3u);
  CHECK_STATUS(fixture.submit(&first_context, SubmissionDescriptor(2u, 32u, 64u), awaited), Status::invalid_parameter);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

/* Feature 045: a process binds a host-visible chunk - guest RAM the host GPU
 * imports - to its live rvk context by escape. The chunk becomes a GUEST blob of
 * the context's own resource range, past its three transport resources. */
namespace {

using av::HostVisibleBindRecord;
using BindOperation = av::HostVisibleBindRecord::Operation;

constexpr std::uint32_t chunk_base = av::RvkContextBinding::first_resource_id + 3u;

Status bind_chunk(Fixture &fixture, void *context, std::uint32_t allocation, HostVisibleBindRecord &answer)
{
    answer = HostVisibleBindRecord(BindOperation::bind, allocation, 0u);
    return fixture.table().bind_chunk(context, answer, fixture.device());
}

Status unbind_chunk(Fixture &fixture, void *context, std::uint32_t resource)
{
    HostVisibleBindRecord record(BindOperation::unbind, 0u, resource);
    return fixture.table().bind_chunk(context, record, fixture.device());
}

}  // namespace

TEST(rvk_context_table_bind_chunk, a_mapped_chunk_becomes_a_blob_of_the_context_s_range)
{ Fixture fixture;
  Host host(fixture.mapped());
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  ProcessAllocation chunk(fixture.platform(), fixture.allocations(), fixture.handles(),
                          AllocationKind::host_visible_memory, 4u * page);
  chunk.map_into_aperture(40u);
  HostVisibleBindRecord answer;
  CHECK_STATUS(bind_chunk(fixture, &first_context, chunk.handle(), answer), Status::ok);
  CHECK_EQ(answer.resource_id(), chunk_base);
  CHECK_EQ(answer.pages(), 4u);
  CHECK(fixture.table().invariants_hold());
  /* The fourth blob: the chunk's pages, as one entry here since they are contiguous. */
  CHECK_EQ(host.count_of(create_blob_type), 4u);
  CHECK_EQ(host.field(create_blob_type, 3u, 24u), chunk_base);
  CHECK_EQ(host.field(create_blob_type, 3u, 36u), 1u);
  CHECK_EQ(host.field(create_blob_type, 3u, 56u),
           static_cast<std::uint32_t>(fixture.platform().physical_address(chunk.bytes())));
  CHECK(fixture.table().user_of(chunk.allocation()) == &first_context);
  CHECK_EQ(fixture.handles().outstanding(), 0u);

  /* A second chunk takes the next slot; unbinding the first frees it for the next bind. */
  ProcessAllocation second(fixture.platform(), fixture.allocations(), fixture.handles(),
                           AllocationKind::host_visible_memory, page);
  second.map_into_aperture(50u);
  HostVisibleBindRecord second_answer;
  CHECK_STATUS(bind_chunk(fixture, &first_context, second.handle(), second_answer), Status::ok);
  CHECK_EQ(second_answer.resource_id(), chunk_base + 1u);
  /* The second is found past the first, which is still bound; then the first. */
  CHECK_STATUS(unbind_chunk(fixture, &first_context, chunk_base + 1u), Status::ok);
  CHECK_EQ(host.field(unref_type, 0u, 24u), chunk_base + 1u);
  CHECK_STATUS(unbind_chunk(fixture, &first_context, chunk_base), Status::ok);
  CHECK_EQ(host.count_of(unref_type), 2u);
  CHECK_EQ(host.field(unref_type, 1u, 24u), chunk_base);
  CHECK(fixture.table().user_of(chunk.allocation()) == nullptr);
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(bind_chunk(fixture, &first_context, chunk.handle(), answer), Status::ok);
  CHECK_EQ(answer.resource_id(), chunk_base);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

/* Each refusal sends the device nothing and binds nothing. */
TEST(rvk_context_table_bind_chunk, refuses_what_is_not_the_process_s_mapped_chunk_on_a_live_context)
{ Fixture fixture;
  Host host(fixture.mapped());
  ProcessAllocation chunk(fixture.platform(), fixture.allocations(), fixture.handles(),
                          AllocationKind::host_visible_memory, 2u * page);
  ProcessAllocation unmapped(fixture.platform(), fixture.allocations(), fixture.handles(),
                             AllocationKind::host_visible_memory, page);
  chunk.map_into_aperture(40u);
  HostVisibleBindRecord answer;
  /* No context bound yet. */
  CHECK_STATUS(bind_chunk(fixture, &first_context, chunk.handle(), answer), Status::invalid_parameter);
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  const std::uint32_t asked = host.queue().requests();
  CHECK_STATUS(bind_chunk(fixture, &second_context, chunk.handle(), answer), Status::invalid_parameter);
  CHECK_STATUS(bind_chunk(fixture, &first_context, fixture.ring().handle(), answer), Status::invalid_parameter);
  CHECK_STATUS(bind_chunk(fixture, &first_context, unmapped.handle(), answer), Status::invalid_parameter);
  CHECK_STATUS(bind_chunk(fixture, &first_context, 0x7777u, answer), Status::invalid_parameter);
  CHECK_STATUS(unbind_chunk(fixture, &first_context, chunk_base), Status::invalid_parameter);
  CHECK_STATUS(unbind_chunk(fixture, &first_context, chunk_base + 5u), Status::invalid_parameter);
  CHECK_EQ(host.queue().requests(), asked);
  CHECK_EQ(answer.resource_id(), 0u);
  /* The same chunk twice. */
  CHECK_STATUS(bind_chunk(fixture, &first_context, chunk.handle(), answer), Status::ok);
  const std::uint32_t after = host.queue().requests();
  HostVisibleBindRecord again;
  CHECK_STATUS(bind_chunk(fixture, &first_context, chunk.handle(), again), Status::invalid_parameter);
  CHECK_EQ(host.queue().requests(), after);
  CHECK_EQ(fixture.handles().outstanding(), 0u);
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

/* A device refusing the blob leaves the slot free and the chunk unbound. */
TEST(rvk_context_table_bind_chunk, a_blob_the_device_refuses_binds_nothing)
{ Fixture fixture;
  Host host(fixture.mapped());
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  ProcessAllocation chunk(fixture.platform(), fixture.allocations(), fixture.handles(),
                          AllocationKind::host_visible_memory, page);
  chunk.map_into_aperture(40u);
  host.refuse_at(static_cast<int>(host.queue().requests()));
  HostVisibleBindRecord answer;
  CHECK_STATUS(bind_chunk(fixture, &first_context, chunk.handle(), answer), Status::not_supported);
  CHECK(fixture.table().user_of(chunk.allocation()) == nullptr);
  CHECK_STATUS(bind_chunk(fixture, &first_context, chunk.handle(), answer), Status::ok);
  CHECK_EQ(answer.resource_id(), chunk_base);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

/* Every chunk slot taken refuses the next chunk without asking the device. */
TEST(rvk_context_table_bind_chunk, the_context_s_chunk_slots_full_refuses_the_next)
{ Fixture fixture;
  Host host(fixture.mapped());
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  std::optional<ProcessAllocation> chunks[av::RvkContextBinding::chunk_slots + 1u];
  for (std::uint32_t index = 0u; index <= av::RvkContextBinding::chunk_slots; index++) {
      chunks[index].emplace(fixture.platform(), fixture.allocations(), fixture.handles(),
                            AllocationKind::host_visible_memory, page);
      chunks[index]->map_into_aperture(100u + index);
  }
  HostVisibleBindRecord answer;
  for (std::uint32_t index = 0u; index < av::RvkContextBinding::chunk_slots; index++) {
      CHECK_STATUS(bind_chunk(fixture, &first_context, chunks[index]->handle(), answer), Status::ok);
      CHECK_EQ(answer.resource_id(), chunk_base + index);
  }
  const std::uint32_t asked = host.queue().requests();
  CHECK_STATUS(bind_chunk(fixture, &first_context, chunks[av::RvkContextBinding::chunk_slots]->handle(), answer),
               Status::no_space);
  CHECK_EQ(host.queue().requests(), asked);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok);
  /* Teardown released every chunk (the fake device logs only the first 64
   * requests, so what is judged is that none is still named). */
  for (std::uint32_t index = 0u; index <= av::RvkContextBinding::chunk_slots; index++)
      CHECK(fixture.table().user_of(chunks[index]->allocation()) == nullptr);
  CHECK(fixture.table().invariants_hold()); }

/* A context torn down is destroyed on the device before its chunks are
 * released: the device refuses to release a chunk it still holds imported, and
 * destroying the context ends its imports. A second context's chunks take its
 * own range. */
TEST(rvk_context_table_bind_chunk, teardown_destroys_the_context_before_its_chunks_and_ranges_are_per_context)
{ Fixture fixture;
  Host host(fixture.mapped());
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  ProcessAllocation chunk(fixture.platform(), fixture.allocations(), fixture.handles(),
                          AllocationKind::host_visible_memory, page);
  chunk.map_into_aperture(40u);
  HostVisibleBindRecord answer;
  CHECK_STATUS(bind_chunk(fixture, &first_context, chunk.handle(), answer), Status::ok);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok);
  /* CTX_DESTROY, then the three transport unrefs, then the chunk's. */
  std::uint32_t chunk_unref = 0u;
  std::uint32_t destroy = 0u;
  for (std::uint32_t i = 0u; i < host.queue().requests(); i++) {
      if (host.queue().request_type(i) == unref_type) chunk_unref = i;
      if (host.queue().request_type(i) == ctx_destroy_type) destroy = i;
  }
  CHECK(destroy < chunk_unref);
  CHECK_EQ(host.field(unref_type, 3u, 24u), chunk_base);
  CHECK(fixture.table().user_of(chunk.allocation()) == nullptr);
  CHECK(fixture.table().invariants_hold());

  /* Slot 1 of the adapter: its chunks start past its own transport resources. */
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  ProcessAllocation ring(fixture.platform(), fixture.allocations(), fixture.handles(), AllocationKind::context_ring,
                         2u * page);
  ProcessAllocation fences(fixture.platform(), fixture.allocations(), fixture.handles(), AllocationKind::fence_page,
                           page);
  ProcessAllocation reply(fixture.platform(), fixture.allocations(), fixture.handles(), AllocationKind::reply_slot,
                          page);
  ring.map_into_aperture(60u);
  fences.map_into_aperture(70u);
  reply.map_into_aperture(80u);
  CHECK_STATUS(fixture.bind(&second_context, RvkContextRecord(SubApi::vulkan, 12u, ring.handle(), fences.handle(),
                                                              reply.handle())),
               Status::ok);
  CHECK_STATUS(bind_chunk(fixture, &second_context, chunk.handle(), answer), Status::ok);
  CHECK_EQ(answer.resource_id(),
           av::RvkContextBinding::first_resource_id + av::RvkContextBinding::resources_per_context + 3u);
  CHECK_STATUS(fixture.table().unbind(&second_context, &fixture.device()), Status::ok);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

/* Abandoned by a reset, a context's chunks are let go of without a request. */
TEST(rvk_context_table_bind_chunk, an_abandoned_context_releases_its_chunks_silently)
{ Fixture fixture;
  Host host(fixture.mapped());
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  ProcessAllocation chunk(fixture.platform(), fixture.allocations(), fixture.handles(),
                          AllocationKind::host_visible_memory, page);
  chunk.map_into_aperture(40u);
  HostVisibleBindRecord answer;
  CHECK_STATUS(bind_chunk(fixture, &first_context, chunk.handle(), answer), Status::ok);
  fixture.table().abandon_all();
  CHECK_STATUS(bind_chunk(fixture, &first_context, chunk.handle(), answer), Status::invalid_parameter);
  const std::uint32_t asked = host.queue().requests();
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok);
  CHECK_EQ(host.queue().requests(), asked);
  CHECK(fixture.table().user_of(chunk.allocation()) == nullptr);
  CHECK(fixture.table().invariants_hold()); }

/* A context the host faulted takes no chunk: it will never import one. */
TEST(rvk_context_table_bind_chunk, a_context_the_host_faulted_is_refused)
{ Fixture fixture;
  Host host(fixture.mapped());
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  ProcessAllocation chunk(fixture.platform(), fixture.allocations(), fixture.handles(),
                          AllocationKind::host_visible_memory, page);
  chunk.map_into_aperture(40u);
  fixture.complete_through(0u, 6u);
  fixture.serve();
  HostVisibleBindRecord answer;
  const std::uint32_t asked = host.queue().requests();
  CHECK_STATUS(bind_chunk(fixture, &first_context, chunk.handle(), answer), Status::invalid_parameter);
  CHECK_EQ(host.queue().requests(), asked);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

/* A chunk slot naming an allocation with no resource, or a chunk on a slot no
 * context holds, is a table that has lost track of what the device holds. */
TEST(rvk_context_table_invariants, detect_a_broken_chunk_or_one_on_a_free_slot)
{ Fixture fixture;
  Host host(fixture.mapped());
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  av::HostVisibleBinding &held = RvkContextTableProbe::chunk(fixture.table(), 0u, 3u);
  held.take(*fixture.ring().allocation(), 0u);
  CHECK(!fixture.table().invariants_hold());
  held.clear();
  CHECK(fixture.table().invariants_hold());
  av::HostVisibleBinding &stray = RvkContextTableProbe::chunk(fixture.table(), 5u, 0u);
  stray.take(*fixture.ring().allocation(), 7u);
  CHECK(!fixture.table().invariants_hold());
  stray.clear();
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }


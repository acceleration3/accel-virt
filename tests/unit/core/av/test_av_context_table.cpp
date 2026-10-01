/* The adapter's av contexts, over the pieces a started adapter has: a mapped
 * virtio-gpu-av-pci with its doorbell pages, dxgkrnl's handle table, the
 * allocation table with the page a process names its context by, and a device
 * the test answers. A record binds the context, on a ring, fence page and reply
 * slot the driver allocates and maps into the process (docs/protocol.md,
 * "Context memory"); a submission rings its doorbell; the DPC's serve finishes on
 * the node only what the fence page says the host completed. */
#include "core/ddi/dxgk_allocation_info_record.hpp"
#include "core/ddi/dxgk_build_paging_buffer_map_aperture_segment_record.hpp"
#include "core/ddi/dxgk_build_paging_buffer_unmap_aperture_segment_record.hpp"
#include "core/ddi/dxgkarg_create_allocation_record.hpp"
#include "core/memory/allocation.hpp"
#include "core/memory/allocation_table.hpp"
#include "core/memory/shared_object_table.hpp"
#include "core/memory/aperture_segment.hpp"
#include "core/memory/host_window.hpp"
#include "core/memory/memory_segments.hpp"
#include "core/miniport/interrupt_notification.hpp"
#include "core/av/av_context_table.hpp"
#include "core/scheduling/hw_queue_submissions.hpp"
#include "core/scheduling/execution_node.hpp"
#include "core/support/status.hpp"
#include "core/virtio/virtio_gpu_device.hpp"
#include "harness/device/fake_register_bytes.hpp"
#include "harness/device/fake_virtqueue_device.hpp"
#include "harness/device/mapped_virtio_function.hpp"
#include "harness/dxgkrnl/fake_handle_table.hpp"
#include "core/trace/state_writer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/violation.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/gl_context_options.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/context/av_context_record.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/context/context_views_record.hpp"
#include "proto/resources/host_window_record.hpp"
#include "proto/submission/fence_page.hpp"
#include "proto/submission/ring_header.hpp"
#include "proto/submission/submission_descriptor.hpp"
#include "proto/virtio_gpu/gpu_response.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <optional>

namespace av {

struct AvContextTableProbe {
    static void set_live(AvContextTable &table, std::uint32_t slot, bool live) { table.live_[slot] = live; }
    static void set_context(AvContextTable &table, std::uint32_t slot, void *context) { table.contexts_[slot] = context; }
    static ContextChunk &chunk(AvContextTable &table, std::uint32_t slot, std::uint32_t index)
    {
        return table.chunks_[slot][index];
    }
    static const Allocation *swap_name(AvContextTable &table, std::uint32_t slot, const Allocation *name)
    {
        const Allocation *const was = table.names_[slot];
        table.names_[slot] = name;
        return was;
    }
    static ObjectStorage<ContextMemory> &memory(AvContextTable &table, std::uint32_t slot)
    {
        return table.memories_[slot];
    }
};

/* The one part of a context's memory a test breaks on its own. */
struct ContextMemoryProbe {
    static GuestPages &fence(ContextMemory &memory) { return memory.fence_; }
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
using av::AvContextRecord;
using av::AvContextTable;
using av::AvContextTableProbe;
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
constexpr std::uint32_t map_blob_type = 0x0208u;
constexpr std::uint32_t unmap_blob_type = 0x0209u;
constexpr std::uint8_t  uuid[av::GpuIdentity::uuid_bytes] = {};

CapsetInfo capset()
{
    return CapsetInfo(av::ProtocolVersion(1u, 0u), 0x7u, 64u, 12u, 20u, 0u,
                      av::GpuIdentity(0u, 0u, 0u, uuid, 0u, 0u, "gpu"));
}

/* A device answering every control request from the stall between the
 * driver's polls - MAP_BLOB with the caching it offers, everything else with
 * OK_NODATA - or refusing one at a chosen index. */
class Host {
public:
    explicit Host(av::MappedVirtioFunction &mapped) : queue_(mapped.queue_device(0u)), platform_(&mapped.platform())
    { mapped.platform().on_stall(&Host::stall, this); }

    void                 refuse_at(int index) { refuse_at_ = index; }
    /* Refuses the next request of `type`. */
    void                 refuse_next(std::uint32_t type) { refuse_type_ = type; }
    void                 answer_map_with(std::uint32_t map_info) { map_info_ = map_info; }
    /* What the window and the process looked like when the device was asked
     * to unmap or release a blob: the order a teardown must keep. */
    void                 watch(const av::HostWindow &window) { window_ = &window; }
    std::uint32_t        pages_when_released() const { return pages_when_released_; }
    std::uint32_t        views_when_unmapped() const { return views_when_unmapped_; }
    std::uint32_t        answered() const { return answered_; }
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
        const std::uint32_t asked = host->queue_.pending_type();
        if (asked == unref_type && host->window_ != nullptr) host->pages_when_released_ = host->window_->pages_in_use();
        if (asked == unmap_blob_type) host->views_when_unmapped_ = host->platform_->process_mappings();
        GpuResponse answer;
        if (asked == map_blob_type)
            answer.set_map_info(host->map_info_);
        else
            answer.set(GpuResponse::ok_nodata);
        if (static_cast<int>(host->answered_) == host->refuse_at_ || asked == host->refuse_type_) {
            answer.set(GpuResponse::err_invalid_parameter);
            host->refuse_type_ = asked == host->refuse_type_ ? 0u : host->refuse_type_;
        }
        host->answered_++;
        static_cast<void>(host->queue_.respond_all(answer.bytes(), static_cast<std::uint32_t>(answer.length())));
    }

    av::FakeVirtqueueDevice queue_;
    av::MockPlatform       *platform_;
    const av::HostWindow   *window_ = nullptr;
    std::uint32_t           answered_ = 0u;
    int                     refuse_at_ = -1;
    std::uint32_t           refuse_type_ = 0u;
    std::uint32_t           map_info_ = GpuResponse::map_cache_cached;
    std::uint32_t           pages_when_released_ = UINT32_MAX;
    std::uint32_t           views_when_unmapped_ = UINT32_MAX;
};

/* One allocation of a process: created in the table, given a D3DKMT handle,
 * and - as VidMm would make it resident - mappable into the aperture over guest
 * memory the platform can map back. */
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
        : aperture_(mapped_.platform()), segments_(mapped_.platform(), aperture_),
          shared_(mapped_.platform()), allocations_(mapped_.platform(), aperture_, segments_, shared_),
          window_(mapped_.platform()), table_(mapped_.platform(), mapped_.interface(), allocations_, window_),
          notification_(mapped_.platform(), mapped_.interface()), node_(mapped_.platform()),
          queues_(mapped_.platform())
    {
        mapped_.function().add_doorbell_region();
        CHECK_STATUS(mapped_.map(), Status::ok);
        CHECK_STATUS(aperture_.initialize(std::int64_t{1} << 30u), Status::ok);
        device_.emplace(mapped_.platform(), mapped_.mappings(), mapped_.layout());
        CHECK_STATUS(device_->start(6u), Status::ok);
        name_.emplace(mapped_.platform(), allocations_, handles_, AllocationKind::context_ring, page);
    }
    ~Fixture()
    {
        mapped_.platform().set_irql(Irql::passive);
        mapped_.platform().on_stall(nullptr, nullptr);
    }

    AvContextRecord record(std::uint8_t order = 12u) const
    {
        return AvContextRecord(SubApi::vulkan, order, name_->handle(), page);
    }
    Status bind(void *context) { return bind(context, record()); }
    Status bind(void *context, const AvContextRecord &asked)
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
    /* The memory the driver gave the context in `slot`. */
    av::ContextMemory &memory(std::uint32_t slot = 0u)
    {
        return AvContextTableProbe::memory(table_, slot).object();
    }
    /* Its ring header and fence page, as the host reaches them: the rig's
     * pages lie in one block, so a page's view reaches the rest of it too. */
    std::uint8_t *ring_memory(std::uint32_t slot = 0u) { return memory(slot).pages().ring_view(); }
    std::uint8_t *fence_page(std::uint32_t slot = 0u)
    {
        return const_cast<std::uint8_t *>(memory(slot).pages().fence_view());
    }
    /* What the host writes when it finishes the batches through `completed`. */
    void complete_through(std::uint64_t completed, std::uint32_t fault = 0u)
    {
        const FencePage written = fault == 0u ? FencePage(completed, completed) : FencePage(completed, completed, fault);
        static_cast<void>(written.encode(fence_page(), 32u));
    }
    void serve()
    {
        mapped_.platform().set_irql(Irql::dispatch);
        table_.serve(node_, queues_);
        CHECK_STATUS(node_.complete(notification_), Status::ok);
        mapped_.platform().set_irql(Irql::passive);
    }

    av::MockPlatform         &platform() { return mapped_.platform(); }
    av::MappedVirtioFunction &mapped() { return mapped_; }
    av::VirtioGpuDevice      &device() { return *device_; }
    av::FakeHandleTable      &handles() { return handles_; }
    av::AllocationTable      &allocations() { return allocations_; }
    av::HostWindow           &window() { return window_; }
    AvContextTable          &table() { return table_; }
    ExecutionNode            &node() { return node_; }
    av::HwQueueSubmissions   &queues() { return queues_; }
    /* The page the process names its context by. */
    ProcessAllocation        &ring() { return *name_; }

private:
    av::MappedVirtioFunction                mapped_;
    av::FakeHandleTable                     handles_;
    av::ApertureSegment                     aperture_;
    av::MemorySegments                      segments_;
    av::SharedObjectTable shared_;
    av::AllocationTable                     allocations_;
    av::HostWindow                          window_;
    AvContextTable                         table_;
    av::InterruptNotification               notification_;
    ExecutionNode                           node_;
    /* Feature 049: what a hardware queue of a context waits on is the same
     * completion its packets do, so a serve answers both. */
    av::HwQueueSubmissions                  queues_;
    std::optional<av::VirtioGpuDevice>      device_;
    std::optional<ProcessAllocation>        name_;
};

int first_context = 0;
int second_context = 0;

std::uint8_t *at(std::uint64_t view)
{
    return reinterpret_cast<std::uint8_t *>(static_cast<std::uintptr_t>(view));
}

}  // namespace

TEST(av_context_table_bind, sets_the_host_context_up_on_memory_the_driver_owns)
{ Fixture fixture;
  Host host(fixture.mapped());
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  CHECK(fixture.table().invariants_hold());
  CHECK_EQ(fixture.table().bound(), 1u);
  /* The slot is the doorbell page, which is what a doorbell of this context's
   * hardware queue names (feature 049). A context nobody bound has none. */
  CHECK_EQ(fixture.table().doorbell_of(&first_context), 0u);
  CHECK_EQ(fixture.table().doorbell_of(&second_context), av::AvContextTable::max_contexts);
  CHECK_EQ(host.count_of(ctx_create_type), 1u);
  CHECK_EQ(host.count_of(create_blob_type), 3u);
  CHECK_EQ(host.count_of(submit_3d_type), 1u);
  /* Slot 0: host context 1, blobs from the driver's first id, whose one memory
   * entry is the ring's first page - the driver's own pages, three blocks of
   * them: the ring, the fence page and the reply slot. */
  const av::ContextPages pages = fixture.memory().pages();
  CHECK_EQ(fixture.platform().owned_page_blocks(), 3u);
  CHECK_EQ(host.field(ctx_create_type, 0u, 16u), 1u);
  CHECK_EQ(host.field(create_blob_type, 0u, 24u), av::AvContextBinding::first_resource_id);
  CHECK_EQ(host.field(create_blob_type, 0u, 36u), 1u);   /* one entry: the ring is contiguous here */
  CHECK_EQ(host.field(create_blob_type, 0u, 56u), static_cast<std::uint32_t>(pages.ring().frame(0u)));
  CHECK_EQ(host.field(create_blob_type, 1u, 56u), static_cast<std::uint32_t>(pages.fence().frame(0u)));
  CHECK_EQ(host.field(create_blob_type, 2u, 56u), static_cast<std::uint32_t>(pages.reply().frame(0u)));
  /* The ring the device reads at setup already has its header. */
  av::RingHeader header;
  CHECK_STATUS(header.decode(fixture.ring_memory(), av::RingHeader::encoded_size), Status::ok);
  CHECK_EQ(header.order(), 12u);
  /* The doorbell's fence queue got the first message past the sources. */
  CHECK_EQ(fixture.mapped().common().queue(2u).vector, 3u);
  /* The driver maps the ring header and the fence page for itself, and the
   * ring and reply slot into the process, and nothing else. */
  CHECK_EQ(fixture.platform().mapped_frames(), 2u);
  CHECK_EQ(fixture.platform().process_mappings(), 2u);
  CHECK(fixture.platform().maps_into_process(fixture.memory().ring_view(), pages.ring().frame(0u), 2u * page));
  CHECK(fixture.platform().maps_into_process(fixture.memory().reply_view(), pages.reply().frame(0u), page));
  CHECK(fixture.platform().logged_line("av context on doorbell 0 message 3 status 0x00000000"));
  CHECK_EQ(fixture.handles().outstanding(), 0u);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok);
  CHECK_EQ(fixture.platform().mapped_frames(), 0u);
  CHECK_EQ(fixture.platform().process_mappings(), 0u);
  CHECK_EQ(fixture.platform().owned_page_blocks(), 0u); }

/* The process asks where its ring and reply slot are, and is told the views
 * the driver made of them; a context not live has none. */
TEST(av_context_table_views, answers_the_process_where_its_ring_and_reply_slot_are)
{ Fixture fixture;
  Host host(fixture.mapped());
  av::ContextViewsRecord answer;
  CHECK_STATUS(fixture.table().views(&first_context, answer), Status::invalid_parameter);
  CHECK_EQ(answer.ring(), 0u);
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  CHECK_STATUS(fixture.table().views(&first_context, answer), Status::ok);
  CHECK_EQ(answer.ring(), fixture.memory().ring_view());
  CHECK_EQ(answer.reply(), fixture.memory().reply_view());
  /* What the process writes through its ring is the ring the device reads. */
  at(answer.ring())[av::RingHeader::encoded_size] = 0x5Au;
  CHECK_EQ(fixture.ring_memory()[av::RingHeader::encoded_size], 0x5Au);
  CHECK_STATUS(fixture.table().views(&second_context, answer), Status::invalid_parameter);
  fixture.table().abandon_all();
  av::ContextViewsRecord lost;
  CHECK_STATUS(fixture.table().views(&first_context, lost), Status::invalid_parameter);
  CHECK_EQ(lost.ring(), 0u);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

/* A submission the process describes lands in its ring and rings its doorbell,
 * and the node completes it only when the host's fence page says so. */
TEST(av_context_table_submit, rings_the_doorbell_and_the_node_waits_for_the_host)
{ Fixture fixture;
  Host host(fixture.mapped());
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  std::uint64_t awaited = 99u;
  CHECK_STATUS(fixture.submit(&first_context, SubmissionDescriptor(1u, 0u, 32u), awaited), Status::ok);
  CHECK_EQ(awaited, 1u);
  CHECK_EQ(fixture.mapped().function().doorbells().rings(0u), 1u);
  std::uint64_t tail = 0u;
  std::memcpy(&tail, fixture.ring_memory() + 8u, sizeof tail);
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
TEST(av_context_table_submit, refuses_what_is_not_a_descriptor_of_a_live_context_s_ring)
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

TEST(av_context_table_bind, refuses_a_record_that_does_not_name_the_context_by_a_ring_of_the_process)
{ Fixture fixture;
  Host host(fixture.mapped());
  ProcessAllocation client(fixture.platform(), fixture.allocations(), fixture.handles(), AllocationKind::client,
                           page);
  const AvContextRecord cases[] = {
      AvContextRecord(SubApi::vulkan, 12u, 0x7777u, page),           /* no such handle */
      AvContextRecord(SubApi::vulkan, 12u, client.handle(), page),   /* not a ring */
  };
  for (const AvContextRecord &asked : cases) {
      CHECK_STATUS(fixture.bind(&first_context, asked), Status::invalid_parameter);
      CHECK_EQ(fixture.table().bound(), 0u);
      CHECK_EQ(fixture.handles().outstanding(), 0u);
      CHECK_EQ(fixture.platform().owned_page_blocks(), 0u);
      CHECK(fixture.table().invariants_hold());
  }
  CHECK_EQ(host.queue().requests(), 0u);
  CHECK_STATUS(fixture.bind(nullptr), Status::invalid_state);
  CHECK_EQ(fixture.platform().mapped_frames(), 0u); }

/* The name is only a name: VidMm never mapping it, or evicting it, moves
 * nothing the device holds, and a ring of any order the capset allows binds. */
TEST(av_context_table_bind, a_name_vidmm_never_mapped_or_evicted_still_binds)
{ Fixture fixture;
  Host host(fixture.mapped());
  CHECK_STATUS(fixture.bind(&first_context, fixture.record(20u)), Status::ok);
  CHECK_EQ(fixture.memory().pages().ring().count(), 257u);   /* 32 + 1 MiB, in pages */
  fixture.ring().map_into_aperture(10u);
  fixture.ring().evict(10u);
  std::uint64_t awaited = 0u;
  CHECK_STATUS(fixture.submit(&first_context, SubmissionDescriptor(1u, 0u, 32u), awaited), Status::ok);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

TEST(av_context_table_bind, a_context_bound_twice_or_memory_already_in_use_is_refused)
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
TEST(av_context_table_bind, every_doorbell_taken_refuses_the_next_context)
{ Fixture fixture;
  Host host(fixture.mapped());
  static int contexts[AvContextTable::max_contexts + 1u];
  std::optional<ProcessAllocation> names[AvContextTable::max_contexts + 1u];
  for (std::uint32_t index = 0u; index <= AvContextTable::max_contexts; index++)
      names[index].emplace(fixture.platform(), fixture.allocations(), fixture.handles(), AllocationKind::context_ring,
                           page);
  const auto record_of = [&](std::uint32_t index) {
      return AvContextRecord(SubApi::vulkan, 12u, names[index]->handle(), page);
  };
  for (std::uint32_t index = 0u; index < AvContextTable::max_contexts; index++)
      CHECK_STATUS(fixture.bind(&contexts[index], record_of(index)), Status::ok);
  CHECK_EQ(fixture.table().bound(), AvContextTable::max_contexts);
  const std::uint32_t last = AvContextTable::max_contexts;
  CHECK_STATUS(fixture.bind(&contexts[last], record_of(last)), Status::no_space);
  CHECK_STATUS(fixture.table().unbind(&contexts[7], &fixture.device()), Status::ok);
  CHECK_STATUS(fixture.bind(&contexts[last], record_of(last)), Status::ok);
  CHECK(fixture.platform().logged_line("av context on doorbell 7 "));
  CHECK(fixture.table().invariants_hold());
  for (std::uint32_t index = 0u; index <= last; index++)
      if (index != 7u) CHECK_STATUS(fixture.table().unbind(&contexts[index], &fixture.device()), Status::ok);
  CHECK_EQ(fixture.platform().mapped_frames(), 0u);
  CHECK_EQ(fixture.platform().owned_page_blocks(), 0u); }

/* No memory for the ring, fence page or reply slot, no view of one for the
 * kernel or the process, or a refused vector: nothing bound, nothing held,
 * nothing asked of the device. */
TEST(av_context_table_bind, memory_or_a_view_that_cannot_be_had_or_a_refused_vector_binds_nothing)
{ Fixture fixture;
  Host host(fixture.mapped());
  const auto nothing_held = [&fixture, &host] {
      CHECK_EQ(fixture.platform().mapped_frames(), 0u);
      CHECK_EQ(fixture.platform().process_mappings(), 0u);
      CHECK_EQ(fixture.platform().owned_page_blocks(), 0u);
      CHECK_EQ(fixture.table().bound(), 0u);
      CHECK_EQ(host.queue().requests(), 0u);
      CHECK(fixture.table().invariants_hold());
  };
  for (std::uint32_t nth = 1u; nth <= 3u; nth++) {
      fixture.platform().fail_page_allocation_at(nth);
      CHECK_STATUS(fixture.bind(&first_context), Status::no_memory);
      nothing_held();
  }
  for (std::uint32_t nth = 1u; nth <= 2u; nth++) {
      fixture.platform().fail_frame_map_at(nth);
      CHECK_STATUS(fixture.bind(&first_context), Status::no_memory);
      nothing_held();
  }
  for (std::uint32_t nth = 1u; nth <= 2u; nth++) {
      fixture.platform().fail_process_map_at(nth);
      CHECK_STATUS(fixture.bind(&first_context), Status::no_memory);
      nothing_held();
  }
  fixture.platform().fail_frame_map_at(0u);
  fixture.mapped().common().refuse_vectors();
  CHECK_STATUS(fixture.bind(&first_context), Status::no_space);
  CHECK_EQ(fixture.platform().mapped_frames(), 0u);
  CHECK_EQ(host.queue().requests(), 0u);
  CHECK(fixture.table().invariants_hold()); }

/* A setup the device refuses releases what it created and leaves nothing bound;
 * the same record binds once the device agrees. */
TEST(av_context_table_bind, a_setup_the_device_refuses_is_undone)
{ Fixture fixture;
  Host host(fixture.mapped());
  host.refuse_at(2);   /* the second blob */
  CHECK_STATUS(fixture.bind(&first_context), Status::not_supported);
  CHECK_EQ(fixture.table().bound(), 0u);
  CHECK_EQ(host.count_of(ctx_destroy_type), 1u);
  CHECK_EQ(host.count_of(unref_type), 1u);
  CHECK_EQ(fixture.platform().mapped_frames(), 0u);
  CHECK_EQ(fixture.platform().process_mappings(), 0u);
  CHECK_EQ(fixture.platform().owned_page_blocks(), 0u);
  CHECK(fixture.platform().logged_line("status 0xC00000BB"));
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

TEST(av_context_table_bind, a_record_the_capset_forbids_is_refused_before_the_device_is_asked)
{ Fixture fixture;
  Host host(fixture.mapped());
  const AvContextRecord opengl(SubApi::opengl, 12u, fixture.ring().handle(), page);
  const CapsetInfo vulkan_only(av::ProtocolVersion(1u, 0u), 0x1u, 64u, 12u, 20u, 0u,
                               av::GpuIdentity(0u, 0u, 0u, uuid, 0u, 0u, "gpu"));
  CHECK_STATUS(fixture.table().bind(&first_context, opengl, fixture.device(),
                                    fixture.mapped().function().doorbells(), vulkan_only),
               Status::invalid_parameter);
  CHECK_EQ(host.count_of(ctx_create_type), 0u);
  CHECK_EQ(fixture.table().bound(), 0u); }

TEST(av_context_table_unbind, tears_down_on_the_device_or_lets_go_without_one)
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
  CHECK_EQ(fixture.platform().process_mappings(), 0u);
  CHECK_EQ(fixture.platform().owned_page_blocks(), 0u);
  CHECK(fixture.table().invariants_hold()); }

/* A device that will not answer a teardown still has its context let go of,
 * but may still write its ring, fence page and reply slot: those are kept out
 * of use for good, with no view of them left. */
TEST(av_context_table_unbind, a_device_that_stopped_answering_still_releases_the_slot)
{ Fixture fixture;
  { Host host(fixture.mapped());
    CHECK_STATUS(fixture.bind(&first_context), Status::ok); }
  fixture.platform().on_stall(nullptr, nullptr);
  CHECK_STATUS(fixture.device().stop(), Status::ok);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok);
  CHECK_EQ(fixture.table().bound(), 0u);
  CHECK_EQ(fixture.platform().mapped_frames(), 0u);
  CHECK_EQ(fixture.platform().process_mappings(), 0u);
  CHECK_EQ(fixture.platform().reclaim_page_blocks(), 3u); }

TEST(av_context_table_abandon_all, a_reset_device_serves_and_accepts_nothing_until_unbound)
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
  /* A reset device holds nothing, so the memory is free again. */
  CHECK_EQ(fixture.platform().owned_page_blocks(), 0u);
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.node().abort_pending());
  fixture.platform().set_irql(Irql::passive); }

TEST(av_context_table_user_of, names_the_context_an_allocation_names)
{ Fixture fixture;
  Host host(fixture.mapped());
  CHECK(fixture.table().user_of(fixture.ring().allocation()) == nullptr);
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  CHECK(fixture.table().user_of(fixture.ring().allocation()) == &first_context);
  CHECK(fixture.table().user_of(nullptr) == nullptr);
  int unrelated = 0;
  CHECK(fixture.table().user_of(static_cast<const Allocation *>(static_cast<void *>(&unrelated))) == nullptr);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

TEST(av_context_table, runs_at_the_levels_it_declares)
{ Fixture fixture;
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.table().bind(&first_context, fixture.record(), fixture.device(),
                                         fixture.mapped().function().doorbells(), capset()));
  static_cast<void>(fixture.table().unbind(&first_context, nullptr));
  av::ContextViewsRecord views;
  static_cast<void>(fixture.table().views(&first_context, views));
  CHECK(fixture.platform().claim(Violation::irql_ceiling));
  fixture.table().abandon_all();
  static_cast<void>(fixture.table().user_of(nullptr));
  fixture.platform().set_irql(Irql::isr);
  std::uint64_t awaited = 0u;
  static_cast<void>(fixture.table().submit(&first_context, nullptr, 0u, awaited));
  fixture.table().serve(fixture.node(), fixture.queues());
  fixture.platform().set_irql(Irql::passive);
  while (fixture.platform().claim(Violation::irql_ceiling)) {}
  while (fixture.platform().claim(Violation::allocation_at_dispatch)) {}
  while (fixture.platform().claim(Violation::blocking_at_dispatch)) {}
  while (fixture.platform().claim(Violation::lock_order)) {}
  while (fixture.platform().claim(Violation::release_order)) {} }

TEST(av_context_table_invariants, detect_a_slot_live_unbound_or_bound_without_its_name_or_memory)
{ Fixture fixture;
  Host host(fixture.mapped());
  AvContextTable &table = fixture.table();
  CHECK(table.invariants_hold());
  /* Free slots: live, holding memory, or naming an allocation. */
  AvContextTableProbe::set_live(table, 5u, true);
  CHECK(!table.invariants_hold());
  AvContextTableProbe::set_live(table, 5u, false);
  static_cast<void>(AvContextTableProbe::memory(table, 6u).construct(fixture.platform()));
  CHECK(!table.invariants_hold());
  AvContextTableProbe::memory(table, 6u).destroy();
  AvContextTableProbe::swap_name(table, 6u, fixture.ring().allocation());
  CHECK(!table.invariants_hold());
  AvContextTableProbe::swap_name(table, 6u, nullptr);
  /* A slot claimed but not yet set up is bound without a binding or memory;
   * live, it is broken. */
  AvContextTableProbe::set_context(table, 7u, &second_context);
  AvContextTableProbe::swap_name(table, 7u, fixture.ring().allocation());
  CHECK(table.invariants_hold());
  AvContextTableProbe::set_live(table, 7u, true);
  CHECK(!table.invariants_hold());
  AvContextTableProbe::set_live(table, 7u, false);
  /* Claimed with no name at all. */
  AvContextTableProbe::swap_name(table, 7u, nullptr);
  CHECK(!table.invariants_hold());
  AvContextTableProbe::set_context(table, 7u, nullptr);
  CHECK(table.invariants_hold());
  /* A bound, live slot: named, and on memory that is held and mapped into
   * the process. */
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  CHECK(table.invariants_hold());
  const Allocation *const name = AvContextTableProbe::swap_name(table, 0u, nullptr);
  CHECK(!table.invariants_hold());
  AvContextTableProbe::swap_name(table, 0u, name);
  av::ContextMemory &memory = fixture.memory();
  memory.release();
  CHECK(!table.invariants_hold());   /* live on no memory */
  CHECK_STATUS(memory.allocate(12u, 4096u), Status::ok);
  CHECK(!table.invariants_hold());   /* live on memory the process cannot see */
  CHECK_STATUS(memory.map_into_process(), Status::ok);
  CHECK(table.invariants_hold());
  /* Memory whose own invariant is broken. */
  AvContextTableProbe::set_live(table, 0u, false);
  av::ContextMemoryProbe::fence(memory).release();
  CHECK(!table.invariants_hold());
  memory.release();
  CHECK(fixture.platform().claim(Violation::broken_invariant));
  CHECK_STATUS(memory.allocate(12u, 4096u), Status::ok);
  CHECK_STATUS(memory.map_into_process(), Status::ok);
  AvContextTableProbe::set_live(table, 0u, true);
  CHECK(table.invariants_hold());
  CHECK_STATUS(table.unbind(&first_context, &fixture.device()), Status::ok); }

/* A handle whose data is not one of this adapter's allocations names no memory. */
TEST(av_context_table_bind, a_handle_to_something_that_is_not_an_allocation_is_refused)
{ Fixture fixture;
  Host host(fixture.mapped());
  int not_an_allocation = 0;
  const std::uint32_t handle = fixture.handles().insert(&not_an_allocation);
  const AvContextRecord record(SubApi::vulkan, 12u, handle, page);
  CHECK_STATUS(fixture.bind(&first_context, record), Status::invalid_parameter);
  CHECK_EQ(fixture.handles().outstanding(), 0u);
  CHECK_STATUS(fixture.table().unbind(nullptr, &fixture.device()), Status::invalid_parameter); }

/* A context its device lied to has failed, but the device still holds it: the
 * teardown is still sent. */
TEST(av_context_table_unbind, a_context_failed_by_its_fence_page_is_still_destroyed_on_the_device)
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

/* An adapter destroyed with contexts still bound lets go of their pages: its
 * device stopped first, and holds none of them. */
TEST(av_context_table, destruction_releases_what_is_still_bound)
{ Fixture fixture;
  Host host(fixture.mapped());
  { AvContextTable table(fixture.platform(), fixture.mapped().interface(), fixture.allocations(), fixture.window());
    CHECK_STATUS(table.bind(&first_context, fixture.record(), fixture.device(),
                            fixture.mapped().function().doorbells(), capset()),
                 Status::ok);
    CHECK_EQ(fixture.platform().mapped_frames(), 2u);
    CHECK_EQ(fixture.platform().owned_page_blocks(), 3u); }
  CHECK_EQ(fixture.platform().mapped_frames(), 0u);
  CHECK_EQ(fixture.platform().process_mappings(), 0u);
  CHECK_EQ(fixture.platform().owned_page_blocks(), 0u); }

/* FR-006: a context the host faulted has everything it queued completed, in
 * order, so the process waiting on it wakes; its next submission is refused
 * and publishes nothing; a packet of another context between its packets is
 * still held for that context's own host. */
TEST(av_context_table_serve, a_fault_the_host_writes_completes_that_context_s_work_and_refuses_more)
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
TEST(av_context_table_serve, a_packet_queued_after_the_fault_was_seen_is_finished_by_the_next_serve)
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
TEST(av_context_table_serve, a_page_no_device_could_write_loses_the_context_too)
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

/* ---- window chunks (docs/host-window.md) ------------------------------------------ */

/* A process asks for a chunk of the host window on its live av context: the
 * driver takes pages of the window, has the device make a host blob of one of
 * the context's chunk resources and place it there, and maps those pages into
 * the process. */
namespace {

using av::HostWindowRecord;
using WindowOperation = av::HostWindowRecord::Operation;

/* A context's chunks take its resources past its three transport ones. */
constexpr std::uint32_t chunk_base = av::AvContextBinding::first_resource_id + 3u;
constexpr std::uint64_t window_base = 0x4000000000ull;
constexpr std::uint64_t window_pages = 16u;
/* What a bound context itself holds: its ring and reply slot in the process,
 * and its ring, fence page and reply slot as pages. */
constexpr std::uint32_t context_views = 2u;
constexpr std::uint32_t context_blocks = 3u;
constexpr std::uint32_t blob_mem_guest = 1u;     /* VIRTIO_GPU_BLOB_MEM_GUEST */
constexpr std::uint32_t blob_mem_host3d = 2u;    /* VIRTIO_GPU_BLOB_MEM_HOST3D */

Status open_window(Fixture &fixture, void *context, std::uint64_t bytes, HostWindowRecord &answer)
{
    answer = HostWindowRecord(WindowOperation::open, bytes, 0u);
    return fixture.table().window_chunk(context, answer, fixture.device());
}

Status close_window(Fixture &fixture, void *context, std::uint32_t resource)
{
    HostWindowRecord record(WindowOperation::close, 0u, resource);
    return fixture.table().window_chunk(context, record, fixture.device());
}

/* The bound context of a fixture whose device offers a window. */
void bind_with_window(Fixture &fixture)
{
    CHECK_STATUS(fixture.window().initialize(window_base, window_pages * page), Status::ok);
    CHECK_STATUS(fixture.bind(&first_context), Status::ok);
}

/* The 64-bit field of the nth request of `type`, at `offset`. */
std::uint64_t field64(Host &host, std::uint32_t type, std::uint32_t nth, std::size_t offset)
{
    return std::uint64_t{host.field(type, nth, offset)} | std::uint64_t{host.field(type, nth, offset + 4u)} << 32u;
}

}  // namespace

TEST(av_context_table_window_chunk, an_open_places_a_host_blob_in_the_window_and_maps_it_into_the_process)
{ Fixture fixture;
  Host host(fixture.mapped());
  bind_with_window(fixture);
  HostWindowRecord answer;
  CHECK_STATUS(open_window(fixture, &first_context, 0x1800u, answer), Status::ok);
  CHECK_EQ(answer.resource_id(), chunk_base);
  CHECK_EQ(answer.bytes(), 0x2000u);   /* whole pages */
  CHECK(answer.address() != 0u);
  CHECK(fixture.platform().maps_into_process(answer.address(), window_base, 0x2000u));
  /* The fourth blob, after the transport's three: HOST3D, mappable, no pages of
   * the guest's, and as long as the pages granted. */
  CHECK_EQ(host.field(create_blob_type, 3u, 24u), chunk_base);
  CHECK_EQ(host.field(create_blob_type, 3u, 28u), blob_mem_host3d);
  CHECK_EQ(host.field(create_blob_type, 3u, 32u), 1u);   /* VIRTIO_GPU_BLOB_FLAG_USE_MAPPABLE */
  CHECK_EQ(host.field(create_blob_type, 3u, 36u), 0u);
  CHECK_EQ(field64(host, create_blob_type, 3u, 48u), 0x2000u);
  CHECK_EQ(host.field(map_blob_type, 0u, 24u), chunk_base);
  CHECK_EQ(field64(host, map_blob_type, 0u, 32u), 0u);
  CHECK_EQ(fixture.window().pages_in_use(), 2u);
  CHECK_EQ(fixture.platform().owned_page_blocks(), context_blocks);   /* no guest RAM taken */

  /* The next chunk takes the next slot and the next free pages. */
  HostWindowRecord second;
  CHECK_STATUS(open_window(fixture, &first_context, page, second), Status::ok);
  CHECK_EQ(second.resource_id(), chunk_base + 1u);
  CHECK_EQ(field64(host, map_blob_type, 1u, 32u), 2u * page);
  CHECK(fixture.platform().maps_into_process(second.address(), window_base + 2u * page, page));
  CHECK_EQ(fixture.platform().process_mappings(), context_views + 2u);

  /* A close takes the process's view down, then the blob, then the pages. */
  host.watch(fixture.window());
  CHECK_STATUS(close_window(fixture, &first_context, chunk_base), Status::ok);
  CHECK_EQ(host.field(unmap_blob_type, 0u, 24u), chunk_base);
  CHECK_EQ(host.field(unref_type, 0u, 24u), chunk_base);
  CHECK_EQ(host.views_when_unmapped(), context_views + 1u);   /* the process's view was already gone */
  CHECK_EQ(host.pages_when_released(), 3u);                   /* the pages were not yet back */
  CHECK_EQ(fixture.window().pages_in_use(), 1u);
  CHECK_EQ(fixture.platform().process_mappings(), context_views + 1u);
  CHECK(!fixture.platform().maps_into_process(answer.address(), window_base, 0x2000u));

  /* First fit from the start of the window: the pages just freed are reused,
   * and so is the chunk slot. */
  HostWindowRecord again;
  CHECK_STATUS(open_window(fixture, &first_context, page, again), Status::ok);
  CHECK_EQ(again.resource_id(), chunk_base);
  CHECK_EQ(field64(host, map_blob_type, 2u, 32u), 0u);
  CHECK(fixture.platform().maps_into_process(again.address(), window_base, page));
  CHECK(fixture.table().invariants_hold());

  /* Destroying the context closes what it still holds, after the context
   * itself: the device ends a context's imports when it destroys it. */
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok);
  CHECK_EQ(fixture.platform().process_mappings(), 0u);
  CHECK_EQ(fixture.window().pages_in_use(), 0u);
  CHECK_EQ(host.count_of(unmap_blob_type), 3u);
  std::uint32_t destroy = 0u, last_unmap = 0u;
  for (std::uint32_t i = 0u; i < host.queue().requests(); i++) {
      if (host.queue().request_type(i) == ctx_destroy_type) destroy = i;
      if (host.queue().request_type(i) == unmap_blob_type) last_unmap = i;
  }
  CHECK(destroy < last_unmap);
  CHECK(fixture.table().invariants_hold()); }

/* A chunk the window cannot hold - there is none - is guest RAM the driver
 * owns: a GUEST blob over its pages, mapped cached into the process, and freed
 * once the device lets go of it. The process cannot tell the two kinds apart
 * but by where they are. */
TEST(av_context_table_ram_chunk, an_open_with_no_window_is_guest_ram_published_and_mapped)
{ Fixture fixture;
  Host host(fixture.mapped());
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  HostWindowRecord answer;
  CHECK_STATUS(open_window(fixture, &first_context, 0x1800u, answer), Status::ok);
  CHECK_EQ(answer.resource_id(), chunk_base);
  CHECK_EQ(answer.bytes(), 0x2000u);
  CHECK_EQ(fixture.platform().owned_page_blocks(), context_blocks + 1u);
  CHECK_EQ(fixture.platform().process_mappings(), context_views + 1u);
  /* A GUEST blob of the context's next chunk resource, one memory entry over
   * the pages the process now sees. */
  CHECK_EQ(host.field(create_blob_type, 3u, 24u), chunk_base);
  CHECK_EQ(host.field(create_blob_type, 3u, 28u), blob_mem_guest);
  CHECK_EQ(host.field(create_blob_type, 3u, 36u), 1u);
  const std::uint64_t frame = field64(host, create_blob_type, 3u, 56u);
  CHECK(fixture.platform().maps_into_process(answer.address(), frame, 0x2000u));
  CHECK_EQ(at(answer.address())[0x1FFF], 0u);   /* zeroed */
  CHECK(fixture.table().invariants_hold());
  /* A close has the device let go of the blob, then frees the pages. */
  CHECK_STATUS(close_window(fixture, &first_context, chunk_base), Status::ok);
  CHECK_EQ(host.field(unref_type, 0u, 24u), chunk_base);
  CHECK_EQ(host.count_of(unmap_blob_type), 0u);   /* nothing was placed */
  CHECK_EQ(fixture.platform().process_mappings(), context_views);
  CHECK_EQ(fixture.platform().owned_page_blocks(), context_blocks);
  CHECK_STATUS(close_window(fixture, &first_context, chunk_base), Status::invalid_parameter);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok);
  CHECK_EQ(fixture.platform().owned_page_blocks(), 0u); }

/* A window with no room for the chunk, one withdrawn, a device that will not
 * make or place the host blob, or a process that cannot be given a view of the
 * window - a 32-bit one, where the window lies above 4 GiB - all get guest RAM
 * instead, with whatever the window took given back. */
TEST(av_context_table_ram_chunk, whatever_keeps_a_chunk_out_of_the_window_puts_it_in_guest_ram)
{ struct Case { const char *what; std::uint64_t bytes; std::uint32_t refused; std::uint32_t map_info; bool fail_view;
                bool withdraw; std::uint32_t unmaps; std::uint32_t unrefs; };
  const Case cases[] = {
      {"too big for the window", (window_pages + 1u) * page, 0u, GpuResponse::map_cache_cached, false, false, 0u, 0u},
      {"withdrawn", page, 0u, GpuResponse::map_cache_cached, false, true, 0u, 0u},
      {"host blob refused", page, create_blob_type, GpuResponse::map_cache_cached, false, false, 0u, 0u},
      {"map refused", page, map_blob_type, GpuResponse::map_cache_cached, false, false, 0u, 1u},
      {"map not cached", page, 0u, 0x3u, false, false, 0u, 1u},
      {"no view of the window", page, 0u, GpuResponse::map_cache_cached, true, false, 1u, 1u},
  };
  for (const Case &one : cases) {
      Fixture fixture;
      Host host(fixture.mapped());
      bind_with_window(fixture);
      if (one.withdraw) fixture.window().withdraw();
      host.refuse_next(one.refused);
      host.answer_map_with(one.map_info);
      if (one.fail_view) fixture.platform().fail_process_map_at(1u);
      HostWindowRecord answer;
      CHECK_STATUS(open_window(fixture, &first_context, one.bytes, answer), Status::ok);
      CHECK_EQ(answer.resource_id(), chunk_base);
      CHECK_EQ(host.count_of(unmap_blob_type), one.unmaps);
      CHECK_EQ(host.count_of(unref_type), one.unrefs);
      /* The window's pages went back, and the chunk is guest RAM. */
      CHECK_EQ(fixture.window().pages_in_use(), 0u);
      CHECK_EQ(fixture.platform().owned_page_blocks(), context_blocks + 1u);
      CHECK_EQ(fixture.platform().process_mappings(), context_views + 1u);
      CHECK(!fixture.platform().maps_into_process(answer.address(), window_base, one.bytes));
      CHECK(fixture.table().invariants_hold());
      host.answer_map_with(GpuResponse::map_cache_cached);
      CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok);
      CHECK_EQ(fixture.platform().owned_page_blocks(), 0u);
  } }

/* Each thing an open or a close can be refused for, and none of them leaves
 * anything taken, placed or mapped. */
TEST(av_context_table_window_chunk, refuses_what_it_cannot_serve_and_holds_nothing_after)
{ Fixture fixture;
  Host host(fixture.mapped());
  HostWindowRecord answer;
  /* No context bound. */
  CHECK_STATUS(open_window(fixture, &first_context, page, answer), Status::invalid_parameter);
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  const std::uint32_t asked = host.queue().requests();
  CHECK_STATUS(fixture.window().initialize(window_base, window_pages * page), Status::ok);
  /* More than a page list can count, in the window or in guest RAM, and more
   * than whole pages can hold at all. */
  CHECK_STATUS(open_window(fixture, &first_context, (std::uint64_t{UINT32_MAX} + 1u) * page, answer),
               Status::no_space);
  CHECK_STATUS(open_window(fixture, &first_context, UINT64_MAX, answer), Status::no_space);
  /* A resource that is none of the context's chunks. */
  CHECK_STATUS(close_window(fixture, &first_context, chunk_base), Status::invalid_parameter);
  CHECK_EQ(host.queue().requests(), asked);
  CHECK_EQ(fixture.window().pages_in_use(), 0u);
  CHECK_EQ(fixture.platform().process_mappings(), context_views);
  CHECK_EQ(fixture.platform().owned_page_blocks(), context_blocks);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

/* Guest RAM that is not there, a process that cannot be given a view of it, or
 * a GUEST blob the device refuses: nothing held, and the slot free again. */
TEST(av_context_table_ram_chunk, memory_a_view_or_a_blob_that_cannot_be_had_holds_nothing)
{ Fixture fixture;
  Host host(fixture.mapped());
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  HostWindowRecord answer;
  fixture.platform().fail_page_allocation_at(1u);
  CHECK_STATUS(open_window(fixture, &first_context, page, answer), Status::no_memory);
  fixture.platform().fail_allocation_at(1u);   /* the pages' frames */
  CHECK_STATUS(open_window(fixture, &first_context, page, answer), Status::no_memory);
  fixture.platform().fail_process_map_at(1u);
  CHECK_STATUS(open_window(fixture, &first_context, page, answer), Status::no_memory);
  const std::uint32_t asked = host.queue().requests();
  host.refuse_next(create_blob_type);
  CHECK_STATUS(open_window(fixture, &first_context, page, answer), Status::not_supported);
  CHECK_EQ(host.queue().requests(), asked + 1u);
  CHECK_EQ(answer.resource_id(), 0u);
  CHECK_EQ(fixture.platform().owned_page_blocks(), context_blocks);
  CHECK_EQ(fixture.platform().process_mappings(), context_views);
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(open_window(fixture, &first_context, page, answer), Status::ok);
  CHECK_EQ(answer.resource_id(), chunk_base);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

/* A RAM chunk an import still holds - the device will not let go of it - stays
 * bound, its pages and the process's view with it, so a process closing chunks
 * early cannot have the driver keep page after page out of use; the context's
 * teardown ends the import, and the pages go then. */
TEST(av_context_table_ram_chunk, one_the_device_will_not_let_go_of_stays_bound_until_the_context_goes)
{ Fixture fixture;
  Host host(fixture.mapped());
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  HostWindowRecord answer;
  CHECK_STATUS(open_window(fixture, &first_context, page, answer), Status::ok);
  host.refuse_next(unref_type);
  CHECK_STATUS(close_window(fixture, &first_context, chunk_base), Status::not_supported);
  CHECK_EQ(fixture.platform().owned_page_blocks(), context_blocks + 1u);
  CHECK_EQ(fixture.platform().process_mappings(), context_views + 1u);
  /* The slot is still taken: the next chunk is the next resource. */
  HostWindowRecord next;
  CHECK_STATUS(open_window(fixture, &first_context, page, next), Status::ok);
  CHECK_EQ(next.resource_id(), chunk_base + 1u);
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok);
  CHECK_EQ(host.count_of(unref_type), 1u + 2u + context_blocks);
  CHECK_EQ(fixture.platform().owned_page_blocks(), 0u);
  CHECK_EQ(fixture.platform().process_mappings(), 0u); }

/* A device that answers a GUEST blob with nothing may have made it over the
 * pages, and one that stopped answering may still hold a chunk's blob: both
 * keep the pages out of use for good, with the process's view gone. */
TEST(av_context_table_ram_chunk, pages_a_device_may_still_hold_are_never_freed)
{ Fixture fixture;
  { Host host(fixture.mapped());
    CHECK_STATUS(fixture.bind(&first_context), Status::ok);
    HostWindowRecord answer;
    CHECK_STATUS(open_window(fixture, &first_context, page, answer), Status::ok); }
  fixture.platform().on_stall(nullptr, nullptr);
  HostWindowRecord unanswered;
  const Status status = open_window(fixture, &first_context, page, unanswered);
  CHECK(status != Status::ok && status != Status::not_supported);
  CHECK_EQ(fixture.platform().process_mappings(), context_views + 1u);
  CHECK_STATUS(fixture.device().stop(), Status::ok);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok);
  CHECK_EQ(fixture.table().bound(), 0u);
  CHECK_EQ(fixture.platform().process_mappings(), 0u);
  /* The context's three blocks, the chunk it held, and the unanswered one. */
  CHECK_EQ(fixture.platform().reclaim_page_blocks(), context_blocks + 2u); }

/* A context the device no longer holds - reset, or gone - holds none of its
 * RAM chunks either: freed with nothing asked. */
TEST(av_context_table_ram_chunk, a_context_the_device_let_go_of_frees_its_ram_silently)
{ const bool abandoned[] = {true, false};
  for (bool reset : abandoned) {
      Fixture fixture;
      Host host(fixture.mapped());
      CHECK_STATUS(fixture.bind(&first_context), Status::ok);
      HostWindowRecord answer;
      CHECK_STATUS(open_window(fixture, &first_context, 3u * page, answer), Status::ok);
      if (reset) fixture.table().abandon_all();
      const std::uint32_t asked = host.queue().requests();
      CHECK_STATUS(fixture.table().unbind(&first_context, reset ? &fixture.device() : nullptr), Status::ok);
      CHECK_EQ(host.queue().requests(), asked);
      CHECK_EQ(fixture.platform().process_mappings(), 0u);
      CHECK_EQ(fixture.platform().owned_page_blocks(), 0u);
      CHECK(fixture.table().invariants_hold());
  } }

/* Every chunk slot taken refuses the next open without taking pages or asking
 * the device. */
TEST(av_context_table_window_chunk, the_context_s_chunk_slots_full_refuses_the_next)
{ Fixture fixture;
  Host host(fixture.mapped());
  CHECK_STATUS(fixture.window().initialize(window_base, 2u * av::AvContextBinding::chunk_slots * page), Status::ok);
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  HostWindowRecord answer;
  for (std::uint32_t index = 0u; index < av::AvContextBinding::chunk_slots; index++)
      CHECK_STATUS(open_window(fixture, &first_context, page, answer), Status::ok);
  const std::uint32_t asked = host.queue().requests();
  CHECK_STATUS(open_window(fixture, &first_context, page, answer), Status::no_space);
  CHECK_EQ(host.queue().requests(), asked);
  CHECK_EQ(fixture.window().pages_in_use(), av::AvContextBinding::chunk_slots);
  CHECK_EQ(fixture.platform().owned_page_blocks(), context_blocks);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok);
  CHECK_EQ(fixture.window().pages_in_use(), 0u);
  CHECK_EQ(fixture.platform().process_mappings(), 0u); }

/* A blob the device would not release may still be placed over its pages, so
 * they stay out of the window for good - whether the open was being undone or
 * the chunk closed - and the next chunk lands elsewhere. */
TEST(av_context_table_window_chunk, pages_the_device_did_not_let_go_of_are_never_handed_out_again)
{ Fixture fixture;
  Host host(fixture.mapped());
  bind_with_window(fixture);
  fixture.platform().fail_process_map_at(1u);
  host.refuse_next(unref_type);
  HostWindowRecord answer;
  /* The window's view failed and its blob stayed: those pages are kept, and
   * the chunk is guest RAM. */
  CHECK_STATUS(open_window(fixture, &first_context, page, answer), Status::ok);
  CHECK_EQ(fixture.window().pages_in_use(), 1u);
  CHECK_EQ(fixture.platform().owned_page_blocks(), context_blocks + 1u);
  HostWindowRecord windowed;
  CHECK_STATUS(open_window(fixture, &first_context, page, windowed), Status::ok);
  CHECK_EQ(windowed.resource_id(), chunk_base + 1u);
  CHECK(fixture.platform().maps_into_process(windowed.address(), window_base + page, page));
  host.refuse_next(unmap_blob_type);
  CHECK_STATUS(close_window(fixture, &first_context, chunk_base + 1u), Status::ok);
  CHECK_EQ(fixture.platform().process_mappings(), context_views + 1u);   /* its view is gone all the same */
  CHECK_EQ(fixture.window().pages_in_use(), 2u);
  CHECK_STATUS(close_window(fixture, &first_context, chunk_base + 1u), Status::invalid_parameter);
  CHECK_STATUS(open_window(fixture, &first_context, page, windowed), Status::ok);
  CHECK(fixture.platform().maps_into_process(windowed.address(), window_base + 2u * page, page));
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok);
  CHECK_EQ(fixture.window().pages_in_use(), 2u);
  CHECK_EQ(fixture.platform().owned_page_blocks(), 0u); }

/* A context the device no longer holds - reset, or gone - holds none of its
 * window chunks either: the process's views come down and the pages are free,
 * with nothing asked of the device. */
TEST(av_context_table_window_chunk, a_context_the_device_let_go_of_frees_its_pages_silently)
{ const bool abandoned[] = {true, false};
  for (bool reset : abandoned) {
      Fixture fixture;
      Host host(fixture.mapped());
      bind_with_window(fixture);
      HostWindowRecord answer;
      CHECK_STATUS(open_window(fixture, &first_context, 3u * page, answer), Status::ok);
      if (reset) fixture.table().abandon_all();
      if (reset) CHECK_STATUS(open_window(fixture, &first_context, page, answer), Status::invalid_parameter);
      const std::uint32_t asked = host.queue().requests();
      CHECK_STATUS(fixture.table().unbind(&first_context, reset ? &fixture.device() : nullptr), Status::ok);
      CHECK_EQ(host.queue().requests(), asked);
      CHECK_EQ(fixture.platform().process_mappings(), 0u);
      CHECK_EQ(fixture.window().pages_in_use(), 0u);
      CHECK(fixture.table().invariants_hold());
  } }

/* A device that stopped answering mid-teardown may still hold the blob placed:
 * the view comes down, and the pages are kept. */
TEST(av_context_table_window_chunk, a_device_that_stopped_answering_keeps_the_pages)
{ Fixture fixture;
  { Host host(fixture.mapped());
    bind_with_window(fixture);
    HostWindowRecord answer;
    CHECK_STATUS(open_window(fixture, &first_context, page, answer), Status::ok); }
  fixture.platform().on_stall(nullptr, nullptr);
  CHECK_STATUS(fixture.device().stop(), Status::ok);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok);
  CHECK_EQ(fixture.table().bound(), 0u);
  CHECK_EQ(fixture.platform().process_mappings(), 0u);
  CHECK_EQ(fixture.window().pages_in_use(), 1u);
  CHECK_EQ(fixture.platform().reclaim_page_blocks(), context_blocks); }

/* A table destroyed with chunks open - one in the window, one in guest RAM -
 * still takes the process's views down and frees the RAM: the device stopped
 * before the adapter went. */
TEST(av_context_table_window_chunk, destruction_unmaps_what_is_still_open)
{ Fixture fixture;
  Host host(fixture.mapped());
  CHECK_STATUS(fixture.window().initialize(window_base, page), Status::ok);
  { AvContextTable table(fixture.platform(), fixture.mapped().interface(), fixture.allocations(), fixture.window());
    CHECK_STATUS(table.bind(&first_context, fixture.record(), fixture.device(),
                            fixture.mapped().function().doorbells(), capset()),
                 Status::ok);
    HostWindowRecord windowed(WindowOperation::open, page, 0u);
    CHECK_STATUS(table.window_chunk(&first_context, windowed, fixture.device()), Status::ok);
    HostWindowRecord ram(WindowOperation::open, page, 0u);
    CHECK_STATUS(table.window_chunk(&first_context, ram, fixture.device()), Status::ok);
    CHECK_EQ(fixture.platform().process_mappings(), context_views + 2u);
    CHECK_EQ(fixture.platform().owned_page_blocks(), context_blocks + 1u); }
  CHECK_EQ(fixture.platform().process_mappings(), 0u);
  CHECK_EQ(fixture.platform().mapped_frames(), 0u);
  CHECK_EQ(fixture.platform().owned_page_blocks(), 0u); }

/* A context the host faulted takes no chunk either. */
TEST(av_context_table_window_chunk, a_context_the_host_faulted_is_refused)
{ Fixture fixture;
  Host host(fixture.mapped());
  bind_with_window(fixture);
  fixture.complete_through(0u, 6u);
  fixture.serve();
  const std::uint32_t asked = host.queue().requests();
  HostWindowRecord answer;
  CHECK_STATUS(open_window(fixture, &first_context, page, answer), Status::invalid_parameter);
  CHECK_EQ(host.queue().requests(), asked);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

TEST(av_context_table_window_chunk, is_passive_only)
{ Fixture fixture;
  fixture.platform().set_irql(Irql::dispatch);
  HostWindowRecord answer(WindowOperation::open, page, 0u);
  static_cast<void>(fixture.table().window_chunk(&first_context, answer, fixture.device()));
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().claim(Violation::irql_ceiling));
  while (fixture.platform().claim(Violation::irql_ceiling)) {}
  while (fixture.platform().claim(Violation::blocking_at_dispatch)) {}
  while (fixture.platform().claim(Violation::lock_order)) {}
  while (fixture.platform().claim(Violation::release_order)) {} }

/* A chunk slot holding pages with no resource, or a chunk on a slot no context
 * holds, is a table that has lost track of what the device holds. */
TEST(av_context_table_invariants, detect_a_broken_chunk_or_one_on_a_free_slot)
{ Fixture fixture;
  Host host(fixture.mapped());
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  av::GuestPages pages(fixture.platform());
  av::ContextChunk &held = AvContextTableProbe::chunk(fixture.table(), 0u, 3u);
  held.take_ram(0u, pages);
  CHECK(!fixture.table().invariants_hold());
  held.clear();
  CHECK(fixture.table().invariants_hold());
  av::ContextChunk &stray = AvContextTableProbe::chunk(fixture.table(), 5u, 0u);
  stray.take_ram(7u, pages);
  CHECK(!fixture.table().invariants_hold());
  stray.clear();
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

TEST(av_context_table_report, writes_a_window_chunk_s_pages)
{ Fixture fixture;
  Host host(fixture.mapped());
  bind_with_window(fixture);
  HostWindowRecord answer;
  CHECK_STATUS(open_window(fixture, &first_context, page, answer), Status::ok);
  CHECK_STATUS(open_window(fixture, &first_context, 2u * page, answer), Status::ok);
  char bytes[1024];
  av::StateWriter writer(bytes, sizeof bytes);
  fixture.table().report(writer);
  char expected[128];
  std::snprintf(expected, sizeof expected, "av_context_table/0/av_context/0/chunk/1 resource=%u window_page=1 pages=2",
                static_cast<unsigned>(chunk_base + 1u));
  CHECK(std::strstr(writer.dump(), expected) != nullptr);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

/* ---- report -------------------------------------------------------------------- */

TEST(av_context_table_report, writes_each_live_context_its_memory_and_its_chunks)
{ Fixture fixture;
  Host host(fixture.mapped());
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  HostWindowRecord answer;
  CHECK_STATUS(open_window(fixture, &first_context, page, answer), Status::ok);
  char bytes[1024];
  av::StateWriter writer(bytes, sizeof bytes);
  fixture.table().report(writer);
  const auto hex_of = [](const void *object) {
      return static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(object));
  };
  char expected[1024];
  std::snprintf(expected, sizeof expected,
                "av_context_table/0 busy=false bound=1\n"
                "av_context_table/0/av_context/0 handle=0x%016llX doorbell=0 ring_order=12 name=0x%016llX"
                " ring_view=%llu reply_view=%llu state=2 context_id=1 completed=0 live=true lost=false\n"
                "av_context_table/0/av_context/0/chunk/0 resource=%u ram_bytes=4096",
                hex_of(&first_context), hex_of(fixture.ring().allocation()),
                static_cast<unsigned long long>(fixture.memory().ring_view()),
                static_cast<unsigned long long>(fixture.memory().reply_view()), static_cast<unsigned>(chunk_base));
  CHECK(std::strcmp(writer.dump(), expected) == 0);
  CHECK(fixture.table().invariants_hold());
  CHECK_EQ(fixture.platform().locks_held(), 0u);
  /* A context faulted by its fence page is reported lost once the DPC saw it. */
  fixture.complete_through(0u, 3u);
  fixture.serve();
  av::StateWriter faulted(bytes, sizeof bytes);
  fixture.table().report(faulted);
  CHECK(std::strstr(faulted.dump(), " lost=true\n") != nullptr);
  fixture.platform().hold_elsewhere(av::LockRank::av_context_slots);
  av::StateWriter busy(bytes, sizeof bytes);
  fixture.table().report(busy);
  CHECK(std::strcmp(busy.dump(), "av_context_table/0 busy=true") == 0);
  fixture.platform().set_irql(Irql::isr);
  fixture.table().report(busy);
  CHECK(fixture.platform().claim(av::Violation::irql_ceiling));
  CHECK(fixture.platform().claim(av::Violation::irql_ceiling));
  fixture.platform().set_irql(Irql::passive);
  fixture.platform().released_elsewhere();
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

namespace {

/* A dump taken at each lock the bind takes, as an inspector's escape could be
 * served on another thread meanwhile. */
struct MidBind {
    av::MockPlatform *platform;
    AvContextTable  *table;
    bool              claimed_unset = false;
};

void report_mid_bind(void *context)
{
    auto *const mid = static_cast<MidBind *>(context);
    mid->platform->on_acquire(nullptr, nullptr);
    char bytes[512];
    av::StateWriter writer(bytes, sizeof bytes);
    mid->table->report(writer);
    if (std::strcmp(writer.dump(), "av_context_table/0 busy=false bound=1") == 0) mid->claimed_unset = true;
    mid->platform->on_acquire(&report_mid_bind, mid);
}

}  // namespace

/* A context claimed but not yet set up has no binding to describe: it is
 * counted and not listed. */
TEST(av_context_table_report, a_context_being_set_up_is_counted_but_not_listed)
{ Fixture fixture;
  Host host(fixture.mapped());
  MidBind mid{&fixture.platform(), &fixture.table()};
  fixture.platform().on_acquire(&report_mid_bind, &mid);
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  fixture.platform().on_acquire(nullptr, nullptr);
  CHECK(mid.claimed_unset);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

/* Feature 050: an OpenGL context's options reach the host's setup, and a share
 * context - named by its ring's D3DKMT handle, which only resolves in the
 * process that made it - becomes that context's device id. Everything the
 * record cannot mean is refused before a request is sent. */
namespace {

using av::GlContextOptions;
using GlProfile = av::GlContextOptions::Profile;

constexpr std::size_t setup_offset = 32u;   /* past submit_3d's header and size */
constexpr std::size_t options_offset = setup_offset + 32u;

CapsetInfo capset_at(std::uint16_t minor)
{
    return CapsetInfo(av::ProtocolVersion(1u, minor), 0x7u, 64u, 12u, 20u, 0u,
                      av::GpuIdentity(0u, 0u, 0u, uuid, 0u, 0u, "gpu"));
}

/* A second context's name. */
class Transport {
public:
    explicit Transport(Fixture &fixture)
        : ring_(fixture.platform(), fixture.allocations(), fixture.handles(), AllocationKind::context_ring, page)
    {
    }
    AvContextRecord record(SubApi api, const GlContextOptions &options) const
    {
        return AvContextRecord(api, 12u, ring_.handle(), page, options);
    }
    ProcessAllocation &ring() { return ring_; }

private:
    ProcessAllocation ring_;
};

Status bind_at(Fixture &fixture, void *context, const AvContextRecord &record, std::uint16_t minor = 1u)
{
    return fixture.table().bind(context, record, fixture.device(), fixture.mapped().function().doorbells(),
                                capset_at(minor));
}

std::uint8_t setup_byte(const Host &host, std::uint32_t nth, std::size_t offset)
{
    return static_cast<std::uint8_t>(host.field(submit_3d_type, nth, offset) & 0xFFu);
}

int third_context = 0;

}  // namespace

TEST(av_context_table_bind, a_gl_contexts_options_reach_the_setup_unchanged)
{ Fixture fixture;
  Host host(fixture.mapped());
  const GlContextOptions options(4u, 6u, GlProfile::core,
                                 GlContextOptions::debug | GlContextOptions::robust_access, 0u);
  CHECK_STATUS(bind_at(fixture, &first_context,
                       AvContextRecord(SubApi::opengl, 12u, fixture.ring().handle(), page, options)),
               Status::ok);
  CHECK_EQ(host.count_of(submit_3d_type), 1u);
  CHECK_EQ(host.field(submit_3d_type, 0u, 24u), 48u);            /* the setup's size */
  CHECK_EQ(host.field(submit_3d_type, 0u, setup_offset + 4u), 0x00010001u);   /* protocol 1.1 */
  CHECK_EQ(setup_byte(host, 0u, options_offset), 4u);
  CHECK_EQ(setup_byte(host, 0u, options_offset + 1u), 6u);
  CHECK_EQ(setup_byte(host, 0u, options_offset + 2u), 1u);
  CHECK_EQ(setup_byte(host, 0u, options_offset + 3u), 5u);
  CHECK_EQ(host.field(submit_3d_type, 0u, options_offset + 4u), 0u);
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

/* A device at the previous minor is given the 32-byte setup of old, which
 * cannot carry options: a record asking for any is refused unsent. */
TEST(av_context_table_bind, a_device_at_the_previous_minor_gets_the_old_setup_and_no_options)
{ Fixture fixture;
  Host host(fixture.mapped());
  CHECK_STATUS(bind_at(fixture, &first_context, fixture.record(), 0u), Status::ok);
  CHECK_EQ(host.field(submit_3d_type, 0u, 24u), 32u);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok);
  const std::uint32_t sent = host.queue().requests();
  const AvContextRecord with_options(SubApi::opengl, 12u, fixture.ring().handle(), page,
                                      GlContextOptions(3u, 3u, GlProfile::core, 0u, 0u));
  CHECK_STATUS(bind_at(fixture, &first_context, with_options, 0u), Status::invalid_parameter);
  CHECK_EQ(host.queue().requests(), sent);
  CHECK_EQ(fixture.table().bound(), 0u);
  CHECK_EQ(fixture.handles().outstanding(), 0u); }

TEST(av_context_table_bind, a_share_context_naming_a_live_gl_context_becomes_its_device_id)
{ Fixture fixture;
  Host host(fixture.mapped());
  const GlContextOptions core(4u, 6u, GlProfile::core, 0u, 0u);
  CHECK_STATUS(bind_at(fixture, &first_context,
                       AvContextRecord(SubApi::opengl, 12u, fixture.ring().handle(), page, core)),
               Status::ok);
  Transport second(fixture);
  CHECK_STATUS(bind_at(fixture, &second_context,
                       second.record(SubApi::opengl, core.sharing(fixture.ring().handle()))),
               Status::ok);
  /* The first context is host context 1, and the second's setup names it. */
  CHECK_EQ(host.count_of(submit_3d_type), 2u);
  CHECK_EQ(host.field(submit_3d_type, 1u, options_offset + 4u), 1u);
  CHECK_EQ(setup_byte(host, 1u, options_offset), 4u);
  CHECK_EQ(fixture.handles().outstanding(), 0u);
  CHECK(fixture.table().invariants_hold());
  /* A third may share the second, whose own ring names it. */
  Transport third(fixture);
  CHECK_STATUS(bind_at(fixture, &third_context, third.record(SubApi::opengl, core.sharing(second.ring().handle()))),
               Status::ok);
  CHECK_EQ(host.field(submit_3d_type, 2u, options_offset + 4u), 2u);
  CHECK_STATUS(fixture.table().unbind(&third_context, &fixture.device()), Status::ok);
  CHECK_STATUS(fixture.table().unbind(&second_context, &fixture.device()), Status::ok);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

TEST(av_context_table_bind, a_share_context_that_is_not_a_live_gl_context_of_the_device_is_refused_unsent)
{ Fixture fixture;
  Host host(fixture.mapped());
  const GlContextOptions core(4u, 6u, GlProfile::core, 0u, 0u);
  /* The first context speaks Vulkan. */
  CHECK_STATUS(bind_at(fixture, &first_context, fixture.record()), Status::ok);
  Transport gl(fixture);
  Transport asking(fixture);
  const std::uint32_t own = asking.ring().handle();
  ProcessAllocation stray(fixture.platform(), fixture.allocations(), fixture.handles(), AllocationKind::context_ring,
                          page);
  ProcessAllocation client(fixture.platform(), fixture.allocations(), fixture.handles(), AllocationKind::client, page);
  /* A handle of another process resolves to nothing here. */
  ProcessAllocation elsewhere(fixture.platform(), fixture.allocations(), fixture.handles(),
                              AllocationKind::context_ring, page);
  const std::uint32_t foreign = elsewhere.handle();
  fixture.handles().remove(foreign);
  const std::uint32_t refused[] = {
      0x7777u,                       /* an unknown handle */
      foreign,                       /* another process's */
      fixture.ring().handle(),       /* a Vulkan context's ring */
      own,                           /* the context's own */
      stray.handle(),                /* a ring no context runs on */
      client.handle(),               /* not a ring */
  };
  const std::uint32_t sent = host.queue().requests();
  for (const std::uint32_t share : refused) {
      CHECK_STATUS(bind_at(fixture, &third_context, asking.record(SubApi::opengl, core.sharing(share))),
                   Status::invalid_parameter);
      CHECK_EQ(host.queue().requests(), sent);
      CHECK_EQ(fixture.table().bound(), 1u);
      CHECK_EQ(fixture.handles().outstanding(), 0u);
      CHECK(fixture.table().invariants_hold());
  }
  /* A destroyed GL context's ring still exists, and names no context. */
  CHECK_STATUS(bind_at(fixture, &second_context, gl.record(SubApi::opengl, core)), Status::ok);
  CHECK_STATUS(fixture.table().unbind(&second_context, &fixture.device()), Status::ok);
  const std::uint32_t after = host.queue().requests();
  CHECK_STATUS(bind_at(fixture, &third_context, asking.record(SubApi::opengl, core.sharing(gl.ring().handle()))),
               Status::invalid_parameter);
  CHECK_EQ(host.queue().requests(), after);
  /* A live GL context whose ring another device holds. */
  CHECK_STATUS(bind_at(fixture, &second_context, gl.record(SubApi::opengl, core)), Status::ok);
  int other_device = 0;
  int this_device = 0;
  int this_process = 0;
  CHECK_STATUS(gl.ring().allocation()->open(&other_device, &this_process), Status::ok);
  CHECK_STATUS(asking.ring().allocation()->open(&this_device, &this_process), Status::ok);
  const std::uint32_t before = host.queue().requests();
  CHECK_STATUS(bind_at(fixture, &third_context, asking.record(SubApi::opengl, core.sharing(gl.ring().handle()))),
               Status::invalid_parameter);
  CHECK_EQ(host.queue().requests(), before);
  /* The same device, and it binds. */
  CHECK_STATUS(asking.ring().allocation()->close(), Status::ok);
  CHECK_STATUS(asking.ring().allocation()->open(&other_device, &this_process), Status::ok);
  CHECK_STATUS(bind_at(fixture, &third_context, asking.record(SubApi::opengl, core.sharing(gl.ring().handle()))),
               Status::ok);
  CHECK_STATUS(fixture.table().unbind(&third_context, &fixture.device()), Status::ok);
  /* An OpenCL context sharing the GL one's objects comes from another ICD, on
   * a device of its own: another device of the same process binds, and a
   * device of another process does not (cl_khr_gl_sharing). */
  const GlContextOptions shares = GlContextOptions().sharing(gl.ring().handle());
  int other_process = 0;
  CHECK_STATUS(asking.ring().allocation()->close(), Status::ok);
  CHECK_STATUS(asking.ring().allocation()->open(&this_device, &other_process), Status::ok);
  const std::uint32_t before_cl = host.queue().requests();
  CHECK_STATUS(bind_at(fixture, &third_context, asking.record(SubApi::opencl, shares)), Status::invalid_parameter);
  CHECK_EQ(host.queue().requests(), before_cl);
  CHECK_STATUS(asking.ring().allocation()->close(), Status::ok);
  CHECK_STATUS(asking.ring().allocation()->open(&this_device, &this_process), Status::ok);
  CHECK_STATUS(bind_at(fixture, &third_context, asking.record(SubApi::opencl, shares)), Status::ok);
  CHECK_STATUS(fixture.table().unbind(&third_context, &fixture.device()), Status::ok);
  CHECK_STATUS(fixture.table().unbind(&second_context, &fixture.device()), Status::ok);
  CHECK_STATUS(asking.ring().allocation()->close(), Status::ok);
  CHECK_STATUS(gl.ring().allocation()->close(), Status::ok);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

/* A GL context the device let go of - a reset abandons every context, which
 * keep their rings until they are destroyed - is no context to share. */
TEST(av_context_table_bind, an_abandoned_gl_context_is_no_share_context)
{ Fixture fixture;
  Host host(fixture.mapped());
  const GlContextOptions core(4u, 6u, GlProfile::core, 0u, 0u);
  CHECK_STATUS(bind_at(fixture, &first_context,
                       AvContextRecord(SubApi::opengl, 12u, fixture.ring().handle(), page, core)),
               Status::ok);
  fixture.table().abandon_all();
  Transport second(fixture);
  const std::uint32_t sent = host.queue().requests();
  CHECK_STATUS(bind_at(fixture, &second_context, second.record(SubApi::opengl, core.sharing(fixture.ring().handle()))),
               Status::invalid_parameter);
  CHECK_EQ(host.queue().requests(), sent);
  CHECK_EQ(fixture.handles().outstanding(), 0u);
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

/* Feature 049: a doorbell connected on a context's hardware queue moves its
 * submissions out of the driver, and the table is what tells the context so.
 * Only a context it bound has anywhere to send that. */
TEST(av_context_table, tells_the_context_whose_process_submits_for_itself)
{ Fixture fixture;
  Host    host(fixture.mapped());
  std::uint64_t progress = 0u;
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_STATUS(fixture.table().user_mode_submits(&first_context, &progress), Status::invalid_parameter);
  CHECK_STATUS(fixture.table().driver_submits(&first_context), Status::invalid_parameter);
  fixture.platform().set_irql(Irql::passive);
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);

  fixture.platform().set_irql(Irql::dispatch);
  CHECK_STATUS(fixture.table().user_mode_submits(&first_context, &progress), Status::ok);
  fixture.platform().set_irql(Irql::passive);
  /* A completion of work the driver never published is believed now, and is
   * reported where the process reads its hardware queue's progress. */
  fixture.complete_through(4u);
  fixture.serve();
  CHECK_EQ(progress, 4u);

  fixture.platform().set_irql(Irql::dispatch);
  CHECK_STATUS(fixture.table().driver_submits(&first_context), Status::ok);
  CHECK_STATUS(fixture.table().driver_submits(&second_context), Status::invalid_parameter);
  fixture.platform().set_irql(Irql::passive);

  /* A device reset leaves the slot naming the context and holding nothing:
   * there is no context left to tell either way. */
  fixture.table().abandon_all();
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_STATUS(fixture.table().user_mode_submits(&first_context, &progress), Status::invalid_parameter);
  CHECK_STATUS(fixture.table().driver_submits(&first_context), Status::invalid_parameter);
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

/* SubmitCommandVirtual asks for a DPC only for a batch the host already
 * finished, whose interrupt may have come before the packet was queued; the
 * answer reads the fence page as it is now. A context that is not there, or
 * is lost, has nothing still running. */
TEST(av_context_table_completed_already, reads_the_fence_page_now)
{ Fixture fixture;
  Host host(fixture.mapped());
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  std::uint64_t awaited = 0u;
  CHECK_STATUS(fixture.submit(&first_context, SubmissionDescriptor(1u, 0u, 32u), awaited), Status::ok);
  CHECK_STATUS(fixture.submit(&first_context, SubmissionDescriptor(2u, 32u, 64u), awaited), Status::ok);
  fixture.complete_through(1u);
  fixture.platform().set_irql(Irql::dispatch);
  CHECK(fixture.table().completed_already(&first_context, 1u));
  CHECK(!fixture.table().completed_already(&first_context, 2u));
  CHECK(fixture.table().completed_already(&second_context, 2u));   /* never bound */
  fixture.platform().set_irql(Irql::passive);
  fixture.complete_through(2u);
  fixture.platform().set_irql(Irql::dispatch);
  CHECK(fixture.table().completed_already(&first_context, 2u));
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

TEST(av_context_table_completed_already, a_lost_context_has_nothing_running)
{ Fixture fixture;
  Host host(fixture.mapped());
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  std::uint64_t awaited = 0u;
  CHECK_STATUS(fixture.submit(&first_context, SubmissionDescriptor(1u, 0u, 32u), awaited), Status::ok);
  CHECK_STATUS(fixture.submit(&first_context, SubmissionDescriptor(2u, 32u, 64u), awaited), Status::ok);
  fixture.complete_through(0u, 6u);
  fixture.serve();
  fixture.platform().set_irql(Irql::dispatch);
  CHECK(fixture.table().completed_already(&first_context, 2u));
  fixture.platform().set_irql(Irql::passive);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

/* A packet that follows a batch the process published by itself kicks the
 * batch's context: its doorbell rings with nothing submitted, so a host worker
 * asleep since the process's own store looks at the ring. A context that is
 * not bound, or is lost, is rung for nothing. */
TEST(av_context_table_kick, rings_a_live_context_s_doorbell_and_nothing_else)
{ Fixture fixture;
  Host host(fixture.mapped());
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  fixture.platform().set_irql(Irql::dispatch);
  fixture.table().kick(&first_context);
  fixture.table().kick(&second_context);
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.mapped().function().doorbells().rings(0u), 1u);
  std::uint64_t tail = 0u;
  std::memcpy(&tail, fixture.ring_memory() + 8u, sizeof tail);
  CHECK_EQ(tail, 0u);                                   /* nothing was published */

  std::uint64_t awaited = 0u;
  CHECK_STATUS(fixture.submit(&first_context, SubmissionDescriptor(1u, 0u, 32u), awaited), Status::ok);
  fixture.complete_through(0u, 6u);
  fixture.serve();
  const std::uint32_t rings = fixture.mapped().function().doorbells().rings(0u);
  fixture.platform().set_irql(Irql::dispatch);
  fixture.table().kick(&first_context);
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.mapped().function().doorbells().rings(0u), rings);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok);
  fixture.platform().set_irql(Irql::dispatch);
  fixture.table().kick(&first_context);
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.mapped().function().doorbells().rings(0u), rings); }

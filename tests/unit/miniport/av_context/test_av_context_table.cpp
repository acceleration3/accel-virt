#include "miniport/ddi/dxgk_allocation_info_record.hpp"
#include "miniport/ddi/dxgk_build_paging_buffer_map_aperture_segment_record.hpp"
#include "miniport/ddi/dxgk_build_paging_buffer_unmap_aperture_segment_record.hpp"
#include "miniport/ddi/dxgkarg_create_allocation_record.hpp"
#include "miniport/memory/allocation.hpp"
#include "miniport/memory/allocation_table.hpp"
#include "miniport/memory/shared_object_table.hpp"
#include "miniport/memory/aperture_segment.hpp"
#include "miniport/memory/host_window.hpp"
#include "miniport/memory/memory_segments.hpp"
#include "miniport/driver/interrupt_notification.hpp"
#include "miniport/av_context/av_context_table.hpp"
#include "miniport/scheduling/hw_queue_submissions.hpp"
#include "miniport/scheduling/execution_node.hpp"
#include "core/support/status.hpp"
#include "miniport/virtio/virtio_gpu_device.hpp"
#include "harness/virtio/fake_register_bytes.hpp"
#include "harness/virtio/fake_virtqueue_device.hpp"
#include "harness/virtio/mapped_virtio_function.hpp"
#include "harness/dxgkrnl/fake_handle_table.hpp"
#include "core/trace/state_writer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/violation.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/gl_context_options.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/escape/av_context_record.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/escape/context_views_record.hpp"
#include "proto/escape/host_window_record.hpp"
#include "proto/resources/host_blob_id.hpp"
#include "proto/resources/host_memory_request.hpp"
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
    static void set_held(AvContextTable &table, std::uint32_t slot, std::uint32_t held)
    {
        table.held_chunks_[slot] = held;
    }
    static ContextChunk *add_chunk(AvContextTable &table, std::uint32_t slot, std::uint32_t resource, void *mapping)
    {
        ContextChunk *made = nullptr;
        static_cast<void>(table.chunks_.create(made, slot, resource, 0u, 1u, mapping));
        return made;
    }
    static void drop_chunk(AvContextTable &table, ContextChunk *chunk, std::uint32_t slot)
    {
        static_cast<void>(table.chunks_.destroy(chunk));
        table.held_chunks_[slot]--;
    }
    static void take_id(AvContextTable &table)
    {
        std::uint32_t id = 0u;
        static_cast<void>(table.ids_.take(id));
    }
    static void grow_ids(AvContextTable &table)
    {
        std::uint32_t id = 0u;
        static_cast<void>(table.ids_.take(id));
        table.ids_.give_back(id);
    }
    static const Allocation *swap_name(AvContextTable &table, std::uint32_t slot, const Allocation *name)
    {
        const Allocation *const was = table.names_[slot];
        table.names_[slot] = name;
        return was;
    }
    static ObjectStorage<AvContextBinding> &binding(AvContextTable &table, std::uint32_t slot)
    {
        return table.bindings_[slot];
    }
    static void set_wake(AvContextTable &table, std::uint32_t slot, void *wake) { table.wakes_[slot] = wake; }
    static void set_raised(AvContextTable &table, std::uint32_t slot, std::uint64_t raised) { table.raised_[slot] = raised; }
    static void set_told_lost(AvContextTable &table, std::uint32_t slot, bool told) { table.told_lost_[slot] = told; }
    static ObjectStorage<ContextMemory> &memory(AvContextTable &table, std::uint32_t slot)
    {
        return table.memories_[slot];
    }
};

struct ContextMemoryProbe {
    static GuestPages &fence(ContextMemory &memory) { return memory.fence_; }
};

}

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

class Host {
public:
    explicit Host(av::MappedVirtioFunction &mapped) : queue_(mapped.queue_device(0u)), platform_(&mapped.platform())
    { mapped.platform().on_stall(&Host::stall, this); }

    void                 refuse_at(int index) { refuse_at_ = index; }
    void                 refuse_from(int index) { refuse_from_ = index; }
    void                 refuse_next(std::uint32_t type) { refuse_type_ = type; }
    void                 answer_map_with(std::uint32_t map_info) { map_info_ = map_info; }
    void                 delay_next(std::uint32_t type, std::uint64_t ticks)
    {
        delay_type_ = type;
        delay_ = ticks;
    }
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
        if (asked == host->delay_type_) {
            const std::uint64_t now = host->platform_->interrupt_time();
            if (!host->delaying_) host->delayed_since_ = now;
            host->delaying_ = true;
            if (now - host->delayed_since_ < host->delay_) return;
            host->delay_type_ = 0u;
            host->delaying_ = false;
        }
        if (asked == unref_type && host->window_ != nullptr) host->pages_when_released_ = host->window_->pages_in_use();
        if (asked == unmap_blob_type) host->views_when_unmapped_ = host->platform_->process_mappings();
        GpuResponse answer;
        if (asked == map_blob_type)
            answer.set_map_info(host->map_info_);
        else
            answer.set(GpuResponse::ok_nodata);
        if (static_cast<int>(host->answered_) == host->refuse_at_ || asked == host->refuse_type_ ||
            (host->refuse_from_ >= 0 && static_cast<int>(host->answered_) >= host->refuse_from_)) {
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
    int                     refuse_from_ = -1;
    std::uint32_t           refuse_type_ = 0u;
    std::uint32_t           map_info_ = GpuResponse::map_cache_cached;
    std::uint32_t           delay_type_ = 0u;
    std::uint64_t           delay_ = 0u;
    std::uint64_t           delayed_since_ = 0u;
    bool                    delaying_ = false;
    std::uint32_t           pages_when_released_ = UINT32_MAX;
    std::uint32_t           views_when_unmapped_ = UINT32_MAX;
};

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
    av::ContextMemory &memory(std::uint32_t slot = 0u)
    {
        return AvContextTableProbe::memory(table_, slot).object();
    }
    std::uint8_t *ring_memory(std::uint32_t slot = 0u) { return memory(slot).pages().ring_view(); }
    std::uint8_t *fence_page(std::uint32_t slot = 0u)
    {
        return const_cast<std::uint8_t *>(memory(slot).pages().fence_view());
    }
    void complete_through(std::uint64_t completed, std::uint32_t fault = 0u)
    {
        const FencePage written = fault == 0u ? FencePage(completed, completed) : FencePage(completed, completed, fault);
        static_cast<void>(written.encode(fence_page(), FencePage::encoded_size));
    }
    void raise(std::uint64_t completed, std::uint64_t progress, std::uint64_t raised)
    {
        static_cast<void>(FencePage(completed, progress).raising(raised).encode(fence_page(), FencePage::encoded_size));
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

}

TEST(av_context_table_bind, sets_the_host_context_up_on_memory_the_driver_owns)
{ Fixture fixture;
  Host host(fixture.mapped());
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  CHECK(fixture.table().invariants_hold());
  CHECK_EQ(fixture.table().bound(), 1u);
  CHECK_EQ(fixture.table().doorbell_of(&first_context), 0u);
  CHECK_EQ(fixture.table().doorbell_of(&second_context), av::AvContextTable::max_contexts);
  CHECK_EQ(host.count_of(ctx_create_type), 1u);
  CHECK_EQ(host.count_of(create_blob_type), 3u);
  CHECK_EQ(host.count_of(submit_3d_type), 1u);
  const av::ContextPages pages = fixture.memory().pages();
  CHECK_EQ(fixture.platform().owned_page_blocks(), 3u);
  CHECK_EQ(host.field(ctx_create_type, 0u, 16u), 1u);
  CHECK_EQ(host.field(create_blob_type, 0u, 24u), av::AvContextBinding::first_resource_id);
  CHECK_EQ(host.field(create_blob_type, 0u, 36u), 1u);
  CHECK_EQ(host.field(create_blob_type, 0u, 56u), static_cast<std::uint32_t>(pages.ring().frame(0u)));
  CHECK_EQ(host.field(create_blob_type, 1u, 56u), static_cast<std::uint32_t>(pages.fence().frame(0u)));
  CHECK_EQ(host.field(create_blob_type, 2u, 56u), static_cast<std::uint32_t>(pages.reply().frame(0u)));
  av::RingHeader header;
  CHECK_STATUS(header.decode(fixture.ring_memory(), av::RingHeader::encoded_size), Status::ok);
  CHECK_EQ(header.order(), 12u);
  CHECK_EQ(fixture.mapped().common().queue(2u).vector, 3u);
  CHECK_EQ(fixture.platform().mapped_frames(), 2u);
  CHECK_EQ(fixture.platform().process_mappings(), 3u);
  CHECK_EQ(fixture.platform().read_only_process_mappings(), 1u);
  CHECK(fixture.platform().maps_into_process(fixture.memory().fence_view(), pages.fence().frame(0u), page));
  CHECK(fixture.platform().maps_into_process(fixture.memory().ring_view(), pages.ring().frame(0u), 2u * page));
  CHECK(fixture.platform().maps_into_process(fixture.memory().reply_view(), pages.reply().frame(0u), page));
  CHECK(fixture.platform().logged_line("av context on doorbell 0 message 3 status 0x00000000"));
  CHECK_EQ(fixture.handles().outstanding(), 0u);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok);
  CHECK_EQ(fixture.platform().mapped_frames(), 0u);
  CHECK_EQ(fixture.platform().process_mappings(), 0u);
  CHECK_EQ(fixture.platform().owned_page_blocks(), 0u); }

TEST(av_context_table_views, answers_the_process_where_its_ring_reply_slot_and_fence_page_are)
{ Fixture fixture;
  Host host(fixture.mapped());
  av::ContextViewsRecord answer;
  CHECK_STATUS(fixture.table().views(&first_context, answer), Status::invalid_parameter);
  CHECK_EQ(answer.ring(), 0u);
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  CHECK_STATUS(fixture.table().views(&first_context, answer), Status::ok);
  CHECK_EQ(answer.ring(), fixture.memory().ring_view());
  CHECK_EQ(answer.reply(), fixture.memory().reply_view());
  CHECK_EQ(answer.fence(), fixture.memory().fence_view());
  av::ContextViewsRecord older(av::ContextViewsRecord::without_fence);
  CHECK_STATUS(fixture.table().views(&first_context, older), Status::ok);
  CHECK_EQ(older.ring(), fixture.memory().ring_view());
  CHECK_EQ(older.fence(), 0u);
  at(answer.ring())[av::RingHeader::encoded_size] = 0x5Au;
  CHECK_EQ(fixture.ring_memory()[av::RingHeader::encoded_size], 0x5Au);
  CHECK_STATUS(fixture.table().views(&second_context, answer), Status::invalid_parameter);
  fixture.table().abandon_all();
  av::ContextViewsRecord lost;
  CHECK_STATUS(fixture.table().views(&first_context, lost), Status::invalid_parameter);
  CHECK_EQ(lost.ring(), 0u);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

TEST(av_context_table_wake, the_event_is_set_once_for_each_serve_that_finds_a_raise)
{ Fixture fixture;
  Host host(fixture.mapped());
  const std::uint64_t event = fixture.platform().make_process_event();
  CHECK_STATUS(fixture.table().wake_on_raise(&first_context, event), Status::invalid_parameter);
  CHECK_EQ(fixture.platform().event_references(event), 0u);
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  CHECK_STATUS(fixture.table().wake_on_raise(&first_context, event + 4u), Status::invalid_parameter);
  CHECK_STATUS(fixture.table().wake_on_raise(&first_context, event), Status::ok);
  fixture.platform().close_process_event(event);
  CHECK_EQ(fixture.platform().event_references(event), 1u);
  fixture.raise(0u, 0u, 0u);
  fixture.serve();
  CHECK_EQ(fixture.platform().event_signals(event), 0u);
  fixture.raise(0u, 1u, 1u);
  fixture.serve();
  CHECK_EQ(fixture.platform().event_signals(event), 1u);
  fixture.serve();
  CHECK_EQ(fixture.platform().event_signals(event), 1u);
  fixture.raise(0u, 3u, 3u);
  fixture.serve();
  CHECK_EQ(fixture.platform().event_signals(event), 2u);
  fixture.raise(0u, 4u, 3u);
  fixture.serve();
  CHECK_EQ(fixture.platform().event_signals(event), 2u);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok);
  CHECK_EQ(fixture.platform().event_signals(event), 3u);
  CHECK_EQ(fixture.platform().event_references(event), 0u); }

TEST(av_context_table_wake, a_lost_or_abandoned_context_sets_the_event)
{ Fixture fixture;
  Host host(fixture.mapped());
  const std::uint64_t event = fixture.platform().make_process_event();
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  CHECK_STATUS(fixture.table().wake_on_raise(&first_context, event), Status::ok);
  fixture.complete_through(5u);
  fixture.serve();
  CHECK_EQ(fixture.platform().event_signals(event), 1u);
  fixture.serve();
  CHECK_EQ(fixture.platform().event_signals(event), 1u);
  fixture.table().abandon_all();
  CHECK_EQ(fixture.platform().event_signals(event), 2u);
  CHECK_STATUS(fixture.table().wake_on_raise(&first_context, event), Status::invalid_parameter);
  CHECK_EQ(fixture.platform().event_references(event), 1u);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok);
  CHECK_EQ(fixture.platform().event_references(event), 0u); }

TEST(av_context_table_wake, a_second_event_replaces_the_first_and_contexts_may_share_one)
{ Fixture fixture;
  Host host(fixture.mapped());
  const std::uint64_t first = fixture.platform().make_process_event();
  const std::uint64_t second = fixture.platform().make_process_event();
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  CHECK_STATUS(fixture.table().wake_on_raise(&first_context, first), Status::ok);
  CHECK_STATUS(fixture.table().wake_on_raise(&first_context, second), Status::ok);
  CHECK_EQ(fixture.platform().event_references(first), 0u);
  CHECK_EQ(fixture.platform().event_references(second), 1u);
  CHECK_STATUS(fixture.table().wake_on_raise(&first_context, second), Status::ok);
  CHECK_EQ(fixture.platform().event_references(second), 1u);
  fixture.raise(0u, 1u, 1u);
  fixture.serve();
  CHECK_EQ(fixture.platform().event_signals(first), 0u);
  CHECK_EQ(fixture.platform().event_signals(second), 1u);
  ProcessAllocation other(fixture.platform(), fixture.allocations(), fixture.handles(), AllocationKind::context_ring,
                         page);
  CHECK_STATUS(fixture.bind(&second_context, AvContextRecord(SubApi::vulkan, 12u, other.handle(), page)), Status::ok);
  CHECK_STATUS(fixture.table().wake_on_raise(&second_context, second), Status::ok);
  CHECK_EQ(fixture.platform().event_references(second), 2u);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok);
  CHECK_EQ(fixture.platform().event_references(second), 1u);
  CHECK_STATUS(fixture.table().unbind(&second_context, &fixture.device()), Status::ok);
  CHECK_EQ(fixture.platform().event_references(second), 0u); }

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
  CHECK_EQ(fixture.node().current_fence(), 0u);
  fixture.complete_through(1u);
  fixture.serve();
  CHECK_EQ(fixture.node().current_fence(), 5u);
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

TEST(av_context_table_submit, refuses_what_is_not_a_descriptor_of_a_live_context_s_ring)
{ Fixture fixture;
  Host host(fixture.mapped());
  std::uint64_t awaited = 7u;
  CHECK_STATUS(fixture.submit(&first_context, SubmissionDescriptor(1u, 0u, 32u), awaited), Status::invalid_parameter);
  CHECK_EQ(awaited, 0u);
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  CHECK_STATUS(fixture.submit(&first_context, SubmissionDescriptor(1u, 0u, 5000u), awaited),
               Status::invalid_parameter);
  std::uint8_t bytes[SubmissionDescriptor::encoded_size] = {};
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_STATUS(fixture.table().submit(&first_context, bytes, sizeof bytes, awaited), Status::invalid_parameter);
  CHECK_STATUS(fixture.table().submit(&first_context, nullptr, 0u, awaited), Status::invalid_parameter);
  CHECK_STATUS(fixture.table().submit(&first_context, bytes, 8u, awaited), Status::invalid_parameter);
  fixture.platform().set_irql(Irql::passive);
  CHECK_STATUS(fixture.submit(&first_context, SubmissionDescriptor(1u, 8u, 32u), awaited), Status::invalid_parameter);
  CHECK_EQ(fixture.mapped().function().doorbells().rings(0u), 0u);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

TEST(av_context_table_bind, refuses_a_record_that_does_not_name_the_context_by_a_ring_of_the_process)
{ Fixture fixture;
  Host host(fixture.mapped());
  ProcessAllocation client(fixture.platform(), fixture.allocations(), fixture.handles(), AllocationKind::client,
                           page);
  const AvContextRecord cases[] = {
      AvContextRecord(SubApi::vulkan, 12u, 0x7777u, page),
      AvContextRecord(SubApi::vulkan, 12u, client.handle(), page),
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

TEST(av_context_table_bind, a_name_vidmm_never_mapped_or_evicted_still_binds)
{ Fixture fixture;
  Host host(fixture.mapped());
  CHECK_STATUS(fixture.bind(&first_context, fixture.record(20u)), Status::ok);
  CHECK_EQ(fixture.memory().pages().ring().count(), 257u);
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
  CHECK_STATUS(fixture.bind(&second_context), Status::invalid_parameter);
  av::FakeRegisterBytes one_page(0x800u);
  CHECK_STATUS(fixture.table().bind(&second_context, fixture.record(), fixture.device(), one_page, capset()),
               Status::no_space);
  CHECK_EQ(fixture.table().bound(), 1u);
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

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

TEST(av_context_table_bind, a_setup_the_device_refuses_is_undone)
{ Fixture fixture;
  Host host(fixture.mapped());
  host.refuse_at(2);
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

TEST(av_context_table_bind, a_setup_the_device_never_answers_keeps_its_pages_out_of_use)
{ Fixture fixture;
  Host host(fixture.mapped());
  host.delay_next(ctx_create_type, ~std::uint64_t{0});
  CHECK_STATUS(fixture.bind(&first_context), Status::not_supported);
  CHECK_EQ(fixture.table().bound(), 0u);
  CHECK_EQ(fixture.platform().process_mappings(), 0u);
  CHECK_EQ(fixture.platform().mapped_frames(), 0u);
  CHECK_EQ(fixture.platform().owned_page_blocks(), 3u);
  CHECK(fixture.table().invariants_hold());
  CHECK_EQ(fixture.platform().reclaim_page_blocks(), 3u); }

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
  CHECK_STATUS(fixture.table().unbind(&first_context, nullptr), Status::ok);
  CHECK_EQ(host.queue().requests(), asked);
  CHECK_EQ(fixture.platform().mapped_frames(), 0u);
  CHECK_EQ(fixture.platform().process_mappings(), 0u);
  CHECK_EQ(fixture.platform().owned_page_blocks(), 0u);
  CHECK(fixture.table().invariants_hold()); }

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
  CHECK_EQ(fixture.node().current_fence(), 0u);
  const std::uint32_t asked = host.queue().requests();
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok);
  CHECK_EQ(host.queue().requests(), asked);
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
  AvContextTableProbe::set_live(table, 5u, true);
  CHECK(!table.invariants_hold());
  AvContextTableProbe::set_live(table, 5u, false);
  static_cast<void>(AvContextTableProbe::memory(table, 6u).construct(fixture.platform()));
  CHECK(!table.invariants_hold());
  AvContextTableProbe::memory(table, 6u).destroy();
  AvContextTableProbe::swap_name(table, 6u, fixture.ring().allocation());
  CHECK(!table.invariants_hold());
  AvContextTableProbe::swap_name(table, 6u, nullptr);
  AvContextTableProbe::set_context(table, 7u, &second_context);
  AvContextTableProbe::swap_name(table, 7u, fixture.ring().allocation());
  CHECK(table.invariants_hold());
  AvContextTableProbe::set_live(table, 7u, true);
  CHECK(!table.invariants_hold());
  AvContextTableProbe::set_live(table, 7u, false);
  AvContextTableProbe::swap_name(table, 7u, nullptr);
  CHECK(!table.invariants_hold());
  AvContextTableProbe::set_context(table, 7u, nullptr);
  CHECK(table.invariants_hold());
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  CHECK(table.invariants_hold());
  const Allocation *const name = AvContextTableProbe::swap_name(table, 0u, nullptr);
  CHECK(!table.invariants_hold());
  AvContextTableProbe::swap_name(table, 0u, name);
  av::ContextMemory &memory = fixture.memory();
  memory.release();
  CHECK(!table.invariants_hold());
  CHECK_STATUS(memory.allocate(12u, 4096u), Status::ok);
  CHECK(!table.invariants_hold());
  CHECK_STATUS(memory.map_into_process(), Status::ok);
  CHECK(table.invariants_hold());
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

TEST(av_context_table_bind, a_handle_to_something_that_is_not_an_allocation_is_refused)
{ Fixture fixture;
  Host host(fixture.mapped());
  int not_an_allocation = 0;
  const std::uint32_t handle = fixture.handles().insert(&not_an_allocation);
  const AvContextRecord record(SubApi::vulkan, 12u, handle, page);
  CHECK_STATUS(fixture.bind(&first_context, record), Status::invalid_parameter);
  CHECK_EQ(fixture.handles().outstanding(), 0u);
  CHECK_STATUS(fixture.table().unbind(nullptr, &fixture.device()), Status::invalid_parameter); }

TEST(av_context_table_unbind, a_context_failed_by_its_fence_page_is_still_destroyed_on_the_device)
{ Fixture fixture;
  Host host(fixture.mapped());
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  fixture.complete_through(5u);
  fixture.serve();
  std::uint64_t awaited = 0u;
  CHECK_STATUS(fixture.submit(&first_context, SubmissionDescriptor(1u, 0u, 32u), awaited), Status::invalid_parameter);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok);
  CHECK_EQ(host.count_of(ctx_destroy_type), 1u);
  CHECK_EQ(host.count_of(unref_type), 3u); }

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
  fixture.complete_through(1u, 6u);
  fixture.serve();
  CHECK(fixture.table().invariants_hold());
  CHECK_EQ(fixture.node().current_fence(), 11u);
  CHECK_EQ(fixture.node().pending(), 2u);
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

TEST(av_context_table_serve, a_page_no_device_could_write_loses_the_context_too)
{ Fixture fixture;
  Host host(fixture.mapped());
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  std::uint64_t awaited = 0u;
  CHECK_STATUS(fixture.submit(&first_context, SubmissionDescriptor(1u, 0u, 32u), awaited), Status::ok);
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_STATUS(fixture.node().submit(3u, &first_context, 1u), Status::ok);
  fixture.platform().set_irql(Irql::passive);
  fixture.complete_through(9u);
  fixture.serve();
  CHECK_EQ(fixture.node().pending(), 0u);
  CHECK_EQ(fixture.node().current_fence(), 3u);
  CHECK_STATUS(fixture.submit(&first_context, SubmissionDescriptor(2u, 32u, 64u), awaited), Status::invalid_parameter);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

namespace {

using av::HostBlobId;
using av::HostMemoryRequest;
using av::HostWindowRecord;
using WindowOperation = av::HostWindowRecord::Operation;

constexpr std::uint32_t chunk_base = HostBlobId::first;
constexpr std::uint64_t window_base = 0x4000000000ull;
constexpr std::uint64_t window_pages = 16u;
constexpr std::uint32_t context_views = 3u;
constexpr std::uint32_t context_blocks = 3u;
constexpr std::uint32_t blob_mem_host3d = 2u;
const HostMemoryRequest upload(HostMemoryRequest::Kind::upload, 0u);

Status open_window(Fixture &fixture, void *context, std::uint64_t bytes, HostWindowRecord &answer,
                   HostMemoryRequest memory = upload)
{
    answer = HostWindowRecord(WindowOperation::open, bytes, 0u, memory);
    return fixture.table().window_chunk(context, answer, fixture.device());
}

Status close_window(Fixture &fixture, void *context, std::uint32_t resource)
{
    HostWindowRecord record(WindowOperation::close, 0u, resource, HostMemoryRequest());
    return fixture.table().window_chunk(context, record, fixture.device());
}

void bind_with_window(Fixture &fixture)
{
    CHECK_STATUS(fixture.window().initialize(window_base, window_pages * page), Status::ok);
    CHECK_STATUS(fixture.bind(&first_context), Status::ok);
}

std::uint64_t field64(Host &host, std::uint32_t type, std::uint32_t nth, std::size_t offset)
{
    return std::uint64_t{host.field(type, nth, offset)} | std::uint64_t{host.field(type, nth, offset + 4u)} << 32u;
}

void holds_nothing_new(Fixture &fixture)
{
    CHECK_EQ(fixture.window().pages_in_use(), 0u);
    CHECK_EQ(fixture.platform().process_mappings(), context_views);
    CHECK_EQ(fixture.platform().owned_page_blocks(), context_blocks);
    CHECK(fixture.table().invariants_hold());
}

}

TEST(av_context_table_window_chunk, an_open_places_a_host_blob_in_the_window_and_maps_it_into_the_process)
{ Fixture fixture;
  Host host(fixture.mapped());
  bind_with_window(fixture);
  const HostMemoryRequest vulkan(HostMemoryRequest::Kind::vulkan, 3u);
  HostWindowRecord answer;
  CHECK_STATUS(open_window(fixture, &first_context, 0x1800u, answer, vulkan), Status::ok);
  CHECK_EQ(answer.resource_id(), chunk_base);
  CHECK_EQ(answer.bytes(), 0x2000u);
  CHECK(answer.address() != 0u);
  CHECK(fixture.platform().maps_into_process(answer.address(), window_base, 0x2000u));
  CHECK_EQ(host.field(create_blob_type, 3u, 16u), 1u);
  CHECK_EQ(host.field(create_blob_type, 3u, 24u), chunk_base);
  CHECK_EQ(host.field(create_blob_type, 3u, 28u), blob_mem_host3d);
  CHECK_EQ(host.field(create_blob_type, 3u, 32u), 1u);
  CHECK_EQ(host.field(create_blob_type, 3u, 36u), 0u);
  CHECK_EQ(field64(host, create_blob_type, 3u, 40u), vulkan.encoded());
  CHECK_EQ(field64(host, create_blob_type, 3u, 48u), 0x2000u);
  CHECK_EQ(host.field(map_blob_type, 0u, 24u), chunk_base);
  CHECK_EQ(field64(host, map_blob_type, 0u, 32u), 0u);
  CHECK_EQ(fixture.window().pages_in_use(), 2u);
  CHECK_EQ(fixture.platform().owned_page_blocks(), context_blocks);
  CHECK_EQ(fixture.platform().write_combined_process_mappings(), 0u);

  HostWindowRecord second;
  CHECK_STATUS(open_window(fixture, &first_context, page, second), Status::ok);
  CHECK_EQ(second.resource_id(), chunk_base + 1u);
  CHECK_EQ(field64(host, map_blob_type, 1u, 32u), 2u * page);
  CHECK(fixture.platform().maps_into_process(second.address(), window_base + 2u * page, page));
  CHECK_EQ(fixture.platform().process_mappings(), context_views + 2u);

  host.watch(fixture.window());
  CHECK_STATUS(close_window(fixture, &first_context, chunk_base), Status::ok);
  CHECK_EQ(host.field(unmap_blob_type, 0u, 24u), chunk_base);
  CHECK_EQ(host.field(unref_type, 0u, 24u), chunk_base);
  CHECK_EQ(host.views_when_unmapped(), context_views + 1u);
  CHECK_EQ(host.pages_when_released(), 3u);
  CHECK_EQ(fixture.window().pages_in_use(), 1u);
  CHECK_EQ(fixture.platform().process_mappings(), context_views + 1u);
  CHECK(!fixture.platform().maps_into_process(answer.address(), window_base, 0x2000u));

  HostWindowRecord again;
  CHECK_STATUS(open_window(fixture, &first_context, page, again), Status::ok);
  CHECK_EQ(again.resource_id(), chunk_base);
  CHECK_EQ(field64(host, map_blob_type, 2u, 32u), 0u);
  CHECK(fixture.platform().maps_into_process(again.address(), window_base, page));
  CHECK(fixture.table().invariants_hold());

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

TEST(av_context_table_window_chunk, the_process_maps_a_chunk_with_the_caching_the_device_answers)
{ Fixture fixture;
  Host host(fixture.mapped());
  bind_with_window(fixture);
  host.answer_map_with(GpuResponse::map_cache_wc);
  HostWindowRecord combined;
  CHECK_STATUS(open_window(fixture, &first_context, page, combined), Status::ok);
  CHECK_EQ(fixture.platform().write_combined_process_mappings(), 1u);
  host.answer_map_with(GpuResponse::map_cache_cached);
  HostWindowRecord cached;
  CHECK_STATUS(open_window(fixture, &first_context, page, cached), Status::ok);
  CHECK_EQ(fixture.platform().write_combined_process_mappings(), 1u);
  CHECK_EQ(fixture.platform().process_mappings(), context_views + 2u);
  CHECK_STATUS(close_window(fixture, &first_context, combined.resource_id()), Status::ok);
  CHECK_EQ(fixture.platform().write_combined_process_mappings(), 0u);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

TEST(av_context_table_window_chunk, refuses_what_it_cannot_serve_and_holds_nothing_after)
{ Fixture fixture;
  Host host(fixture.mapped());
  HostWindowRecord answer;
  CHECK_STATUS(open_window(fixture, &first_context, page, answer), Status::invalid_parameter);
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  const std::uint32_t asked = host.queue().requests();
  CHECK_STATUS(open_window(fixture, &first_context, page, answer), Status::not_supported);
  CHECK_STATUS(fixture.window().initialize(window_base, window_pages * page), Status::ok);
  CHECK_STATUS(open_window(fixture, &first_context, (window_pages + 1u) * page, answer), Status::no_space);
  CHECK_STATUS(open_window(fixture, &first_context, (std::uint64_t{UINT32_MAX} + 1u) * page, answer),
               Status::no_space);
  CHECK_STATUS(open_window(fixture, &first_context, UINT64_MAX, answer), Status::no_space);
  CHECK_STATUS(open_window(fixture, &first_context, page, answer, HostMemoryRequest()), Status::invalid_parameter);
  CHECK_STATUS(close_window(fixture, &first_context, chunk_base), Status::invalid_parameter);
  CHECK_EQ(host.queue().requests(), asked);
  holds_nothing_new(fixture);
  CHECK_STATUS(open_window(fixture, &first_context, page, answer), Status::ok);
  CHECK_EQ(answer.resource_id(), chunk_base);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

TEST(av_context_table_window_chunk, a_step_that_fails_is_undone_in_reverse)
{ struct Case {
      const char   *name;
      std::uint32_t refuse;
      std::uint32_t map_info;
      bool          no_view;
      bool          no_memory;
      Status        expected;
      std::uint32_t unrefs;
      std::uint32_t unmaps;
  };
  const Case cases[] = {
      {"create refused", create_blob_type, GpuResponse::map_cache_cached, false, false, Status::not_supported, 0u, 0u},
      {"map refused", map_blob_type, GpuResponse::map_cache_cached, false, false, Status::not_supported, 1u, 0u},
      {"no caching a process can have", 0u, 0u, false, false, Status::malformed, 1u, 0u},
      {"no view", 0u, GpuResponse::map_cache_cached, true, false, Status::no_memory, 1u, 1u},
      {"no memory for the chunk", 0u, GpuResponse::map_cache_cached, false, true, Status::no_memory, 1u, 1u},
  };
  for (const Case &one : cases) {
      Fixture fixture;
      Host    host(fixture.mapped());
      bind_with_window(fixture);
      if (one.refuse != 0u) host.refuse_next(one.refuse);
      host.answer_map_with(one.map_info);
      if (one.no_view) fixture.platform().fail_process_map_at(1u);
      if (one.no_memory) {
          AvContextTableProbe::grow_ids(fixture.table());
          fixture.platform().fail_allocation_at(1u);
      }
      HostWindowRecord answer;
      CHECK_STATUS(open_window(fixture, &first_context, 2u * page, answer), one.expected);
      CHECK_EQ(answer.resource_id(), 0u);
      CHECK_EQ(host.count_of(unref_type), one.unrefs);
      CHECK_EQ(host.count_of(unmap_blob_type), one.unmaps);
      holds_nothing_new(fixture);
      host.answer_map_with(GpuResponse::map_cache_cached);
      CHECK_STATUS(open_window(fixture, &first_context, page, answer), Status::ok);
      CHECK_EQ(answer.resource_id(), chunk_base);
      CHECK(fixture.platform().maps_into_process(answer.address(), window_base, page));
      CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok);
  } }

TEST(av_context_table_window_chunk, pages_and_ids_the_device_did_not_let_go_of_are_never_handed_out_again)
{ Fixture fixture;
  Host host(fixture.mapped());
  bind_with_window(fixture);
  fixture.platform().fail_process_map_at(1u);
  host.refuse_next(unref_type);
  HostWindowRecord answer;
  CHECK_STATUS(open_window(fixture, &first_context, page, answer), Status::no_memory);
  CHECK_EQ(fixture.window().pages_in_use(), 1u);
  CHECK_EQ(fixture.platform().process_mappings(), context_views);
  HostWindowRecord windowed;
  CHECK_STATUS(open_window(fixture, &first_context, page, windowed), Status::ok);
  CHECK_EQ(windowed.resource_id(), chunk_base + 1u);
  CHECK(fixture.platform().maps_into_process(windowed.address(), window_base + page, page));
  host.refuse_next(unmap_blob_type);
  CHECK_STATUS(close_window(fixture, &first_context, chunk_base + 1u), Status::ok);
  CHECK_EQ(fixture.platform().process_mappings(), context_views);
  CHECK_EQ(fixture.window().pages_in_use(), 2u);
  CHECK_STATUS(close_window(fixture, &first_context, chunk_base + 1u), Status::invalid_parameter);
  CHECK_STATUS(open_window(fixture, &first_context, page, windowed), Status::ok);
  CHECK_EQ(windowed.resource_id(), chunk_base + 2u);
  CHECK(fixture.platform().maps_into_process(windowed.address(), window_base + 2u * page, page));
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok);
  CHECK_EQ(fixture.window().pages_in_use(), 2u);
  CHECK_EQ(fixture.platform().owned_page_blocks(), 0u); }

TEST(av_context_table_window_chunk, a_create_the_host_takes_long_over_is_waited_for)
{ Fixture fixture;
  Host host(fixture.mapped());
  bind_with_window(fixture);
  constexpr std::uint64_t quarter_second = 2500000u;
  host.delay_next(create_blob_type, quarter_second);
  const std::uint64_t asked = fixture.platform().interrupt_time();
  HostWindowRecord answer;
  CHECK_STATUS(open_window(fixture, &first_context, page, answer), Status::ok);
  CHECK(fixture.platform().interrupt_time() - asked >= quarter_second);
  CHECK_EQ(answer.resource_id(), chunk_base);
  CHECK_EQ(host.count_of(unref_type), 0u);
  CHECK_STATUS(close_window(fixture, &first_context, chunk_base), Status::ok);
  CHECK_STATUS(open_window(fixture, &first_context, page, answer), Status::ok);
  CHECK_EQ(answer.resource_id(), chunk_base);
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

TEST(av_context_table_window_chunk, a_create_the_device_never_answers_keeps_its_id_and_pages)
{ Fixture fixture;
  Host host(fixture.mapped());
  bind_with_window(fixture);
  host.delay_next(create_blob_type, ~std::uint64_t{0});
  HostWindowRecord answer;
  CHECK_STATUS(open_window(fixture, &first_context, page, answer), Status::not_found);
  CHECK_EQ(answer.resource_id(), 0u);
  CHECK_EQ(host.count_of(unref_type), 0u);
  CHECK_EQ(fixture.window().pages_in_use(), 1u);
  CHECK_EQ(fixture.platform().process_mappings(), context_views);
  CHECK(fixture.table().invariants_hold());
  host.delay_next(0u, 0u);
  CHECK_EQ(host.queue().respond_all(0x1100u), 1u);
  HostWindowRecord windowed;
  CHECK_STATUS(open_window(fixture, &first_context, page, windowed), Status::ok);
  CHECK_EQ(windowed.resource_id(), chunk_base + 1u);
  CHECK(fixture.platform().maps_into_process(windowed.address(), window_base + page, page));
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

TEST(av_context_table_window_chunk, a_context_past_the_old_per_process_quota_opens_the_next)
{ Fixture fixture;
  Host host(fixture.mapped());
  bind_with_window(fixture);
  AvContextTableProbe::set_held(fixture.table(), 0u, 4097u);
  HostWindowRecord answer;
  CHECK_STATUS(open_window(fixture, &first_context, page, answer), Status::ok);
  CHECK_EQ(answer.resource_id(), chunk_base);
  CHECK(fixture.platform().claim(Violation::broken_invariant));
  CHECK(fixture.platform().claim(Violation::broken_invariant));
  AvContextTableProbe::set_held(fixture.table(), 0u, 1u);
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

TEST(av_context_table_window_chunk, ids_that_cannot_be_noted_refuse_the_open_as_out_of_memory)
{ Fixture fixture;
  Host host(fixture.mapped());
  bind_with_window(fixture);
  const std::uint32_t asked = host.queue().requests();
  HostWindowRecord answer;
  fixture.platform().fail_allocation_at(1u);
  CHECK_STATUS(open_window(fixture, &first_context, page, answer), Status::no_memory);
  CHECK_EQ(host.queue().requests(), asked);
  CHECK_EQ(fixture.window().pages_in_use(), 0u);
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(open_window(fixture, &first_context, page, answer), Status::ok);
  CHECK_EQ(answer.resource_id(), chunk_base);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

TEST(av_context_table_window_chunk, contexts_share_the_ids_and_close_only_their_own)
{ Fixture fixture;
  Host host(fixture.mapped());
  bind_with_window(fixture);
  ProcessAllocation other(fixture.platform(), fixture.allocations(), fixture.handles(), AllocationKind::context_ring, page);
  CHECK_STATUS(fixture.bind(&second_context, AvContextRecord(SubApi::vulkan, 12u, other.handle(), page)), Status::ok);
  HostWindowRecord first, second;
  CHECK_STATUS(open_window(fixture, &first_context, page, first), Status::ok);
  CHECK_STATUS(open_window(fixture, &second_context, page, second), Status::ok);
  CHECK_EQ(first.resource_id(), chunk_base);
  CHECK_EQ(second.resource_id(), chunk_base + 1u);
  CHECK_EQ(host.field(create_blob_type, 7u, 16u), 2u);
  CHECK_STATUS(close_window(fixture, &second_context, first.resource_id()), Status::invalid_parameter);
  CHECK_STATUS(close_window(fixture, &first_context, second.resource_id()), Status::invalid_parameter);
  CHECK_STATUS(close_window(fixture, &second_context, second.resource_id()), Status::ok);
  CHECK_STATUS(fixture.table().unbind(&second_context, &fixture.device()), Status::ok);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok);
  CHECK_EQ(fixture.window().pages_in_use(), 0u); }

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

TEST(av_context_table_window_chunk, destruction_unmaps_what_is_still_open)
{ Fixture fixture;
  Host host(fixture.mapped());
  CHECK_STATUS(fixture.window().initialize(window_base, 2u * page), Status::ok);
  { AvContextTable table(fixture.platform(), fixture.mapped().interface(), fixture.allocations(), fixture.window());
    CHECK_STATUS(table.bind(&first_context, fixture.record(), fixture.device(),
                            fixture.mapped().function().doorbells(), capset()),
                 Status::ok);
    HostWindowRecord first(WindowOperation::open, page, 0u, upload);
    CHECK_STATUS(table.window_chunk(&first_context, first, fixture.device()), Status::ok);
    HostWindowRecord second(WindowOperation::open, page, 0u, upload);
    CHECK_STATUS(table.window_chunk(&first_context, second, fixture.device()), Status::ok);
    CHECK_EQ(fixture.platform().process_mappings(), context_views + 2u); }
  CHECK_EQ(fixture.platform().process_mappings(), 0u);
  CHECK_EQ(fixture.platform().mapped_frames(), 0u);
  CHECK_EQ(fixture.window().pages_in_use(), 0u);
  CHECK_EQ(fixture.platform().owned_page_blocks(), 0u); }

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
  HostWindowRecord answer(WindowOperation::open, page, 0u, upload);
  static_cast<void>(fixture.table().window_chunk(&first_context, answer, fixture.device()));
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().claim(Violation::irql_ceiling));
  while (fixture.platform().claim(Violation::irql_ceiling)) {}
  while (fixture.platform().claim(Violation::blocking_at_dispatch)) {}
  while (fixture.platform().claim(Violation::lock_order)) {}
  while (fixture.platform().claim(Violation::release_order)) {} }

namespace {

constexpr std::uint32_t attach_type = 0x0106u;
constexpr std::uint32_t blob_mem_host3d_guest = 3u;
constexpr std::uint64_t lent_at = 0x7FF000000000ull;
const HostMemoryRequest readback(HostMemoryRequest::Kind::readback, 0u);
const std::uint64_t     lent_frames[3] = {0x50000u, 0x51000u, 0x70000u};

Status adopt(Fixture &fixture, void *context, std::uint64_t bytes, HostWindowRecord &answer)
{
    answer = HostWindowRecord(lent_at, bytes, readback);
    return fixture.table().window_chunk(context, answer, fixture.device());
}

}

TEST(av_context_table_adopt, lent_pages_become_a_blob_the_device_makes_over_them_and_come_back_on_close)
{ Fixture fixture;
  Host host(fixture.mapped());
  bind_with_window(fixture);
  fixture.platform().lend_process_range(lent_at, lent_frames, 3u);
  HostWindowRecord answer;
  CHECK_STATUS(adopt(fixture, &first_context, 3u * page, answer), Status::ok);
  CHECK(answer.adopts());
  CHECK_EQ(answer.resource_id(), chunk_base);
  CHECK_EQ(answer.bytes(), 3u * page);
  CHECK_EQ(answer.address(), lent_at);
  CHECK_EQ(fixture.platform().process_locks(), 1u);
  CHECK_EQ(host.field(create_blob_type, 3u, 16u), 1u);
  CHECK_EQ(host.field(create_blob_type, 3u, 24u), chunk_base);
  CHECK_EQ(host.field(create_blob_type, 3u, 28u), blob_mem_host3d_guest);
  CHECK_EQ(host.field(create_blob_type, 3u, 32u), 0u);
  CHECK_EQ(field64(host, create_blob_type, 3u, 40u), readback.encoded());
  CHECK_EQ(host.field(attach_type, 0u, 24u), chunk_base);
  CHECK_EQ(host.field(attach_type, 0u, 28u), 2u);
  CHECK_EQ(field64(host, attach_type, 0u, 32u), lent_frames[0]);
  CHECK_EQ(host.count_of(map_blob_type), 0u);
  CHECK_EQ(fixture.window().pages_in_use(), 0u);
  CHECK_EQ(fixture.platform().process_mappings(), context_views);
  CHECK(fixture.table().invariants_hold());
  HostWindowRecord windowed;
  CHECK_STATUS(open_window(fixture, &first_context, page, windowed), Status::ok);
  CHECK_EQ(windowed.resource_id(), chunk_base + 1u);
  CHECK_STATUS(close_window(fixture, &first_context, chunk_base), Status::ok);
  CHECK_EQ(host.field(unref_type, 0u, 24u), chunk_base);
  CHECK_EQ(host.count_of(unmap_blob_type), 0u);
  CHECK_EQ(fixture.platform().process_locks(), 0u);
  CHECK_EQ(fixture.platform().process_mappings(), context_views + 1u);
  CHECK_STATUS(adopt(fixture, &first_context, 3u * page, answer), Status::ok);
  CHECK_EQ(answer.resource_id(), chunk_base);
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok);
  CHECK_EQ(fixture.platform().process_locks(), 0u);
  CHECK_EQ(host.count_of(unref_type), 2u + 1u + 3u); }

TEST(av_context_table_adopt, a_range_too_long_or_not_the_processs_holds_nothing)
{ Fixture fixture;
  Host host(fixture.mapped());
  bind_with_window(fixture);
  const std::uint32_t asked = host.queue().requests();
  HostWindowRecord answer(lent_at, AvContextTable::max_adopted_bytes + page, readback);
  CHECK_STATUS(fixture.table().window_chunk(&first_context, answer, fixture.device()), Status::no_space);
  CHECK_STATUS(adopt(fixture, &first_context, page, answer), Status::invalid_parameter);
  CHECK_EQ(host.queue().requests(), asked);
  fixture.platform().lend_process_range(lent_at, lent_frames, 1u);
  fixture.platform().fail_allocation_at(1u);
  CHECK_STATUS(adopt(fixture, &first_context, page, answer), Status::no_memory);
  CHECK_EQ(host.queue().requests(), asked);
  CHECK_EQ(fixture.platform().process_locks(), 0u);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

TEST(av_context_table_adopt, a_step_that_fails_is_undone_and_the_pages_let_go)
{ struct Case {
      const char   *name;
      std::uint32_t refuse;
      bool          no_memory;
      Status        expected;
      std::uint32_t unrefs;
  };
  const Case cases[] = {
      {"create refused", create_blob_type, false, Status::not_supported, 0u},
      {"pages refused", attach_type, false, Status::not_supported, 1u},
      {"no room to name the pages", 0u, true, Status::no_memory, 0u},
  };
  for (const Case &one : cases) {
      Fixture fixture;
      Host    host(fixture.mapped());
      bind_with_window(fixture);
      fixture.platform().lend_process_range(lent_at, lent_frames, 2u);
      if (one.refuse != 0u) host.refuse_next(one.refuse);
      if (one.no_memory) fixture.platform().fail_allocation_at(1u);
      HostWindowRecord answer;
      CHECK_STATUS(adopt(fixture, &first_context, 2u * page, answer), one.expected);
      CHECK_EQ(answer.resource_id(), 0u);
      CHECK_EQ(host.count_of(unref_type), one.unrefs);
      CHECK_EQ(fixture.platform().process_locks(), 0u);
      CHECK_STATUS(adopt(fixture, &first_context, 2u * page, answer), Status::ok);
      CHECK_EQ(answer.resource_id(), chunk_base);
      CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok);
  }
  { Fixture fixture;
    Host    host(fixture.mapped());
    bind_with_window(fixture);
    fixture.platform().lend_process_range(lent_at, lent_frames, 1u);
    fixture.platform().fail_allocation_at(4u);
    HostWindowRecord answer;
    CHECK_STATUS(adopt(fixture, &first_context, page, answer), Status::no_memory);
    CHECK_EQ(host.count_of(unref_type), 1u);
    CHECK_EQ(fixture.platform().process_locks(), 0u);
    CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }
  { Fixture fixture;
    Host    host(fixture.mapped());
    bind_with_window(fixture);
    fixture.platform().lend_process_range(lent_at, lent_frames, 1u);
    HostWindowRecord answer;
    CHECK_STATUS(adopt(fixture, &first_context, page, answer), Status::ok);
    host.refuse_next(unref_type);
    CHECK_STATUS(close_window(fixture, &first_context, chunk_base), Status::ok);
    CHECK_EQ(fixture.platform().process_locks(), 0u);
    CHECK_STATUS(adopt(fixture, &first_context, page, answer), Status::ok);
    CHECK_EQ(answer.resource_id(), chunk_base + 1u);
    CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }
  Fixture fixture;
  Host    host(fixture.mapped());
  bind_with_window(fixture);
  fixture.platform().lend_process_range(lent_at, lent_frames, 1u);
  fixture.platform().fail_allocation_at(4u);
  host.refuse_next(unref_type);
  HostWindowRecord answer;
  CHECK_STATUS(adopt(fixture, &first_context, page, answer), Status::no_memory);
  CHECK_EQ(fixture.platform().process_locks(), 0u);
  CHECK_STATUS(adopt(fixture, &first_context, page, answer), Status::ok);
  CHECK_EQ(answer.resource_id(), chunk_base + 1u);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

TEST(av_context_table_adopt, destruction_lets_go_of_pages_still_lent)
{ Fixture fixture;
  Host host(fixture.mapped());
  CHECK_STATUS(fixture.window().initialize(window_base, 2u * page), Status::ok);
  fixture.platform().lend_process_range(lent_at, lent_frames, 1u);
  { AvContextTable table(fixture.platform(), fixture.mapped().interface(), fixture.allocations(), fixture.window());
    CHECK_STATUS(table.bind(&first_context, fixture.record(), fixture.device(),
                            fixture.mapped().function().doorbells(), capset()),
                 Status::ok);
    HostWindowRecord lent(lent_at, page, readback);
    CHECK_STATUS(table.window_chunk(&first_context, lent, fixture.device()), Status::ok);
    CHECK_EQ(fixture.platform().process_locks(), 1u); }
  CHECK_EQ(fixture.platform().process_locks(), 0u);
  CHECK_EQ(fixture.platform().owned_page_blocks(), 0u); }

TEST(av_context_table_invariants, detect_a_broken_chunk_a_stray_one_or_a_miscount)
{ Fixture fixture;
  Host host(fixture.mapped());
  bind_with_window(fixture);
  HostWindowRecord answer;
  CHECK_STATUS(open_window(fixture, &first_context, page, answer), Status::ok);
  CHECK(fixture.table().invariants_hold());
  AvContextTableProbe::set_held(fixture.table(), 0u, 2u);
  CHECK(!fixture.table().invariants_hold());
  AvContextTableProbe::set_held(fixture.table(), 0u, 1u);
  CHECK(fixture.table().invariants_hold());
  av::ContextChunk *uncounted = AvContextTableProbe::add_chunk(fixture.table(), 0u, chunk_base + 7u, &first_context);
  AvContextTableProbe::set_held(fixture.table(), 0u, 2u);
  CHECK(!fixture.table().invariants_hold());
  AvContextTableProbe::drop_chunk(fixture.table(), uncounted, 0u);
  CHECK(fixture.table().invariants_hold());
  AvContextTableProbe::take_id(fixture.table());
  av::ContextChunk *stray = AvContextTableProbe::add_chunk(fixture.table(), 5u, chunk_base + 7u, &first_context);
  AvContextTableProbe::set_held(fixture.table(), 5u, 1u);
  CHECK(!fixture.table().invariants_hold());
  AvContextTableProbe::drop_chunk(fixture.table(), stray, 5u);
  CHECK(fixture.table().invariants_hold());
  av::ContextChunk *broken = AvContextTableProbe::add_chunk(fixture.table(), 0u, 0u, &first_context);
  AvContextTableProbe::set_held(fixture.table(), 0u, 2u);
  CHECK(!fixture.table().invariants_hold());
  AvContextTableProbe::drop_chunk(fixture.table(), broken, 0u);
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

TEST(av_context_table_report, counts_a_context_s_chunks)
{ Fixture fixture;
  Host host(fixture.mapped());
  bind_with_window(fixture);
  HostWindowRecord answer;
  CHECK_STATUS(open_window(fixture, &first_context, page, answer), Status::ok);
  CHECK_STATUS(open_window(fixture, &first_context, 2u * page, answer), Status::ok);
  char bytes[1024];
  av::StateWriter writer(bytes, sizeof bytes);
  fixture.table().report(writer);
  CHECK(std::strstr(writer.dump(), " raised=0 chunks=2") != nullptr);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

TEST(av_context_table_report, writes_each_live_context_its_memory_and_its_chunks)
{ Fixture fixture;
  Host host(fixture.mapped());
  bind_with_window(fixture);
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
                " ring_view=%llu reply_view=%llu state=2 context_id=1 completed=0 live=true lost=false"
                " wakes=false raised=0 chunks=1",
                hex_of(&first_context), hex_of(fixture.ring().allocation()),
                static_cast<unsigned long long>(fixture.memory().ring_view()),
                static_cast<unsigned long long>(fixture.memory().reply_view()));
  CHECK(std::strcmp(writer.dump(), expected) == 0);
  CHECK(fixture.table().invariants_hold());
  CHECK_EQ(fixture.platform().locks_held(), 0u);
  fixture.complete_through(0u, 3u);
  fixture.serve();
  av::StateWriter faulted(bytes, sizeof bytes);
  fixture.table().report(faulted);
  CHECK(std::strstr(faulted.dump(), " lost=true wakes=false raised=0 chunks=1") != nullptr);
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

}

TEST(av_context_table_report, a_context_being_set_up_is_counted_but_not_listed)
{ Fixture fixture;
  Host host(fixture.mapped());
  MidBind mid{&fixture.platform(), &fixture.table()};
  fixture.platform().on_acquire(&report_mid_bind, &mid);
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  fixture.platform().on_acquire(nullptr, nullptr);
  CHECK(mid.claimed_unset);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

namespace {

using av::GlContextOptions;
using GlProfile = av::GlContextOptions::Profile;

constexpr std::size_t setup_offset = 32u;
constexpr std::size_t options_offset = setup_offset + 32u;

CapsetInfo current_capset()
{
    return CapsetInfo(av::ProtocolVersion(av::ProtocolVersion::current_major, av::ProtocolVersion::current_minor), 0x7u, 64u, 12u, 20u, 0u,
                      av::GpuIdentity(0u, 0u, 0u, uuid, 0u, 0u, "gpu"));
}

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

Status bind_at(Fixture &fixture, void *context, const AvContextRecord &record)
{
    return fixture.table().bind(context, record, fixture.device(), fixture.mapped().function().doorbells(),
                                current_capset());
}

std::uint8_t setup_byte(const Host &host, std::uint32_t nth, std::size_t offset)
{
    return static_cast<std::uint8_t>(host.field(submit_3d_type, nth, offset) & 0xFFu);
}

int third_context = 0;

}

TEST(av_context_table_bind, a_gl_contexts_options_reach_the_setup_unchanged)
{ Fixture fixture;
  Host host(fixture.mapped());
  const GlContextOptions options(4u, 6u, GlProfile::core,
                                 GlContextOptions::debug | GlContextOptions::robust_access, 0u);
  CHECK_STATUS(bind_at(fixture, &first_context,
                       AvContextRecord(SubApi::opengl, 12u, fixture.ring().handle(), page, options)),
               Status::ok);
  CHECK_EQ(host.count_of(submit_3d_type), 1u);
  CHECK_EQ(host.field(submit_3d_type, 0u, 24u), 48u);
  CHECK_EQ(host.field(submit_3d_type, 0u, setup_offset + 4u),
           av::ProtocolVersion::current_major | (std::uint32_t{av::ProtocolVersion::current_minor} << 16u));
  CHECK_EQ(setup_byte(host, 0u, options_offset), 4u);
  CHECK_EQ(setup_byte(host, 0u, options_offset + 1u), 6u);
  CHECK_EQ(setup_byte(host, 0u, options_offset + 2u), 1u);
  CHECK_EQ(setup_byte(host, 0u, options_offset + 3u), 5u);
  CHECK_EQ(host.field(submit_3d_type, 0u, options_offset + 4u), 0u);
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

TEST(av_context_table_bind, options_that_do_not_fit_the_sub_api_are_refused_before_anything_is_sent)
{ Fixture fixture;
  Host host(fixture.mapped());
  const std::uint32_t sent = host.queue().requests();
  const AvContextRecord with_options(SubApi::vulkan, 12u, fixture.ring().handle(), page,
                                      GlContextOptions(3u, 3u, GlProfile::core, 0u, 0u));
  CHECK_STATUS(bind_at(fixture, &first_context, with_options), Status::invalid_parameter);
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
  CHECK_EQ(host.count_of(submit_3d_type), 2u);
  CHECK_EQ(host.field(submit_3d_type, 1u, options_offset + 4u), 1u);
  CHECK_EQ(setup_byte(host, 1u, options_offset), 4u);
  CHECK_EQ(fixture.handles().outstanding(), 0u);
  CHECK(fixture.table().invariants_hold());
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
  CHECK_STATUS(bind_at(fixture, &first_context, fixture.record()), Status::ok);
  Transport gl(fixture);
  Transport asking(fixture);
  const std::uint32_t own = asking.ring().handle();
  ProcessAllocation stray(fixture.platform(), fixture.allocations(), fixture.handles(), AllocationKind::context_ring,
                          page);
  ProcessAllocation client(fixture.platform(), fixture.allocations(), fixture.handles(), AllocationKind::client, page);
  ProcessAllocation elsewhere(fixture.platform(), fixture.allocations(), fixture.handles(),
                              AllocationKind::context_ring, page);
  const std::uint32_t foreign = elsewhere.handle();
  fixture.handles().remove(foreign);
  const std::uint32_t refused[] = {
      0x7777u,
      foreign,
      fixture.ring().handle(),
      own,
      stray.handle(),
      client.handle(),
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
  CHECK_STATUS(bind_at(fixture, &second_context, gl.record(SubApi::opengl, core)), Status::ok);
  CHECK_STATUS(fixture.table().unbind(&second_context, &fixture.device()), Status::ok);
  const std::uint32_t after = host.queue().requests();
  CHECK_STATUS(bind_at(fixture, &third_context, asking.record(SubApi::opengl, core.sharing(gl.ring().handle()))),
               Status::invalid_parameter);
  CHECK_EQ(host.queue().requests(), after);
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
  CHECK_STATUS(asking.ring().allocation()->close(), Status::ok);
  CHECK_STATUS(asking.ring().allocation()->open(&other_device, &this_process), Status::ok);
  CHECK_STATUS(bind_at(fixture, &third_context, asking.record(SubApi::opengl, core.sharing(gl.ring().handle()))),
               Status::ok);
  CHECK_STATUS(fixture.table().unbind(&third_context, &fixture.device()), Status::ok);
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
  fixture.complete_through(4u);
  fixture.serve();
  CHECK_EQ(progress, 4u);

  fixture.platform().set_irql(Irql::dispatch);
  CHECK_STATUS(fixture.table().driver_submits(&first_context), Status::ok);
  CHECK_STATUS(fixture.table().driver_submits(&second_context), Status::invalid_parameter);
  fixture.platform().set_irql(Irql::passive);

  fixture.table().abandon_all();
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_STATUS(fixture.table().user_mode_submits(&first_context, &progress), Status::invalid_parameter);
  CHECK_STATUS(fixture.table().driver_submits(&first_context), Status::invalid_parameter);
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

TEST(av_context_table, forget_progress_stops_a_bound_context_writing_where_its_queue_was)
{ Fixture fixture;
  Host    host(fixture.mapped());
  std::uint64_t progress = 0u;
  std::uint64_t other = 0u;
  fixture.platform().set_irql(Irql::dispatch);
  CHECK(!fixture.table().forget_progress(&first_context, &progress));
  fixture.platform().set_irql(Irql::passive);
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_STATUS(fixture.table().user_mode_submits(&first_context, &progress), Status::ok);
  CHECK(!fixture.table().forget_progress(&second_context, &progress));
  CHECK(!fixture.table().forget_progress(&first_context, &other));
  fixture.platform().set_irql(Irql::passive);
  fixture.complete_through(2u);
  fixture.serve();
  CHECK_EQ(progress, 2u);

  fixture.platform().set_irql(Irql::dispatch);
  CHECK(fixture.table().forget_progress(&first_context, &progress));
  CHECK(!fixture.table().forget_progress(&first_context, &progress));
  fixture.platform().set_irql(Irql::passive);
  fixture.complete_through(5u);
  fixture.serve();
  CHECK_EQ(progress, 2u);
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

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
  CHECK(fixture.table().completed_already(&second_context, 2u));
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
  CHECK_EQ(tail, 0u);

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

TEST(av_context_table_release_batch, releases_batches_in_ring_order_and_holds_one_that_arrived_early)
{ Fixture fixture;
  Host host(fixture.mapped());
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  std::uint64_t awaited = 0u;
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_STATUS(fixture.table().release_batch(&first_context, SubmissionDescriptor(2u, 32u, 64u), awaited), Status::ok);
  CHECK_EQ(awaited, 2u);
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.mapped().function().doorbells().rings(0u), 0u);
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_STATUS(fixture.table().release_batch(&first_context, SubmissionDescriptor(1u, 0u, 32u), awaited), Status::ok);
  CHECK_EQ(awaited, 1u);
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.mapped().function().doorbells().rings(0u), 2u);
  std::uint64_t tail = 0u;
  std::memcpy(&tail, fixture.ring_memory() + 8u, sizeof tail);
  CHECK_EQ(tail, 64u);
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_STATUS(fixture.table().release_batch(&first_context, SubmissionDescriptor(1u, 0u, 32u), awaited), Status::ok);
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.mapped().function().doorbells().rings(0u), 2u);
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

TEST(av_context_table_release_batch, refuses_a_context_it_does_not_hold_and_a_batch_the_ring_cannot_hold)
{ Fixture fixture;
  Host host(fixture.mapped());
  std::uint64_t awaited = 9u;
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_STATUS(fixture.table().release_batch(&first_context, SubmissionDescriptor(1u, 0u, 32u), awaited),
               Status::invalid_parameter);
  CHECK_EQ(awaited, 0u);
  fixture.platform().set_irql(Irql::passive);
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_STATUS(fixture.table().release_batch(&first_context, SubmissionDescriptor(1u, 0u, 5000u), awaited),
               Status::invalid_parameter);
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.mapped().function().doorbells().rings(0u), 0u);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

TEST(av_context_table_fault_if_held, loses_a_context_that_holds_a_batch_no_earlier_one_released)
{ Fixture fixture;
  Host host(fixture.mapped());
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  std::uint64_t awaited = 0u;
  fixture.platform().set_irql(Irql::dispatch);
  CHECK(!fixture.table().fault_if_held(&first_context));
  CHECK(!fixture.table().fault_if_held(nullptr));
  CHECK_STATUS(fixture.table().release_batch(&first_context, SubmissionDescriptor(2u, 32u, 64u), awaited), Status::ok);
  CHECK(!fixture.table().fault_if_held(&second_context));
  CHECK(fixture.table().fault_if_held(&first_context));
  CHECK(!fixture.table().fault_if_held(&first_context));
  CHECK_STATUS(fixture.table().release_batch(&first_context, SubmissionDescriptor(1u, 0u, 32u), awaited),
               Status::invalid_parameter);
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

TEST(av_context_table_bind, a_ring_order_or_reply_the_memory_cannot_be_is_refused_as_such)
{ Fixture fixture;
  Host host(fixture.mapped());
  CHECK_STATUS(fixture.bind(&first_context, fixture.record(40u)), Status::invalid_parameter);
  CHECK_STATUS(fixture.bind(&first_context, AvContextRecord(SubApi::vulkan, 12u, fixture.ring().handle(), 0u)),
               Status::invalid_parameter);
  CHECK_EQ(fixture.table().bound(), 0u);
  CHECK_EQ(host.queue().requests(), 0u);
  CHECK_EQ(fixture.platform().owned_page_blocks(), 0u);
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

TEST(av_context_table_bind, a_setup_and_a_teardown_the_device_both_refuse_leave_the_memory_it_may_still_write)
{ Fixture fixture;
  Host host(fixture.mapped());
  host.refuse_from(2);
  CHECK_STATUS(fixture.bind(&first_context), Status::not_supported);
  CHECK_EQ(fixture.table().bound(), 0u);
  CHECK_EQ(host.count_of(ctx_destroy_type), 1u);
  CHECK_EQ(fixture.platform().mapped_frames(), 0u);
  CHECK_EQ(fixture.platform().process_mappings(), 0u);
  CHECK(fixture.table().invariants_hold());
  host.refuse_from(-1);
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

TEST(av_context_table_submit, a_submission_after_a_release_waits_its_turn_like_a_release)
{ Fixture fixture;
  Host host(fixture.mapped());
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  std::uint64_t awaited = 0u;
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_STATUS(fixture.table().release_batch(&first_context, SubmissionDescriptor(1u, 0u, 32u), awaited), Status::ok);
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.mapped().function().doorbells().rings(0u), 1u);
  CHECK_STATUS(fixture.submit(&first_context, SubmissionDescriptor(3u, 64u, 96u), awaited), Status::ok);
  CHECK_EQ(awaited, 3u);
  CHECK_EQ(fixture.mapped().function().doorbells().rings(0u), 1u);
  CHECK_STATUS(fixture.submit(&first_context, SubmissionDescriptor(2u, 32u, 64u), awaited), Status::ok);
  CHECK_EQ(fixture.mapped().function().doorbells().rings(0u), 1u + 2u);
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

TEST(av_context_table_release_batch, a_slot_not_yet_live_takes_nothing_and_has_nothing_running)
{ Fixture fixture;
  Host host(fixture.mapped());
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  std::uint64_t awaited = 9u;
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_STATUS(fixture.table().release_batch(&first_context, SubmissionDescriptor(2u, 32u, 64u), awaited), Status::ok);
  AvContextTableProbe::set_live(fixture.table(), 0u, false);
  CHECK_STATUS(fixture.table().release_batch(&first_context, SubmissionDescriptor(1u, 0u, 32u), awaited),
               Status::invalid_parameter);
  CHECK(!fixture.table().fault_if_held(&first_context));
  const std::uint32_t rings = fixture.mapped().function().doorbells().rings(0u);
  fixture.table().kick(&first_context);
  CHECK_EQ(fixture.mapped().function().doorbells().rings(0u), rings);
  CHECK(fixture.table().completed_already(&first_context, 1000u));
  AvContextTableProbe::set_live(fixture.table(), 0u, true);
  fixture.table().kick(&first_context);
  CHECK_EQ(fixture.mapped().function().doorbells().rings(0u), rings + 1u);
  CHECK(!fixture.table().completed_already(&first_context, 1000u));
  fixture.platform().set_irql(Irql::passive);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

TEST(av_context_table_release_batch, a_batch_overlapping_one_released_or_refused_by_the_ring_loses_the_context)
{ Fixture fixture;
  Host host(fixture.mapped());
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  std::uint64_t awaited = 0u;
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_STATUS(fixture.table().release_batch(&first_context, SubmissionDescriptor(1u, 0u, 32u), awaited), Status::ok);
  CHECK_STATUS(fixture.table().release_batch(&first_context, SubmissionDescriptor(2u, 16u, 48u), awaited), Status::ok);
  CHECK(fixture.platform().logged_line("the context is lost"));
  CHECK_STATUS(fixture.table().release_batch(&first_context, SubmissionDescriptor(3u, 32u, 64u), awaited),
               Status::invalid_parameter);
  fixture.platform().set_irql(Irql::passive);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok);

  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  CHECK_STATUS(fixture.submit(&first_context, SubmissionDescriptor(7u, 0u, 32u), awaited), Status::ok);
  const std::uint32_t lost = fixture.platform().logged();
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_STATUS(fixture.table().release_batch(&first_context, SubmissionDescriptor(3u, 32u, 64u), awaited), Status::ok);
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().logged() > lost);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok);

  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_STATUS(fixture.table().release_batch(&first_context, SubmissionDescriptor(5u, 32u, 64u), awaited), Status::ok);
  const std::uint32_t held = fixture.platform().logged();
  CHECK_STATUS(fixture.table().release_batch(&first_context, SubmissionDescriptor(9u, 0u, 32u), awaited), Status::ok);
  CHECK(fixture.platform().logged() > held);
  CHECK(!fixture.table().fault_if_held(&first_context));
  fixture.platform().set_irql(Irql::passive);
  CHECK_STATUS(fixture.table().unbind(&first_context, &fixture.device()), Status::ok); }

TEST(av_context_table_invariants, detect_a_live_slot_without_its_binding_and_a_free_one_with_a_stray_binding_wake_or_flag)
{ Fixture fixture;
  Host host(fixture.mapped());
  AvContextTable &table = fixture.table();
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  CHECK(table.invariants_hold());
  AvContextTableProbe::set_context(table, 0u, nullptr);
  CHECK(!table.invariants_hold());
  AvContextTableProbe::set_context(table, 0u, &first_context);
  CHECK(table.invariants_hold());
  AvContextTableProbe::set_context(table, 1u, &second_context);
  AvContextTableProbe::swap_name(table, 1u, fixture.ring().allocation());
  AvContextTableProbe::set_wake(table, 1u, &second_context);
  AvContextTableProbe::set_context(table, 1u, nullptr);
  CHECK(!table.invariants_hold());
  AvContextTableProbe::swap_name(table, 1u, nullptr);
  CHECK(!table.invariants_hold());
  AvContextTableProbe::set_wake(table, 1u, nullptr);
  CHECK(table.invariants_hold());
  AvContextTableProbe::set_raised(table, 1u, 1u);
  CHECK(!table.invariants_hold());
  AvContextTableProbe::set_raised(table, 1u, 0u);
  AvContextTableProbe::set_told_lost(table, 1u, true);
  CHECK(!table.invariants_hold());
  AvContextTableProbe::set_told_lost(table, 1u, false);
  CHECK(table.invariants_hold());
  AvContextTableProbe::set_context(table, 0u, nullptr);
  CHECK(!table.invariants_hold());
  AvContextTableProbe::set_context(table, 0u, &first_context);
  AvContextTableProbe::binding(table, 0u).destroy();
  CHECK(!table.invariants_hold()); }

TEST(av_context_table_invariants, detect_a_free_slot_that_still_holds_a_binding)
{ Fixture fixture;
  Host host(fixture.mapped());
  AvContextTable &table = fixture.table();
  CHECK_STATUS(fixture.bind(&first_context), Status::ok);
  ProcessAllocation other(fixture.platform(), fixture.allocations(), fixture.handles(), AllocationKind::context_ring,
                          page);
  CHECK_STATUS(fixture.bind(&second_context, AvContextRecord(SubApi::vulkan, 12u, other.handle(), page)), Status::ok);
  CHECK(table.invariants_hold());
  AvContextTableProbe::set_live(table, 1u, false);
  AvContextTableProbe::set_context(table, 1u, nullptr);
  CHECK(!table.invariants_hold());
  AvContextTableProbe::set_context(table, 1u, &second_context);
  AvContextTableProbe::set_live(table, 1u, true);
  CHECK(table.invariants_hold()); }

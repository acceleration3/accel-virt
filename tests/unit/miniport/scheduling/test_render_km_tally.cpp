#include "miniport/adapter/adapter.hpp"
#include "miniport/ddi/dxgk_allocation_info_record.hpp"
#include "miniport/ddi/dxgk_allocation_list_record.hpp"
#include "miniport/ddi/dxgk_open_allocation_info_record.hpp"
#include "miniport/ddi/dxgk_renderkm_command_record.hpp"
#include "miniport/ddi/dxgkarg_create_allocation_record.hpp"
#include "miniport/ddi/dxgkarg_create_device_record.hpp"
#include "miniport/ddi/dxgkarg_open_allocation_record.hpp"
#include "miniport/ddi/dxgkarg_render_record.hpp"
#include "miniport/driver/dxgkrnl_interface.hpp"
#include "miniport/memory/aperture_segment.hpp"
#include "miniport/memory/allocation_table.hpp"
#include "miniport/memory/memory_segments.hpp"
#include "miniport/memory/shared_object_table.hpp"
#include "miniport/scheduling/device_table.hpp"
#include "miniport/scheduling/render_km_tally.hpp"
#include "core/support/nt_status.hpp"
#include "core/support/status.hpp"
#include "core/trace/state_writer.hpp"
#include "harness/dxgkrnl/fake_dxgkrnl_adapter.hpp"
#include "harness/dxgkrnl/fake_handle_table.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"
#include "harness/virtio/fake_virtio_gpu_function.hpp"
#include "proto/resources/allocation_descriptor.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace av {

/* The one thing a tally invariant cannot be reached without: a kind with no BitBlt behind it. */
struct RenderKmTallyProbe {
    static void invent_kind(RenderKmTally &tally) { tally.last_bitblt_source_kind_ = 2u; }
};

}  // namespace av

using av::Adapter;
using av::AllocationDescriptor;
using av::AllocationKind;
using av::AllocationTable;
using av::ApertureSegment;
using av::DeviceTable;
using av::DxgkAllocationInfoRecord;
using av::DxgkAllocationListRecord;
using av::DxgkGdiargBitbltRecord;
using av::DxgkOpenAllocationInfoRecord;
using av::DxgkRenderkmCommandRecord;
using av::DxgkRenderkmOperation;
using av::DxgkargCreateAllocationRecord;
using av::DxgkargCreateDeviceRecord;
using av::DxgkargOpenAllocationRecord;
using av::DxgkargRenderRecord;
using av::DxgkrnlInterface;
using av::FakeDxgkrnlAdapter;
using av::FakeHandleTable;
using av::FakeVirtioGpuFunction;
using av::GuardedBuffer;
using av::Irql;
using av::MockPlatform;
using av::NtStatus;
using av::RenderKmTally;
using av::ScanoutTarget;
using av::Status;
using av::SurfaceLayout;
using av::Violation;

namespace {

constexpr std::uint32_t command_bytes = sizeof(DxgkRenderkmCommandRecord);
constexpr std::uint32_t header_bytes = 8u;
constexpr std::uint32_t dma_bytes = 64u;
constexpr std::uint32_t kind_shadow = 2u;
constexpr std::uint32_t kind_staging = 3u;
constexpr std::uint32_t insufficient_dma_buffer = 0xC01E0001u;

/* A shadow and a staging allocation, both opened by one device: what the
 * allocation list of a GDI context's command buffer names. */
class Fixture {
public:
    Fixture()
        : dxgkrnl_(platform_, function_), interface_(platform_), adapter_(platform_, &pdo_), segment_(platform_),
          segments_(platform_, segment_), shared_(platform_),
          allocations_(platform_, segment_, segments_, shared_), devices_(platform_, allocations_, shared_, interface_),
          tally_(platform_, devices_), infos_{}, descriptors_{}, opens_{}, list_{}
    {
        const AllocationKind kinds[] = {AllocationKind::shadow, AllocationKind::staging};
        DxgkargCreateAllocationRecord create = {};
        create.NumAllocations = 2u;
        create.pAllocationInfo = infos_;
        for (std::uint32_t i = 0u; i < 2u; i++) {
            const AllocationDescriptor descriptor(kinds[i], true, SurfaceLayout(10u, 10u, 21u, 40u), 400u,
                                                  ScanoutTarget());
            static_cast<void>(descriptor.encode(descriptors_[i], AllocationDescriptor::encoded_size));
            infos_[i].pPrivateDriverData = descriptors_[i];
            infos_[i].PrivateDriverDataSize = AllocationDescriptor::encoded_size;
        }
        DxgkargCreateDeviceRecord device = {};
        device.hDevice = &runtime_device_;
        const bool ready = shared_.initialize() == Status::ok &&
                           interface_.initialize(&dxgkrnl_.record()) == Status::ok &&
                           segment_.initialize(std::int64_t{1} << 30u) == Status::ok &&
                           allocations_.create(create) == Status::ok &&
                           devices_.create(adapter_, device) == Status::ok;
        if (!ready) av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "fixture");
        DxgkargOpenAllocationRecord open = {};
        open.NumAllocations = 2u;
        open.pOpenAllocation = opens_;
        for (std::uint32_t i = 0u; i < 2u; i++) opens_[i].hAllocation = kernel_handles_.insert(infos_[i].hAllocation);
        if (devices_.open(device.hDevice, open) != Status::ok)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "open");
        list_[0].hDeviceSpecificAllocation = opens_[0].hDeviceSpecificAllocation;
        list_[1].hDeviceSpecificAllocation = opens_[1].hDeviceSpecificAllocation;
    }

    MockPlatform             &platform() { return platform_; }
    RenderKmTally            &tally() { return tally_; }
    DxgkAllocationListRecord *list() { return list_; }

private:
    MockPlatform                 platform_;
    FakeVirtioGpuFunction        function_;
    FakeDxgkrnlAdapter           dxgkrnl_;
    FakeHandleTable              kernel_handles_;
    DxgkrnlInterface             interface_;
    int                          pdo_ = 0;
    int                          runtime_device_ = 0;
    Adapter                      adapter_;
    ApertureSegment              segment_;
    av::MemorySegments           segments_;
    av::SharedObjectTable        shared_;
    AllocationTable              allocations_;
    DeviceTable                  devices_;
    RenderKmTally                tally_;
    DxgkAllocationInfoRecord     infos_[2];
    std::uint8_t                 descriptors_[2][AllocationDescriptor::encoded_size];
    DxgkOpenAllocationInfoRecord opens_[2];
    DxgkAllocationListRecord     list_[2];
};

/* A command buffer of whole commands, each `size` bytes long. */
class Buffer {
public:
    Buffer() : used_(0u), dma_(dma_bytes), request_{}
    {
        std::memset(bytes_, 0, sizeof bytes_);
        std::memset(dma_.data(), 0xEE, dma_bytes);
        request_.pDmaBuffer = dma_.data();
        request_.DmaSize = dma_bytes;
        request_.pCommand = bytes_;
    }

    /* Appends a command with the given operation and size, a BitBlt's indices in its arguments. */
    void add(std::uint32_t operation, std::uint32_t size, std::uint32_t source = 0u, std::uint32_t destination = 0u)
    {
        DxgkRenderkmCommandRecord command = {};
        command.OpCode = operation;
        command.CommandSize = size;
        command.BitBlt.SrcAllocationIndex = source;
        command.BitBlt.DstAllocationIndex = destination;
        std::memcpy(bytes_ + used_, &command, size < command_bytes ? size : command_bytes);
        used_ += size;
        request_.CommandLength = used_;
    }

    DxgkargRenderRecord &request() { return request_; }
    GuardedBuffer       &dma() { return dma_; }
    std::uint8_t        *bytes() { return bytes_; }

private:
    std::uint8_t        bytes_[8u * command_bytes];
    std::uint32_t       used_;
    GuardedBuffer       dma_;
    DxgkargRenderRecord request_;
};

bool dump_holds(RenderKmTally &tally, const char *line)
{
    char                bytes[1024];
    av::StateWriter     writer(bytes, sizeof bytes);
    tally.report(writer);
    return std::strstr(writer.dump(), line) != nullptr;
}

constexpr std::uint32_t op(DxgkRenderkmOperation operation) { return static_cast<std::uint32_t>(operation); }

}  // namespace

TEST(render_km_tally_render, counts_each_operation_and_answers_a_no_op_dma_buffer)
{ Fixture fixture; Buffer buffer;
  buffer.add(op(DxgkRenderkmOperation::bitblt), command_bytes);
  buffer.add(op(DxgkRenderkmOperation::colorfill), command_bytes);
  buffer.add(op(DxgkRenderkmOperation::colorfill), command_bytes);
  buffer.add(op(DxgkRenderkmOperation::alphablend), command_bytes);
  buffer.add(op(DxgkRenderkmOperation::stretchblt), command_bytes);
  buffer.add(op(DxgkRenderkmOperation::escape), header_bytes);
  buffer.add(op(DxgkRenderkmOperation::transparentblt), command_bytes);
  buffer.add(op(DxgkRenderkmOperation::cleartypeblend), command_bytes);
  const auto *start = static_cast<std::uint8_t *>(buffer.request().pDmaBuffer);
  CHECK_EQ(fixture.tally().render(buffer.request()).value(), 0);
  CHECK(fixture.tally().invariants_hold());
  CHECK(dump_holds(fixture.tally(), "renderkm_calls=1 renderkm_bitblt=1 renderkm_colorfill=2 renderkm_alphablend=1"
                                    " renderkm_stretchblt=1 renderkm_transparentblt=1 renderkm_cleartypeblend=1"
                                    " renderkm_escape=1 renderkm_unknown=0"));
  CHECK(buffer.request().pDmaBuffer == start + 4);
  CHECK_EQ(start[0], 0u);
  CHECK_EQ(start[3], 0u);
  CHECK_EQ(start[4], 0xEEu);
  CHECK(buffer.dma().guards_intact()); }

TEST(render_km_tally_render, counts_across_calls_and_an_empty_buffer_is_a_call)
{ Fixture fixture; Buffer first; Buffer second; Buffer empty;
  first.add(op(DxgkRenderkmOperation::colorfill), command_bytes);
  second.add(op(DxgkRenderkmOperation::colorfill), command_bytes);
  empty.request().pCommand = nullptr;
  CHECK_EQ(fixture.tally().render(first.request()).value(), 0);
  CHECK_EQ(fixture.tally().render(second.request()).value(), 0);
  CHECK_EQ(fixture.tally().render(empty.request()).value(), 0);
  CHECK(dump_holds(fixture.tally(), "renderkm_calls=3 renderkm_bitblt=0 renderkm_colorfill=2"));
  CHECK(fixture.tally().invariants_hold()); }

TEST(render_km_tally_render, an_operation_it_does_not_know_is_counted_and_not_refused)
{ Fixture fixture; Buffer buffer;
  buffer.add(0u, command_bytes);
  buffer.add(8u, command_bytes);
  buffer.add(0xFFFFFFFFu, header_bytes);
  CHECK_EQ(fixture.tally().render(buffer.request()).value(), 0);
  CHECK(dump_holds(fixture.tally(), "renderkm_escape=0 renderkm_unknown=3")); }

TEST(render_km_tally_render, resolves_the_kinds_the_last_bitblt_names_in_the_allocation_list)
{ Fixture fixture; Buffer buffer;
  buffer.request().pAllocationList = fixture.list();
  buffer.request().AllocationListSize = 2u;
  buffer.add(op(DxgkRenderkmOperation::bitblt), command_bytes, 0u, 1u);
  buffer.add(op(DxgkRenderkmOperation::bitblt), command_bytes, 1u, 0u);
  CHECK_EQ(fixture.tally().render(buffer.request()).value(), 0);
  CHECK(fixture.tally().invariants_hold());
  char expected[96];
  std::snprintf(expected, sizeof expected, "renderkm_last_bitblt_source_kind=%u renderkm_last_bitblt_destination_kind=%u",
                kind_staging, kind_shadow);
  CHECK(dump_holds(fixture.tally(), expected)); }

TEST(render_km_tally_render, a_bitblt_naming_nothing_it_opened_has_no_kind)
{ for (int knob = 0; knob < 4; knob++) {
      Fixture fixture; Buffer buffer;
      buffer.request().pAllocationList = fixture.list();
      buffer.request().AllocationListSize = 2u;
      std::uint32_t source = 0u;
      if (knob == 0) source = 2u;                                    /* past the list */
      if (knob == 1) buffer.request().pAllocationList = nullptr;
      if (knob == 2) fixture.list()[0].hDeviceSpecificAllocation = &fixture;   /* not an open of this table */
      if (knob == 3) fixture.list()[0].hDeviceSpecificAllocation = nullptr;
      buffer.add(op(DxgkRenderkmOperation::bitblt), command_bytes, source, 1u);
      CHECK_EQ(fixture.tally().render(buffer.request()).value(), 0);
      CHECK(dump_holds(fixture.tally(), "renderkm_bitblt=1"));
      CHECK(dump_holds(fixture.tally(), "renderkm_last_bitblt_source_kind=0"));
  } }

TEST(render_km_tally_render, a_bitblt_too_short_to_name_its_allocations_is_counted_with_no_kinds)
{ Fixture fixture; Buffer first; Buffer second;
  first.request().pAllocationList = fixture.list();
  first.request().AllocationListSize = 2u;
  first.add(op(DxgkRenderkmOperation::bitblt), command_bytes, 0u, 1u);
  CHECK_EQ(fixture.tally().render(first.request()).value(), 0);
  second.request().pAllocationList = fixture.list();
  second.request().AllocationListSize = 2u;
  second.add(op(DxgkRenderkmOperation::bitblt), header_bytes);
  CHECK_EQ(fixture.tally().render(second.request()).value(), 0);
  CHECK(dump_holds(fixture.tally(), "renderkm_bitblt=2"));
  CHECK(dump_holds(fixture.tally(), "renderkm_last_bitblt_source_kind=0 renderkm_last_bitblt_destination_kind=0")); }

TEST(render_km_tally_render, a_bitblt_whose_command_ends_the_buffer_exactly_at_its_indices_is_read)
{ Fixture fixture; Buffer buffer;
  buffer.request().pAllocationList = fixture.list();
  buffer.request().AllocationListSize = 2u;
  const std::uint32_t read_bytes = header_bytes + static_cast<std::uint32_t>(offsetof(DxgkGdiargBitbltRecord, NumSubRects));
  buffer.add(op(DxgkRenderkmOperation::bitblt), read_bytes, 1u, 1u);
  CHECK_EQ(fixture.tally().render(buffer.request()).value(), 0);
  CHECK(dump_holds(fixture.tally(), "renderkm_last_bitblt_source_kind=3 renderkm_last_bitblt_destination_kind=3")); }

TEST(render_km_tally_render, refuses_a_buffer_it_cannot_walk_and_writes_no_dma)
{ for (int knob = 0; knob < 6; knob++) {
      Fixture fixture; Buffer buffer;
      buffer.add(op(DxgkRenderkmOperation::colorfill), command_bytes);
      buffer.add(op(DxgkRenderkmOperation::colorfill), command_bytes);
      std::uint32_t second_size = command_bytes;
      if (knob == 0) second_size = 0u;                                /* never moves on */
      if (knob == 1) second_size = header_bytes - 1u;
      if (knob == 2) second_size = command_bytes + 1u;                /* runs past the buffer */
      if (knob == 3) second_size = 0xFFFFFFFFu;
      if (knob == 4) buffer.request().CommandLength = command_bytes + header_bytes - 1u;   /* a header cut short */
      if (knob == 5) buffer.request().CommandLength = command_bytes + header_bytes;        /* second is cut short */
      std::memcpy(buffer.bytes() + command_bytes + 4u, &second_size, sizeof second_size);
      const auto *start = static_cast<std::uint8_t *>(buffer.request().pDmaBuffer);
      const std::int32_t answer = fixture.tally().render(buffer.request()).value();
      CHECK_EQ(static_cast<std::uint32_t>(answer), 0xC000000Du);
      CHECK(buffer.request().pDmaBuffer == start);
      CHECK_EQ(start[0], 0xEEu);
      CHECK(buffer.dma().guards_intact());
      CHECK(fixture.tally().invariants_hold());
  } }

TEST(render_km_tally_render, refuses_pointers_it_cannot_read_or_write)
{ for (int knob = 0; knob < 2; knob++) {
      Fixture fixture; Buffer buffer;
      buffer.add(op(DxgkRenderkmOperation::colorfill), command_bytes);
      if (knob == 0) buffer.request().pCommand = nullptr;
      if (knob == 1) buffer.request().pDmaBuffer = nullptr;
      CHECK_EQ(static_cast<std::uint32_t>(fixture.tally().render(buffer.request()).value()), 0xC000000Du);
      CHECK(dump_holds(fixture.tally(), "renderkm_calls=0"));
  } }

TEST(render_km_tally_render, asks_for_more_dma_buffer_and_does_not_count_the_buffer_it_retries)
{ Fixture fixture; Buffer buffer;
  buffer.add(op(DxgkRenderkmOperation::colorfill), command_bytes);
  buffer.request().DmaSize = 3u;
  CHECK_EQ(static_cast<std::uint32_t>(fixture.tally().render(buffer.request()).value()), insufficient_dma_buffer);
  CHECK(dump_holds(fixture.tally(), "renderkm_calls=0 renderkm_bitblt=0 renderkm_colorfill=0"));
  buffer.request().DmaSize = 4u;
  CHECK_EQ(fixture.tally().render(buffer.request()).value(), 0);
  CHECK(dump_holds(fixture.tally(), "renderkm_calls=1 renderkm_bitblt=0 renderkm_colorfill=1")); }

TEST(render_km_tally_invariants, a_kind_with_no_bitblt_behind_it_is_broken)
{ Fixture fixture;
  CHECK(fixture.tally().invariants_hold());
  av::RenderKmTallyProbe::invent_kind(fixture.tally());
  CHECK(!fixture.tally().invariants_hold()); }

TEST(render_km_tally, is_passive_only)
{ Fixture fixture; Buffer buffer;
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_EQ(fixture.tally().render(buffer.request()).value(), 0);
  char bytes[1024];
  av::StateWriter writer(bytes, sizeof bytes);
  fixture.tally().report(writer);
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().claim(Violation::irql_ceiling));
  CHECK(fixture.platform().claim(Violation::irql_ceiling)); }

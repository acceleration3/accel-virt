#include "miniport/ddi/d3dkmdt_gdi_surface_data_record.hpp"
#include "miniport/ddi/d3dkmdt_shadow_surface_data_record.hpp"
#include "miniport/ddi/d3dkmdt_shared_primary_surface_data_record.hpp"
#include "miniport/ddi/d3dkmdt_staging_surface_data_record.hpp"
#include "miniport/ddi/dxgkarg_get_standard_allocation_driver_data_record.hpp"
#include "miniport/memory/standard_allocation.hpp"
#include "core/support/status.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"
#include "proto/resources/allocation_descriptor.hpp"

#include <cstdint>
#include <cstring>

using av::AllocationDescriptor;
using av::AllocationKind;
using av::D3dkmdtGdiSurfaceDataRecord;
using av::D3dkmdtShadowSurfaceDataRecord;
using av::D3dkmdtSharedPrimarySurfaceDataRecord;
using av::D3dkmdtStagingSurfaceDataRecord;
using av::DxgkargGetStandardAllocationDriverDataRecord;
using av::GuardedBuffer;
using av::Irql;
using av::MockPlatform;
using av::StandardAllocation;
using av::Status;
using av::Violation;

namespace {

constexpr std::int32_t shared_primary = 1, shadow = 2, staging = 3, gdi = 4, vgpu = 5, fence_storage = 6;
constexpr std::int32_t a8r8g8b8 = 21, r5g6b5 = 23;

class Request {
public:
    Request(std::int32_t type, void *surface) : record_{}, bytes_(AllocationDescriptor::encoded_size)
    {
        record_.StandardAllocationType = type;
        record_.pCreateSurfaceData = surface;
        record_.pAllocationPrivateDriverData = bytes_.data();
        record_.AllocationPrivateDriverDataSize = 0xEEu;
        record_.ResourcePrivateDriverDataSize = 0xEEu;
    }

    Status describe(MockPlatform &platform) { return StandardAllocation(platform, record_).describe(); }
    DxgkargGetStandardAllocationDriverDataRecord &record() { return record_; }

    AllocationDescriptor descriptor()
    {
        AllocationDescriptor descriptor;
        if (descriptor.decode(bytes_.data(), AllocationDescriptor::encoded_size) != Status::ok)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "descriptor did not decode");
        CHECK(bytes_.guards_intact());
        return descriptor;
    }

private:
    DxgkargGetStandardAllocationDriverDataRecord record_;
    GuardedBuffer                                bytes_;
};

}

TEST(standard_allocation_describe, without_a_buffer_reports_only_the_sizes)
{ MockPlatform platform; Request request(shared_primary, nullptr);
  request.record().pAllocationPrivateDriverData = nullptr;
  CHECK_STATUS(request.describe(platform), Status::ok);
  CHECK_EQ(request.record().AllocationPrivateDriverDataSize, AllocationDescriptor::encoded_size);
  CHECK_EQ(request.record().ResourcePrivateDriverDataSize, 0u); }

TEST(standard_allocation_describe, a_shared_primary_names_its_source_and_refresh_rate)
{ MockPlatform platform;
  D3dkmdtSharedPrimarySurfaceDataRecord surface = {1024u, 768u, a8r8g8b8, 60000u, 1001u, 0u};
  Request request(shared_primary, &surface);
  CHECK_STATUS(request.describe(platform), Status::ok);
  CHECK_EQ(request.record().AllocationPrivateDriverDataSize, AllocationDescriptor::encoded_size);
  CHECK_EQ(request.record().ResourcePrivateDriverDataSize, 0u);
  const AllocationDescriptor descriptor = request.descriptor();
  CHECK(descriptor.kind() == AllocationKind::shared_primary);
  CHECK(descriptor.cpu_visible());
  CHECK_EQ(descriptor.layout().pitch(), 4096u);
  CHECK(descriptor.size() == 4096u * 768u);
  CHECK_EQ(descriptor.scanout().refresh_numerator(), 60000u);
  CHECK_EQ(descriptor.scanout().refresh_denominator(), 1001u); }

TEST(standard_allocation_describe, shadow_and_staging_answer_their_pitch)
{ MockPlatform platform;
  D3dkmdtShadowSurfaceDataRecord shadow_surface = {100u, 50u, a8r8g8b8, 0u};
  Request shadow_request(shadow, &shadow_surface);
  CHECK_STATUS(shadow_request.describe(platform), Status::ok);
  CHECK_EQ(shadow_surface.Pitch, 400u);
  CHECK(shadow_request.descriptor().kind() == AllocationKind::shadow);
  D3dkmdtStagingSurfaceDataRecord staging_surface = {100u, 50u, 0u};
  Request staging_request(staging, &staging_surface);
  CHECK_STATUS(staging_request.describe(platform), Status::ok);
  CHECK_EQ(staging_surface.Pitch, 400u);
  const AllocationDescriptor descriptor = staging_request.descriptor();
  CHECK(descriptor.kind() == AllocationKind::staging);
  CHECK_EQ(descriptor.layout().format(), static_cast<unsigned>(a8r8g8b8));
  CHECK(descriptor.cpu_visible()); }

TEST(standard_allocation_describe, gdi_surfaces_are_cpu_visible_only_when_they_say_so)
{ MockPlatform platform;
  struct Case { std::int32_t type; bool cpu_visible; };
  const Case cases[] = {{1, false}, {2, true}, {3, false}, {4, true}, {5, true}, {6, true}};
  for (const Case &c : cases) {
      D3dkmdtGdiSurfaceDataRecord surface = {16u, 16u, a8r8g8b8, c.type, 0u, 0u};
      Request request(gdi, &surface);
      CHECK_STATUS(request.describe(platform), Status::ok);
      CHECK_EQ(surface.Pitch, 64u);
      const AllocationDescriptor descriptor = request.descriptor();
      CHECK(descriptor.kind() == AllocationKind::gdi);
      CHECK(descriptor.cpu_visible() == c.cpu_visible);
  } }

TEST(standard_allocation_describe, an_existing_heap_is_a_cpu_visible_row_of_its_bytes)
{ MockPlatform platform;
  D3dkmdtGdiSurfaceDataRecord surface = {0x30000u, 1u, 0, 7, 0u, 0xEEu};
  Request request(gdi, &surface);
  CHECK_STATUS(request.describe(platform), Status::ok);
  CHECK_EQ(surface.Pitch, 0x30000u);
  const AllocationDescriptor descriptor = request.descriptor();
  CHECK(descriptor.kind() == AllocationKind::cross_adapter);
  CHECK(descriptor.cpu_visible());
  CHECK(descriptor.size() == 0x30000u);
  CHECK_EQ(descriptor.layout().pitch(), 0x30000u);
  CHECK_EQ(descriptor.layout().height(), 1u);
  const D3dkmdtGdiSurfaceDataRecord refused[] = {{0u, 1u, 0, 7, 0u, 0xEEu}, {0x1000u, 2u, 0, 7, 0u, 0xEEu}};
  for (D3dkmdtGdiSurfaceDataRecord heap : refused) {
      CHECK_STATUS(Request(gdi, &heap).describe(platform), Status::invalid_parameter);
      CHECK_EQ(heap.Pitch, 0xEEu);
  } }

TEST(standard_allocation_describe, a_cross_adapter_texture_has_dxgkrnls_pitch_and_pages)
{ MockPlatform platform;
  D3dkmdtGdiSurfaceDataRecord surface = {1249u, 805u, a8r8g8b8, 7, 0u, 0xEEu};
  Request request(gdi, &surface);
  CHECK_STATUS(request.describe(platform), Status::ok);
  CHECK_EQ(surface.Pitch, 5120u);
  const AllocationDescriptor descriptor = request.descriptor();
  CHECK(descriptor.kind() == AllocationKind::cross_adapter);
  CHECK(descriptor.cpu_visible());
  CHECK(descriptor.size() == 1010u * 4096u);
  CHECK_EQ(descriptor.layout().width(), 1249u);
  CHECK_EQ(descriptor.layout().height(), 805u);
  CHECK_EQ(descriptor.layout().format(), 21u);
  CHECK_EQ(descriptor.layout().pitch(), 5120u);
  D3dkmdtGdiSurfaceDataRecord narrow = {1056u, 16u, a8r8g8b8, 7, 0u, 0xEEu};
  CHECK_STATUS(Request(gdi, &narrow).describe(platform), Status::ok);
  CHECK_EQ(narrow.Pitch, 4224u);
  D3dkmdtGdiSurfaceDataRecord unaligned = {160u, 120u, a8r8g8b8, 7, 0u, 0xEEu};
  CHECK_STATUS(Request(gdi, &unaligned).describe(platform), Status::ok);
  CHECK_EQ(unaligned.Pitch, 640u);
  D3dkmdtGdiSurfaceDataRecord wide = {100u, 100u, 113, 7, 0u, 0xEEu};
  CHECK_STATUS(Request(gdi, &wide).describe(platform), Status::ok);
  CHECK_EQ(wide.Pitch, 896u);
  D3dkmdtGdiSurfaceDataRecord unknown = {16u, 16u, r5g6b5, 7, 0u, 0xEEu};
  CHECK_STATUS(Request(gdi, &unknown).describe(platform), Status::invalid_parameter);
  CHECK_EQ(unknown.Pitch, 0xEEu); }

TEST(standard_allocation_describe, a_cpu_visible_cross_adapter_texture_is_laid_out_as_a_cross_adapter_texture)
{ MockPlatform platform;
  D3dkmdtGdiSurfaceDataRecord plain = {1249u, 805u, a8r8g8b8, 7, 0u, 0xEEu};
  D3dkmdtGdiSurfaceDataRecord visible = {1249u, 805u, a8r8g8b8, 8, 0u, 0xEEu};
  Request plain_request(gdi, &plain);
  Request visible_request(gdi, &visible);
  CHECK_STATUS(plain_request.describe(platform), Status::ok);
  CHECK_STATUS(visible_request.describe(platform), Status::ok);
  CHECK_EQ(visible.Pitch, 5120u);
  CHECK_EQ(visible.Pitch, plain.Pitch);
  const AllocationDescriptor descriptor = visible_request.descriptor();
  CHECK(descriptor.kind() == AllocationKind::cross_adapter);
  CHECK(descriptor.cpu_visible());
  CHECK(descriptor.size() == plain_request.descriptor().size());
  D3dkmdtGdiSurfaceDataRecord unknown = {16u, 16u, r5g6b5, 8, 0u, 0xEEu};
  CHECK_STATUS(Request(gdi, &unknown).describe(platform), Status::invalid_parameter);
  CHECK_EQ(unknown.Pitch, 0xEEu); }

TEST(standard_allocation_describe, a_lookup_table_or_existing_memory_surface_of_an_unlaid_out_format_is_refused)
{ MockPlatform platform;
  const std::int32_t types[] = {4, 5};
  for (std::int32_t type : types) {
      D3dkmdtGdiSurfaceDataRecord surface = {16u, 16u, r5g6b5, type, 0u, 0xEEu};
      CHECK_STATUS(Request(gdi, &surface).describe(platform), Status::invalid_parameter);
      CHECK_EQ(surface.Pitch, 0xEEu);
  } }

TEST(standard_allocation_describe, refuses_what_it_cannot_describe)
{ MockPlatform platform;
  const std::int32_t unsupported_gdi[] = {0, 9, -1};
  for (std::int32_t type : unsupported_gdi) {
      D3dkmdtGdiSurfaceDataRecord surface = {16u, 16u, a8r8g8b8, type, 0u, 0u};
      CHECK_STATUS(Request(gdi, &surface).describe(platform), Status::invalid_parameter);
  }
  D3dkmdtShadowSurfaceDataRecord surface = {16u, 16u, a8r8g8b8, 0u};
  const std::int32_t unsupported_types[] = {0, vgpu, fence_storage, 7, -1};
  for (std::int32_t type : unsupported_types)
      CHECK_STATUS(Request(type, &surface).describe(platform), Status::invalid_parameter);
  CHECK_STATUS(Request(shadow, nullptr).describe(platform), Status::invalid_parameter); }

TEST(standard_allocation_describe, refuses_an_unsupported_format_zero_dimensions_or_overflow)
{ MockPlatform platform;
  struct Case { std::uint32_t width; std::uint32_t height; std::int32_t format; };
  const Case cases[] = {
      {16u, 16u, r5g6b5},
      {0u, 16u, a8r8g8b8},
      {16u, 0u, a8r8g8b8},
      {0x40000000u, 1u, a8r8g8b8},
      {0x10000u, 0x4000u, a8r8g8b8},
  };
  for (const Case &c : cases) {
      D3dkmdtShadowSurfaceDataRecord surface = {c.width, c.height, c.format, 0xEEu};
      Request request(shadow, &surface);
      CHECK_STATUS(request.describe(platform), Status::invalid_parameter);
      CHECK_EQ(surface.Pitch, 0xEEu);
  } }

TEST(standard_allocation_describe, every_surface_type_refuses_a_layout_it_cannot_make)
{ MockPlatform platform;
  D3dkmdtSharedPrimarySurfaceDataRecord primary = {16u, 16u, r5g6b5, 60u, 1u, 0u};
  CHECK_STATUS(Request(shared_primary, &primary).describe(platform), Status::invalid_parameter);
  D3dkmdtStagingSurfaceDataRecord staging_surface = {0u, 16u, 0xEEu};
  CHECK_STATUS(Request(staging, &staging_surface).describe(platform), Status::invalid_parameter);
  CHECK_EQ(staging_surface.Pitch, 0xEEu);
  D3dkmdtGdiSurfaceDataRecord gdi_surface = {16u, 16u, r5g6b5, 1, 0u, 0xEEu};
  CHECK_STATUS(Request(gdi, &gdi_surface).describe(platform), Status::invalid_parameter);
  CHECK_EQ(gdi_surface.Pitch, 0xEEu); }

TEST(standard_allocation, is_passive_only)
{ MockPlatform platform; Request request(shared_primary, nullptr);
  request.record().pAllocationPrivateDriverData = nullptr;
  platform.set_irql(Irql::dispatch);
  static_cast<void>(request.describe(platform));
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling)); }

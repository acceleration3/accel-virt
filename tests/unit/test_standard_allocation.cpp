#include "../../src/core/ddi/d3dkmdt_gdi_surface_data_record.hpp"
#include "../../src/core/ddi/d3dkmdt_shadow_surface_data_record.hpp"
#include "../../src/core/ddi/d3dkmdt_shared_primary_surface_data_record.hpp"
#include "../../src/core/ddi/d3dkmdt_staging_surface_data_record.hpp"
#include "../../src/core/ddi/dxgkarg_get_standard_allocation_driver_data_record.hpp"
#include "../../src/core/standard_allocation.hpp"
#include "../../src/core/status.hpp"
#include "../../src/proto/allocation_descriptor.hpp"
#include "../harness/guarded_buffer.hpp"
#include "../harness/mock_platform.hpp"
#include "../harness/test_registry.hpp"
#include "../harness/violation.hpp"

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

}  // namespace

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
  const Case cases[] = {{1, false}, {2, true}, {3, false}, {6, true}};
  for (const Case &c : cases) {
      D3dkmdtGdiSurfaceDataRecord surface = {16u, 16u, a8r8g8b8, c.type, 0u, 0u};
      Request request(gdi, &surface);
      CHECK_STATUS(request.describe(platform), Status::ok);
      CHECK_EQ(surface.Pitch, 64u);
      const AllocationDescriptor descriptor = request.descriptor();
      CHECK(descriptor.kind() == AllocationKind::gdi);
      CHECK(descriptor.cpu_visible() == c.cpu_visible);
  } }

TEST(standard_allocation_describe, refuses_what_it_cannot_describe)
{ MockPlatform platform;
  const std::int32_t unsupported_gdi[] = {0, 4, 5, 7, 8, 9};
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
      {0x40000000u, 1u, a8r8g8b8},    /* pitch overflows 32 bits */
      {0x10000u, 0x4000u, a8r8g8b8},  /* size overflows 32 bits */
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

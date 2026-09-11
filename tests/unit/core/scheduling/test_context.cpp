#include "core/adapter/adapter.hpp"
#include "core/ddi/dxgkarg_create_context_record.hpp"
#include "core/ddi/dxgkarg_set_root_page_table_record.hpp"
#include "core/memory/gpu_mmu_geometry.hpp"
#include "core/scheduling/context.hpp"
#include "core/scheduling/device.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

#include <cstring>

using av::Adapter;
using av::Context;
using av::Device;
using av::DxgkargCreateContextRecord;
using av::DxgkargSetRootPageTableRecord;
using av::GpuMmuGeometry;
using av::Irql;
using av::MockPlatform;
using av::Status;
using av::Violation;

TEST(context, remembers_its_device_node_and_flags)
{ MockPlatform platform; int pdo = 0;
  Adapter adapter(platform, &pdo);
  Device device(platform, adapter, nullptr, 0u);
  const GpuMmuGeometry geometry(platform);
  const Context context(platform, device, geometry, 0u, 1u);
  CHECK(&context.device() == &device);
  CHECK_EQ(context.node(), 0u);
  CHECK_EQ(context.flags(), 1u); }

TEST(context_report_to, answers_the_context_info_and_the_handle)
{ MockPlatform platform; int pdo = 0;
  Adapter adapter(platform, &pdo);
  Device device(platform, adapter, nullptr, 0u);
  const GpuMmuGeometry geometry(platform);
  Context context(platform, device, geometry, 0u, 0u);
  DxgkargCreateContextRecord request;
  std::memset(&request, 0xEE, sizeof request);
  context.report_to(request);
  CHECK(request.hContext == &context);
  CHECK_EQ(request.ContextInfoDmaBufferSize, 0x10000u);
  CHECK_EQ(request.ContextInfoDmaBufferSegmentSet, 0u);
  CHECK_EQ(request.ContextInfoDmaBufferPrivateDataSize, 0u);
  CHECK_EQ(request.ContextInfoAllocationListSize, 256u);
  CHECK_EQ(request.ContextInfoPatchLocationListSize, 1024u);
  CHECK_EQ(request.ContextInfoReserved, 0u);
  CHECK_EQ(request.ContextInfoCaps, 0u);
  CHECK_EQ(request.ContextInfoPagingCompanionNodeId, 0u); }

TEST(context_assign_root, bounds_the_addresses_it_admits)
{ MockPlatform platform; int pdo = 0;
  Adapter adapter(platform, &pdo);
  Device device(platform, adapter, nullptr, 0u);
  const GpuMmuGeometry geometry(platform);
  Context context(platform, device, geometry, 0u, 0x4u);
  CHECK(context.admits(0x8000000000ull));
  const DxgkargSetRootPageTableRecord root = {&context, 2u, 0u, 0x3000u, 1u, 0u};
  CHECK_STATUS(context.assign_root(root), Status::ok);
  CHECK(context.admits(0x10000u));
  CHECK(!context.admits(0x8000000000ull)); }

TEST(context, is_legal_up_to_dispatch)
{ MockPlatform platform; int pdo = 0;
  Adapter adapter(platform, &pdo);
  Device device(platform, adapter, nullptr, 0u);
  const GpuMmuGeometry geometry(platform);
  Context context(platform, device, geometry, 0u, 0u);
  DxgkargCreateContextRecord request = {};
  platform.set_irql(Irql::dispatch);
  static_cast<void>(context.device());
  static_cast<void>(context.node());
  static_cast<void>(context.flags());
  context.report_to(request);
  CHECK_EQ(platform.unclaimed_violations(), 0u);
  platform.set_irql(Irql::isr);
  static_cast<void>(context.node());
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling)); }

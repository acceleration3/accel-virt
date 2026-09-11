#include "../../src/core/adapter.hpp"
#include "../../src/core/context.hpp"
#include "../../src/core/ddi/dxgkarg_create_context_record.hpp"
#include "../../src/core/device.hpp"
#include "../harness/mock_platform.hpp"
#include "../harness/test_registry.hpp"
#include "../harness/violation.hpp"

#include <cstring>

using av::Adapter;
using av::Context;
using av::Device;
using av::DxgkargCreateContextRecord;
using av::Irql;
using av::MockPlatform;
using av::Violation;

TEST(context, remembers_its_device_node_and_flags)
{ MockPlatform platform; int pdo = 0;
  Adapter adapter(platform, &pdo);
  Device device(platform, adapter, nullptr, 0u);
  const Context context(platform, device, 0u, 1u);
  CHECK(&context.device() == &device);
  CHECK_EQ(context.node(), 0u);
  CHECK_EQ(context.flags(), 1u); }

TEST(context_report_to, answers_the_context_info_and_the_handle)
{ MockPlatform platform; int pdo = 0;
  Adapter adapter(platform, &pdo);
  Device device(platform, adapter, nullptr, 0u);
  Context context(platform, device, 0u, 0u);
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

TEST(context, is_legal_up_to_dispatch)
{ MockPlatform platform; int pdo = 0;
  Adapter adapter(platform, &pdo);
  Device device(platform, adapter, nullptr, 0u);
  Context context(platform, device, 0u, 0u);
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

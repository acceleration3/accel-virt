#include "../../src/core/adapter.hpp"
#include "../../src/core/context.hpp"
#include "../../src/core/device.hpp"
#include "../../src/core/gpu_mmu_geometry.hpp"
#include "../../src/core/hw_queue.hpp"
#include "../harness/mock_platform.hpp"
#include "../harness/test_registry.hpp"
#include "../harness/violation.hpp"

using av::Adapter;
using av::Context;
using av::Device;
using av::GpuMmuGeometry;
using av::HwQueue;
using av::Irql;
using av::MockPlatform;
using av::Violation;

TEST(hw_queue_context, is_the_context_it_runs_on_and_leads_to_the_adapter)
{ MockPlatform platform; int pdo = 0;
  Adapter adapter(platform, &pdo);
  Device device(platform, adapter, nullptr, 0u);
  const GpuMmuGeometry geometry(platform);
  Context context(platform, device, geometry, 0u, 0x14u);
  const HwQueue queue(platform, context);
  CHECK(&queue.context() == &context);
  CHECK(&queue.context().device().adapter() == &adapter);
  platform.set_irql(Irql::apc);
  static_cast<void>(queue.context());
  CHECK_EQ(platform.unclaimed_violations(), 0u);
  platform.set_irql(Irql::dispatch);
  static_cast<void>(queue.context());
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling)); }

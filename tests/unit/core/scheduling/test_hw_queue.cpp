#include "core/adapter/adapter.hpp"
#include "core/memory/gpu_mmu_geometry.hpp"
#include "core/scheduling/context.hpp"
#include "core/scheduling/device.hpp"
#include "core/scheduling/hw_queue.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

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
  std::uint64_t progress = 0u;
  const HwQueue queue(platform, context, &progress);
  CHECK(&queue.context() == &context);
  CHECK(&queue.context().device().adapter() == &adapter);
  /* What dxgkrnl allocated for the queue's progress, kept for the driver to
   * write while the process submits for itself (feature 049). */
  CHECK(queue.progress() == &progress);
  platform.set_irql(Irql::apc);
  static_cast<void>(queue.context());
  static_cast<void>(queue.progress());
  CHECK_EQ(platform.unclaimed_violations(), 0u);
  /* A submission to a hardware queue arrives at DISPATCH and reaches the
   * context from the handle, so that is the ceiling; above it is not. */
  platform.set_irql(Irql::dispatch);
  static_cast<void>(queue.context());
  static_cast<void>(queue.progress());
  CHECK_EQ(platform.unclaimed_violations(), 0u);
  platform.set_irql(Irql::isr);
  static_cast<void>(queue.context());
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling)); }

#include "core/adapter/adapter.hpp"
#include "core/scheduling/device.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

using av::Adapter;
using av::Device;
using av::Irql;
using av::MockPlatform;
using av::Violation;

TEST(device, remembers_its_adapter_runtime_handle_flags_and_process)
{ MockPlatform platform; int pdo = 0, runtime = 0, process = 0;
  Adapter adapter(platform, &pdo);
  const Device device(platform, adapter, &runtime, 2u, &process);
  CHECK(&device.adapter() == &adapter);
  CHECK(device.runtime_handle() == &runtime);
  CHECK_EQ(device.flags(), 2u);
  CHECK(device.process() == &process); }

TEST(device, adapter_is_legal_at_any_irql_dxgkrnl_enters_a_device_ddi)
{ MockPlatform platform; int pdo = 0;
  Adapter adapter(platform, &pdo);
  const Device device(platform, adapter, nullptr, 0u, nullptr);
  platform.set_irql(Irql::dispatch);
  CHECK(&device.adapter() == &adapter);
  platform.set_irql(Irql::isr);
  static_cast<void>(device.flags());
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling));
  CHECK(!platform.claim(Violation::irql_ceiling)); }

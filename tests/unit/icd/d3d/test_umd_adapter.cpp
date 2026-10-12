/* The adapter and the connection under it (data-model.md): one open per
 * runtime, one kernel adapter for the process however many opens there are,
 * and a handle the table never gave answering nothing. */
#include "core/trace/state_writer.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "guest/kmt/adapter_search.hpp"
#include "guest/kmt/avgpu_adapters.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/d3d/adapter_table.hpp"
#include "icd/d3d/ddi/d3dddi_adapter_callbacks_record.hpp"
#include "icd/d3d/device_removal.hpp"
#include "icd/d3d/umd_binding.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "icd/d3d/umd_connection.hpp"
#include "icd/d3d/umd_root.hpp"

#include <cstdint>
#include <cstring>

using av::AdapterScenario;
using av::AdapterSearch;
using av::AvgpuAdapters;
using av::MockKmt;
using av::MockPlatform;
using av::PciIdentity;
using av::RecordingLog;
using av::Status;
using namespace av::d3d;

namespace {

constexpr std::uint32_t d3d11_interface = (11u << 16) | 45u;

const PciIdentity avgpu(AvgpuAdapters::virtio_vendor, AvgpuAdapters::virtio_gpu_device,
                         AvgpuAdapters::virtio_vendor, AvgpuAdapters::av_subsystem);

D3dddiAdapterCallbacksRecord callbacks;
int                          runtime_handle;
/* What an adapter made outside a root is opened over: none of these judge it. */
MockKmt                      no_kernel;
UmdConnection                unopened(no_kernel);

}  // namespace

TEST(umd_adapter, records_what_the_open_gave_and_counts_the_devices_made_on_it)
{ UmdAdapter adapter(RuntimeKind::d3d10_11, &runtime_handle, &callbacks, unopened, d3d11_interface, 26100u);
  CHECK(adapter.invariants_hold());
  CHECK(adapter.runtime() == RuntimeKind::d3d10_11);
  CHECK(adapter.runtime_adapter() == &runtime_handle);
  CHECK(adapter.callbacks() == &callbacks);
  CHECK_EQ(adapter.interface_version(), d3d11_interface);
  CHECK_EQ(adapter.runtime_version(), 26100u);
  CHECK_EQ(adapter.devices(), 0u);
  adapter.device_made();
  adapter.device_made();
  CHECK_EQ(adapter.devices(), 2u);
  CHECK(adapter.invariants_hold());
  adapter.device_gone();
  CHECK_EQ(adapter.devices(), 1u);
  adapter.device_gone();
  CHECK_EQ(adapter.devices(), 0u);
  /* A destroy with nothing left is a caller's mistake, not an underflow that
   * would let the adapter close with a device still on it. */
  adapter.device_gone();
  CHECK_EQ(adapter.devices(), 0u);
  CHECK(adapter.invariants_hold()); }

TEST(umd_adapter, a_d3d12_adapter_carries_no_interface_and_serves_its_own_version)
{ UmdAdapter adapter(RuntimeKind::d3d12, &runtime_handle, &callbacks, unopened, 0u, 0u);
  CHECK(adapter.invariants_hold());
  CHECK_EQ(adapter.interface_version(), 0u);
  CHECK(!adapter.versions().serves(d3d11_interface));
  CHECK(adapter.versions().serves(static_cast<std::uint32_t>(adapter.versions().word(0u) >> 32))); }

TEST(umd_adapter_invariants, fail_for_no_callbacks_or_a_version_the_open_does_not_serve)
{ const UmdAdapter without(RuntimeKind::d3d10_11, &runtime_handle, nullptr, unopened, d3d11_interface, 0u);
  CHECK(!without.invariants_hold());
  const UmdAdapter unserved(RuntimeKind::d3d10_11, &runtime_handle, &callbacks, unopened, d3d11_interface + 1u, 0u);
  CHECK(!unserved.invariants_hold());
  const UmdAdapter twelve_with_interface(RuntimeKind::d3d12, &runtime_handle, &callbacks, unopened, 1u, 0u);
  CHECK(!twelve_with_interface.invariants_hold()); }

TEST(adapter_table, opens_adapters_and_answers_only_the_handles_it_gave)
{ MockPlatform platform;
  AdapterTable table(platform);
  CHECK(table.invariants_hold());
  CHECK_EQ(table.open_adapters(), 0u);
  UmdAdapter *first = nullptr;
  CHECK_STATUS(table.open(first, RuntimeKind::d3d10_11, &runtime_handle, &callbacks, unopened, d3d11_interface, 0u),
               Status::ok);
  UmdAdapter *second = nullptr;
  CHECK_STATUS(table.open(second, RuntimeKind::d3d12, &runtime_handle, &callbacks, unopened, 0u, 0u), Status::ok);
  CHECK_EQ(table.open_adapters(), 2u);
  CHECK(first != second);
  CHECK(table.adapter_of(first) == first);
  CHECK(table.adapter_of(second) == second);
  CHECK(table.invariants_hold());

  int elsewhere = 0;
  CHECK(table.adapter_of(&elsewhere) == nullptr);
  CHECK(table.adapter_of(nullptr) == nullptr);
  CHECK_STATUS(table.close(&elsewhere), Status::invalid_parameter);
  CHECK_STATUS(table.close(nullptr), Status::invalid_parameter);

  CHECK_STATUS(table.close(first), Status::ok);
  CHECK_EQ(table.open_adapters(), 1u);
  /* Closed once is closed: the handle names nothing afterwards. */
  CHECK(table.adapter_of(first) == nullptr);
  CHECK_STATUS(table.close(first), Status::invalid_parameter);
  CHECK_STATUS(table.close(second), Status::ok);
  CHECK_EQ(table.open_adapters(), 0u);
  CHECK(table.invariants_hold()); }

TEST(adapter_table, refuses_to_close_an_adapter_a_device_is_still_made_on)
{ MockPlatform platform;
  AdapterTable table(platform);
  UmdAdapter  *adapter = nullptr;
  CHECK_STATUS(table.open(adapter, RuntimeKind::d3d10_11, &runtime_handle, &callbacks, unopened, d3d11_interface, 0u),
               Status::ok);
  if (adapter == nullptr) return;
  adapter->device_made();
  CHECK_STATUS(table.close(adapter), Status::conflict);
  CHECK_EQ(table.open_adapters(), 1u);
  adapter->device_gone();
  CHECK_STATUS(table.close(adapter), Status::ok);
  CHECK(table.invariants_hold()); }

TEST(adapter_table, an_open_with_no_memory_opens_nothing)
{ MockPlatform platform;
  AdapterTable table(platform);
  platform.fail_allocation_at(1u);
  UmdAdapter *adapter = nullptr;
  CHECK_STATUS(table.open(adapter, RuntimeKind::d3d10_11, &runtime_handle, &callbacks, unopened, d3d11_interface, 0u),
               Status::no_memory);
  CHECK(adapter == nullptr);
  CHECK_EQ(table.open_adapters(), 0u);
  CHECK(table.invariants_hold()); }

TEST(adapter_table, reports_every_adapter_it_holds)
{ MockPlatform platform;
  AdapterTable table(platform);
  UmdAdapter  *adapter = nullptr;
  CHECK_STATUS(table.open(adapter, RuntimeKind::d3d10_11, &runtime_handle, &callbacks, unopened, d3d11_interface, 0u),
               Status::ok);
  char bytes[512] = {};
  av::StateWriter writer(bytes, sizeof bytes);
  table.report(writer);
  CHECK(std::strstr(writer.dump(), "adapter/0 runtime=0") != nullptr);
  CHECK(std::strstr(writer.dump(), "interface=0x00000000000B002D") != nullptr);
  CHECK_STATUS(table.close(adapter), Status::ok); }

/* The kernel adapter is the process's, not the open's: two opens hold one, and
 * it goes with the last. */
TEST(umd_connection, holds_one_kernel_adapter_for_every_open)
{ MockKmt kmt;
  kmt.add_adapter(avgpu, AdapterScenario::wddm_3_2);
  UmdConnection connection(kmt);
  CHECK(connection.invariants_hold());
  CHECK(!connection.open());
  CHECK_EQ(connection.opens(), 0u);

  CHECK_STATUS(connection.acquire(0u), Status::ok);
  CHECK(connection.open());
  const std::uint32_t adapter = connection.adapter();
  CHECK(adapter != 0u);
  CHECK_EQ(kmt.open_objects(), 1u);

  CHECK_STATUS(connection.acquire(0u), Status::ok);
  CHECK_EQ(connection.opens(), 2u);
  CHECK_EQ(connection.adapter(), adapter);
  CHECK_EQ(kmt.open_objects(), 1u);
  CHECK(connection.invariants_hold());

  CHECK_STATUS(connection.release(), Status::ok);
  CHECK(connection.open());
  CHECK_EQ(kmt.open_objects(), 1u);
  CHECK_STATUS(connection.release(), Status::ok);
  CHECK(!connection.open());
  CHECK_EQ(kmt.open_objects(), 0u);
  CHECK(connection.invariants_hold());

  /* A release with nothing held would be a close without an open. */
  CHECK_STATUS(connection.release(), Status::invalid_state);
  CHECK(connection.invariants_hold()); }

TEST(umd_connection, no_avgpu_adapter_is_not_supported_and_holds_nothing)
{ MockKmt kmt;
  UmdConnection connection(kmt);
  CHECK_STATUS(connection.acquire(0u), Status::not_supported);
  CHECK(!connection.open());
  CHECK_EQ(connection.opens(), 0u);
  CHECK_EQ(kmt.open_objects(), 0u);
  CHECK(connection.invariants_hold()); }

/* The runtime opens a UMD on one adapter: with two avgpu adapters the
 * connection holds the one whose LUID it was given, not the first enumerated,
 * and a LUID no adapter has finds nothing. */
TEST(umd_connection, holds_the_adapter_with_the_luid_it_was_given)
{ MockKmt kmt;
  kmt.add_adapter(avgpu, AdapterScenario::wddm_3_2);
  kmt.add_adapter(avgpu, AdapterScenario::wddm_3_2);
  UmdConnection second(kmt);
  CHECK_STATUS(second.acquire(MockKmt::first_luid + 1u), Status::ok);
  std::uint64_t luid = 0u;
  CHECK_STATUS(kmt.adapter_luid(second.adapter(), luid), Status::ok);
  CHECK_EQ(luid, MockKmt::first_luid + 1u);
  CHECK_EQ(kmt.open_objects(), 1u);
  UmdConnection absent(kmt);
  CHECK_STATUS(absent.acquire(MockKmt::first_luid + 7u), Status::not_supported);
  CHECK(!absent.open());
  CHECK_EQ(kmt.open_objects(), 1u);
  CHECK_STATUS(second.release(), Status::ok); }

TEST(umd_connection, reports_what_it_holds)
{ MockKmt kmt;
  kmt.add_adapter(avgpu, AdapterScenario::wddm_3_2);
  UmdConnection connection(kmt);
  CHECK_STATUS(connection.acquire(0u), Status::ok);
  char bytes[256] = {};
  av::StateWriter writer(bytes, sizeof bytes);
  writer.open("connection", 0u);
  connection.report(writer);
  writer.close();
  CHECK(std::strstr(writer.dump(), "open=true") != nullptr);
  CHECK(std::strstr(writer.dump(), "opens=1") != nullptr);
  CHECK_STATUS(connection.release(), Status::ok); }

/* GetDeviceRemovedReason must name what happened, not what happened last: a
 * removal cascades, and every later cause is a consequence of the first. */
TEST(device_removal, keeps_the_first_cause_and_counts_the_rest)
{ RecordingLog  log;
  DeviceRemoval removal;
  CHECK(removal.invariants_hold());
  CHECK(!removal.removed());
  CHECK_EQ(removal.reason(), hr_ok);
  CHECK_EQ(removal.check(log, "pfnPresent"), hr_ok);
  CHECK_EQ(log.lines(), 0u);

  /* hr_ok is not a removal and is ignored. */
  removal.remove(hr_ok);
  CHECK(!removal.removed());
  CHECK(removal.invariants_hold());

  removal.remove(hr_dxgi_device_reset);
  CHECK(removal.removed());
  CHECK_EQ(removal.reason(), hr_dxgi_device_reset);
  removal.remove(hr_dxgi_device_removed);
  removal.remove(hr_dxgi_device_hung);
  CHECK_EQ(removal.reason(), hr_dxgi_device_reset);
  CHECK_EQ(removal.later_causes(), 2u);
  CHECK(removal.invariants_hold()); }

TEST(device_removal, logs_the_reason_once_however_many_callers_notice)
{ RecordingLog  log;
  DeviceRemoval removal;
  removal.remove(hr_dxgi_device_removed);
  CHECK_EQ(removal.check(log, "pfnBlt"), hr_dxgi_device_removed);
  CHECK_EQ(removal.check(log, "pfnPresent"), hr_dxgi_device_removed);
  CHECK_EQ(removal.check(log, "pfnSetDisplayMode"), hr_dxgi_device_removed);
  CHECK_EQ(log.lines(), 1u);
  CHECK_EQ(log.holding("device removed, pfnBlt answers 0x887A0005"), 1u);
  CHECK(removal.invariants_hold()); }

TEST(umd_binding, answers_the_root_it_is_bound_to_and_nothing_once_unbound)
{ UmdBinding binding;
  CHECK(!binding.bound());
  MockPlatform platform;
  MockKmt      kmt;
  kmt.add_adapter(avgpu, AdapterScenario::wddm_3_2);
  av::LockTrace           trace;
  av::RecordingThreadLock lock(trace, 'U');
  RecordingLog            log;
  { UmdRoot root(platform, kmt, lock, log, binding);
    CHECK(binding.bound());
    CHECK(&binding.root() == &root); }
  CHECK(!binding.bound()); }

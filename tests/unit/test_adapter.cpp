#include "../../src/core/adapter.hpp"
#include "../harness/mock_platform.hpp"
#include "../harness/test_registry.hpp"

#include <cstdint>

using av::Adapter;
using av::MockPlatform;

namespace {

constexpr std::uint32_t not_supported = 0xC00000BBu;

std::uint32_t as_unsigned(std::int32_t status) { return static_cast<std::uint32_t>(status); }

}  // namespace

TEST(adapter, remembers_its_physical_device_object)
{ MockPlatform platform; int pdo = 0;
  const Adapter adapter(platform, &pdo);
  CHECK(adapter.physical_device_object() == &pdo); }

TEST(adapter_phase_1, ddis_not_yet_implemented_answer_not_supported)
{ MockPlatform platform; int pdo = 0;
  Adapter adapter(platform, &pdo);
  std::uint32_t sources = 0u, children = 0u;
  CHECK_EQ(as_unsigned(adapter.start_device(nullptr, nullptr, &sources, &children)), not_supported);
  CHECK_EQ(as_unsigned(adapter.stop_device()), not_supported);
  CHECK_EQ(as_unsigned(adapter.dispatch_io_request(0u, nullptr)), not_supported);
  CHECK_EQ(adapter.interrupt_routine(0u), 0u);
  adapter.dpc_routine();
  CHECK_EQ(as_unsigned(adapter.query_child_relations(nullptr, 0u)), not_supported);
  CHECK_EQ(as_unsigned(adapter.query_child_status(nullptr, 0u)), not_supported);
  CHECK_EQ(as_unsigned(adapter.query_device_descriptor(0u, nullptr)), not_supported);
  CHECK_EQ(as_unsigned(adapter.set_power_state(0u, 1, 0)), not_supported);
  adapter.reset_device();
  CHECK_EQ(as_unsigned(adapter.query_interface(nullptr)), not_supported);
  CHECK_EQ(as_unsigned(adapter.link_device(&pdo, nullptr)), not_supported); }

TEST(adapter_notify_acpi_event, succeeds_and_asks_for_nothing)
{ MockPlatform platform; int pdo = 0;
  Adapter adapter(platform, &pdo);
  std::uint32_t flags = 3u;
  CHECK_EQ(adapter.notify_acpi_event(0, 0u, nullptr, &flags), 0);
  CHECK_EQ(flags, 0u); }

#include "core/trace/state_writer.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "guest/kmt/adapter_search.hpp"
#include "guest/kmt/avgpu_adapters.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "icd/d3d/umd_connection.hpp"
#include "icd/d3d/umd_connections.hpp"

#include <cstdint>
#include <cstring>

using av::AdapterScenario;
using av::AdapterSearch;
using av::AvgpuAdapters;
using av::MockKmt;
using av::PciIdentity;
using av::StateWriter;
using av::Status;
using av::d3d::UmdConnection;
using av::d3d::UmdConnections;

namespace {

const PciIdentity avgpu(PciIdentity::virtio_vendor, AvgpuAdapters::virtio_gpu_device,
                         PciIdentity::virtio_vendor, AvgpuAdapters::av_subsystem);

constexpr std::uint64_t first_luid = 0x0000000200001234ull;
constexpr std::uint64_t second_luid = 0x0000000300005678ull;

void add_adapters(MockKmt &kmt, std::uint32_t count)
{
    for (std::uint32_t index = 0u; index < count; index++) {
        kmt.add_adapter(avgpu, AdapterScenario::wddm_3_2);
        kmt.set_adapter_luid(index, index == 0u ? first_luid : index == 1u ? second_luid : first_luid + index);
    }
}

std::uint64_t luid_of(MockKmt &kmt, const UmdConnection &connection)
{
    std::uint64_t luid = 0u;
    return kmt.adapter_luid(connection.adapter(), luid) == Status::ok ? luid : 0u;
}

}

TEST(umd_connection, counts_opens_of_its_own_adapter_only)
{ MockKmt kmt;
  add_adapters(kmt, 2u);
  UmdConnection connection(kmt);
  CHECK(!connection.holds(second_luid));
  CHECK_STATUS(connection.acquire(second_luid), Status::ok);
  CHECK(connection.holds(second_luid));
  CHECK(!connection.holds(first_luid));
  CHECK_EQ(luid_of(kmt, connection), second_luid);
  CHECK_STATUS(connection.acquire(first_luid), Status::conflict);
  CHECK_EQ(connection.opens(), 1u);
  CHECK_EQ(kmt.open_objects(), 1u);
  CHECK_STATUS(connection.release(), Status::ok);
  CHECK(!connection.holds(second_luid));
  CHECK(connection.invariants_hold()); }

TEST(umd_connection, shares_an_adapter_only_with_one_open_on_its_luid)
{ MockKmt kmt;
  add_adapters(kmt, 2u);
  UmdConnection first(kmt);
  UmdConnection other(kmt);
  CHECK(!first.shares_adapter(other));
  CHECK_STATUS(first.acquire(first_luid), Status::ok);
  CHECK(!first.shares_adapter(other));
  CHECK_STATUS(other.acquire(second_luid), Status::ok);
  CHECK(!first.shares_adapter(other));
  CHECK_STATUS(other.release(), Status::ok);
  CHECK_STATUS(other.acquire(first_luid), Status::ok);
  CHECK(first.shares_adapter(other));
  CHECK(other.shares_adapter(first));
  CHECK_STATUS(other.release(), Status::ok);
  CHECK_STATUS(first.release(), Status::ok); }

TEST(umd_connections, hold_one_kernel_adapter_per_luid)
{ MockKmt kmt;
  add_adapters(kmt, 2u);
  UmdConnections connections(kmt);
  UmdConnection *first = nullptr;
  UmdConnection *second = nullptr;
  UmdConnection *again = nullptr;
  CHECK_STATUS(connections.acquire(first_luid, first), Status::ok);
  CHECK_STATUS(connections.acquire(second_luid, second), Status::ok);
  CHECK_STATUS(connections.acquire(second_luid, again), Status::ok);
  if (first == nullptr || second == nullptr) return;
  CHECK(first != second);
  CHECK(again == second);
  CHECK_EQ(luid_of(kmt, *first), first_luid);
  CHECK_EQ(luid_of(kmt, *second), second_luid);
  CHECK_EQ(kmt.open_objects(), 2u);
  CHECK_EQ(connections.opens(), 3u);
  CHECK_EQ(connections.open(), 2u);
  CHECK(connections.invariants_hold());

  CHECK_STATUS(second->release(), Status::ok);
  CHECK(second->holds(second_luid));
  CHECK_STATUS(second->release(), Status::ok);
  CHECK_EQ(connections.open(), 1u);
  CHECK_EQ(kmt.open_objects(), 1u);
  CHECK_STATUS(first->release(), Status::ok);
  CHECK_EQ(connections.open(), 0u);
  CHECK_EQ(kmt.open_objects(), 0u);
  CHECK(connections.invariants_hold()); }

TEST(umd_connections, refuse_past_the_last_connection_and_an_adapter_not_there)
{ MockKmt kmt;
  UmdConnections connections(kmt);
  UmdConnection  unrelated(kmt);
  UmdConnection *held = &unrelated;
  CHECK_STATUS(connections.acquire(second_luid, held), Status::not_supported);
  CHECK(held == nullptr);
  CHECK_EQ(connections.open(), 0u);

  add_adapters(kmt, UmdConnections::max_adapters);
  for (std::uint32_t index = 0u; index < UmdConnections::max_adapters; index++) {
      const std::uint64_t luid = index == 0u ? first_luid : index == 1u ? second_luid : first_luid + index;
      CHECK_STATUS(connections.acquire(luid, held), Status::ok);
  }
  CHECK_EQ(connections.open(), UmdConnections::max_adapters);
  CHECK_STATUS(connections.acquire(0x0000000900000001ull, held), Status::no_space);
  CHECK(held == nullptr);
  CHECK_EQ(kmt.open_objects(), UmdConnections::max_adapters);
  CHECK(connections.invariants_hold()); }

TEST(umd_connections, report_each_open_connection_and_its_luid)
{ MockKmt kmt;
  add_adapters(kmt, 2u);
  UmdConnections connections(kmt);
  UmdConnection *held = nullptr;
  CHECK_STATUS(connections.acquire(second_luid, held), Status::ok);
  char        text[512] = {};
  StateWriter writer(text, sizeof text);
  connections.report(writer);
  CHECK(std::strstr(writer.dump(), "connection/0") != nullptr);
  CHECK(std::strstr(writer.dump(), "connection/1") == nullptr);
  CHECK(std::strstr(writer.dump(), "luid=0x0000000300005678") != nullptr);
  if (held != nullptr) CHECK_STATUS(held->release(), Status::ok); }

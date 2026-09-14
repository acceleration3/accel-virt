/* The context a scenario runs over, over MockKmt: opening reports each step up
 * to the first that fails and attempts nothing after it; close gives back
 * everything open whatever failed, twice being harmless; and a second context
 * opened on its connection shares the device. */
#include "core/support/status.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "guest/suite/adapter_search.hpp"
#include "guest/suite/guest_report.hpp"
#include "guest/suite/scenario_steps.hpp"
#include "guest/suite/scenario_transport.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "proto/context/rvk_context_record.hpp"

#include <cstdint>
#include <cstring>

using av::AdapterScenario;
using av::AdapterSearch;
using av::GuestReport;
using av::KmtTransport;
using av::MockKmt;
using av::PciIdentity;
using av::ScenarioSteps;
using av::ScenarioTransport;
using av::Status;
using av::SubApi;

namespace {

const PciIdentity rvk(AdapterScenario::virtio_vendor, AdapterScenario::virtio_gpu_device,
                      AdapterScenario::virtio_vendor, AdapterScenario::rvk_subsystem);

}  // namespace

TEST(scenario_transport, opens_a_context_of_its_sub_api_on_the_adapter_found)
{ MockKmt kmt;
  kmt.add_adapter(rvk, AdapterScenario::wddm_3_2);
  AdapterSearch search(kmt);
  CHECK_STATUS(search.find(rvk), Status::ok);
  GuestReport   report;
  ScenarioSteps steps(kmt, report, "probe");
  ScenarioTransport link(kmt, search, steps, SubApi::opencl);
  CHECK_STATUS(link.open(), Status::ok);
  CHECK(std::strcmp(report.text(), "ok probe.adapter\nok probe.connection\nok probe.initialize\n") == 0);
  CHECK(link.transport().established());
  CHECK_EQ(link.connection().adapter(), search.adapter());
  av::RvkContextRecord record;
  CHECK_STATUS(record.decode(kmt.last_private(), kmt.last_private_size()), Status::ok);
  CHECK(record.sub_api() == SubApi::opencl);
  CHECK_EQ(record.ring_order(), ScenarioTransport::ring_order);
  /* A second context on the same connection is on the same device. */
  KmtTransport second(link.connection(), SubApi::vulkan, ScenarioTransport::ring_order);
  CHECK_STATUS(second.initialize(), Status::ok);
  CHECK_EQ(kmt.calls(MockKmt::Call::create_device), 1u);
  second.teardown();
  link.close();
  CHECK(!link.transport().established());
  CHECK(!link.connection().opened());
  link.close();
  CHECK_EQ(kmt.open_objects(), 1u);
  CHECK_EQ(kmt.misuses(), 0u);
  CHECK_STATUS(search.close(), Status::ok); }

TEST(scenario_transport, stops_at_the_first_step_that_fails_and_leaves_nothing_open)
{ { MockKmt kmt;   /* no adapter: nothing asked of D3DKMT */
    AdapterSearch search(kmt);
    GuestReport   report;
    ScenarioSteps steps(kmt, report, "probe");
    ScenarioTransport link(kmt, search, steps, SubApi::vulkan);
    CHECK_STATUS(link.open(), Status::not_found);
    CHECK(std::strcmp(report.text(), "FAIL probe.adapter status=3 code=0x00000000\n") == 0);
    CHECK_EQ(kmt.calls(MockKmt::Call::create_device), 0u); }
  { MockKmt kmt;
    kmt.add_adapter(rvk, AdapterScenario::wddm_3_2);
    AdapterSearch search(kmt);
    CHECK_STATUS(search.find(rvk), Status::ok);
    kmt.fail_at(MockKmt::Call::create_device, 1u, 0xC0000017u);
    GuestReport   report;
    ScenarioSteps steps(kmt, report, "probe");
    { ScenarioTransport link(kmt, search, steps, SubApi::vulkan);
      CHECK_STATUS(link.open(), Status::no_memory); }
    CHECK(std::strcmp(report.text(), "ok probe.adapter\nFAIL probe.connection status=2 code=0xC0000017\n") == 0);
    CHECK_EQ(kmt.open_objects(), 1u);
    CHECK_STATUS(search.close(), Status::ok); }
  { MockKmt kmt;
    kmt.add_adapter(rvk, AdapterScenario::wddm_3_2);
    AdapterSearch search(kmt);
    CHECK_STATUS(search.find(rvk), Status::ok);
    kmt.fail_at(MockKmt::Call::create_context_virtual, 1u, 0xC0000017u);
    GuestReport   report;
    ScenarioSteps steps(kmt, report, "probe");
    { ScenarioTransport link(kmt, search, steps, SubApi::vulkan);
      CHECK(link.open() != Status::ok); }   /* the destructor closes what opened */
    CHECK(std::strstr(report.text(), "ok probe.connection\nFAIL probe.initialize") != nullptr);
    CHECK_EQ(kmt.open_objects(), 1u);
    CHECK_EQ(kmt.misuses(), 0u);
    CHECK_STATUS(search.close(), Status::ok); } }

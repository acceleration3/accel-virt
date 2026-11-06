#include "core/support/status.hpp"
#include "guest/kmt/avgpu_adapters.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/probe/host_loss_request.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "proto/submission/command_header.hpp"

#include <cstdint>

using av::AdapterScenario;
using av::AvgpuAdapters;
using av::CommandHeader;
using av::HostLossRequest;
using av::MockKmt;
using av::PciIdentity;
using av::Status;

namespace {

const PciIdentity av_gpu(PciIdentity::virtio_vendor, AvgpuAdapters::virtio_gpu_device, PciIdentity::virtio_vendor,
                         AvgpuAdapters::av_subsystem);

void answer_with(MockKmt &kmt, std::uint32_t answer)
{
    std::uint8_t bytes[CommandHeader::lose_host_device_answer] = {};
    for (std::uint32_t at = 0u; at < 4u; at++) bytes[at] = static_cast<std::uint8_t>(answer >> (8u * at));
    kmt.answer_waits(bytes, sizeof bytes);
}

}

TEST(host_loss_request, tells_the_host_to_lose_its_device_through_a_connection_of_its_own)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, AdapterScenario::wddm_3_2);
  answer_with(kmt, CommandHeader::host_device_lost);
  HostLossRequest request(kmt);
  CHECK_STATUS(request.send(), Status::ok);
  CHECK_EQ(kmt.open_objects(), 0u);
  CHECK_EQ(kmt.misuses(), 0u); }

TEST(host_loss_request, a_host_that_keeps_its_device_is_not_supported)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, AdapterScenario::wddm_3_2);
  answer_with(kmt, CommandHeader::host_device_kept);
  HostLossRequest request(kmt);
  CHECK_STATUS(request.send(), Status::not_supported);
  CHECK_EQ(kmt.open_objects(), 0u); }

TEST(host_loss_request, no_adapter_a_device_or_a_context_to_ask_through_asks_nothing)
{ { MockKmt kmt;
    HostLossRequest request(kmt);
    CHECK(request.send() != Status::ok);
    CHECK_EQ(kmt.open_objects(), 0u); }
  { MockKmt kmt;
    kmt.add_adapter(av_gpu, AdapterScenario::wddm_3_2);
    kmt.fail_at(MockKmt::Call::create_device, 1u, MockKmt::invalid_parameter);
    HostLossRequest request(kmt);
    CHECK(request.send() != Status::ok);
    CHECK_EQ(kmt.open_objects(), 0u);
    CHECK_EQ(kmt.calls(MockKmt::Call::create_context_virtual), 0u); }
  { MockKmt kmt;
    kmt.add_adapter(av_gpu, AdapterScenario::wddm_3_2);
    kmt.fail_at(MockKmt::Call::create_allocation, 1u, MockKmt::invalid_parameter);
    HostLossRequest request(kmt);
    CHECK(request.send() != Status::ok);
    CHECK_EQ(kmt.open_objects(), 0u);
    CHECK_EQ(kmt.calls(MockKmt::Call::submit), 0u); } }

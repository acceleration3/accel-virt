/* The host Vulkan device a scenario stands on, over MockKmt with the test
 * playing the host through ScriptedHost: made in order with each step
 * reported, the host's handles kept, and destroyed device before instance,
 * naming only what was made. */
#include "core/support/status.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "guest/suite/adapter_search.hpp"
#include "guest/suite/guest_report.hpp"
#include "guest/suite/scenario_steps.hpp"
#include "guest/suite/scenario_transport.hpp"
#include "guest/suite/vulkan_device_chain.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/generated/vulkan/vk_protocol_ids.h"
#include "proto/submission/command_header.hpp"

#include <cstdint>
#include <cstring>

namespace av {

struct VulkanDeviceChainProbe {
    /* A device held without the physical device it was made on. */
    static void orphan_device(VulkanDeviceChain &chain) { chain.physical_device_ = 0u; }
    /* A physical device held without its instance. */
    static void orphan_physical_device(VulkanDeviceChain &chain) { chain.device_ = 0u; chain.instance_ = 0u; }
};

}  // namespace av

using av::AdapterScenario;
using av::AdapterSearch;
using av::CommandHeader;
using av::GuestReport;
using av::MockKmt;
using av::PciIdentity;
using av::ScenarioSteps;
using av::ScenarioTransport;
using av::ScriptedHost;
using av::Status;
using av::SubApi;
using av::VulkanDeviceChain;
using av::VulkanDeviceChainProbe;
using namespace av::vkproto;

namespace {

const PciIdentity av(AdapterScenario::virtio_vendor, AdapterScenario::virtio_gpu_device,
                      AdapterScenario::virtio_vendor, AdapterScenario::av_subsystem);
constexpr std::int32_t vk_error_incompatible_driver = -9;

class Rig {
public:
    Rig()
        : search_(kmt_), host_(kmt_), steps_(kmt_, report_, "chain"), link_(kmt_, search_, steps_, SubApi::vulkan),
          chain_(steps_, link_.transport())
    {
        kmt_.add_adapter(av, AdapterScenario::wddm_3_2);
        CHECK_STATUS(search_.find(av), Status::ok);
        CHECK_STATUS(link_.open(), Status::ok);
    }

    ~Rig()
    {
        link_.close();
        CHECK_STATUS(search_.close(), Status::ok);
        CHECK_EQ(kmt_.open_objects(), 0u);
        CHECK_EQ(kmt_.misuses(), 0u);
    }

    MockKmt           &kmt() { return kmt_; }
    ScriptedHost      &host() { return host_; }
    GuestReport       &report() { return report_; }
    VulkanDeviceChain &chain() { return chain_; }

private:
    MockKmt           kmt_;
    AdapterSearch     search_;
    ScriptedHost      host_;
    GuestReport       report_;
    ScenarioSteps     steps_;
    ScenarioTransport link_;
    VulkanDeviceChain chain_;
};

}  // namespace

TEST(vulkan_device_chain, make_reports_each_step_and_keeps_the_hosts_handles)
{ Rig rig;
  rig.host().vk_handle(0, ScriptedHost::vk_instance);
  rig.host().vk_count(0, 2u);
  rig.host().vk_physical_devices(0, 2u);
  rig.host().vk_handle(0, ScriptedHost::vk_device);
  CHECK(rig.chain().device() == VK_NULL_HANDLE);
  CHECK(rig.chain().invariants_hold());
  CHECK_STATUS(rig.chain().make(), Status::ok);
  CHECK(rig.chain().invariants_hold());
  CHECK(std::strstr(rig.report().text(), "ok chain.instance\nok chain.physical_device 2\nok chain.device\n") !=
        nullptr);
  CHECK_EQ(reinterpret_cast<std::uint64_t>(rig.chain().device()), ScriptedHost::vk_device);
  CHECK_EQ(reinterpret_cast<std::uint64_t>(rig.chain().physical_device()), ScriptedHost::vk_physical_device);
  const std::uint32_t made = rig.kmt().commands();
  rig.chain().destroy();
  CHECK(rig.chain().invariants_hold());
  CHECK(rig.chain().device() == VK_NULL_HANDLE);
  CHECK(rig.chain().physical_device() == VK_NULL_HANDLE);
  /* The destroys went out without a wait: the device's, then the instance's. */
  CHECK_EQ(rig.kmt().commands(), made + 2u);
  CHECK_EQ(rig.kmt().command_id(made), static_cast<std::uint32_t>(kCmd_vkDestroyDevice));
  CHECK_EQ(rig.kmt().command_id(made + 1u), static_cast<std::uint32_t>(kCmd_vkDestroyInstance));
  /* Destroyed twice sends nothing more. */
  rig.chain().destroy();
  CHECK_EQ(rig.kmt().commands(), made + 2u); }

/* A device the host refused leaves the instance to destroy; an instance it
 * refused leaves nothing, and nothing is named to the host. */
TEST(vulkan_device_chain, a_refusal_destroys_only_what_was_made)
{ { Rig rig;
    rig.host().vk_handle(0, ScriptedHost::vk_instance);
    rig.host().vk_count(0, 1u);
    rig.host().vk_physical_devices(0, 1u);
    rig.host().vk_handle(vk_error_incompatible_driver, 0u);
    CHECK_STATUS(rig.chain().make(), Status::invalid_state);
    CHECK(rig.chain().invariants_hold());
    CHECK(rig.chain().device() == VK_NULL_HANDLE);
    CHECK(std::strstr(rig.report().text(), "FAIL chain.device status=7 4294967287\n") != nullptr);
    const std::uint32_t made = rig.kmt().commands();
    rig.chain().destroy();
    CHECK_EQ(rig.kmt().commands(), made + 1u);
    CHECK_EQ(rig.kmt().command_id(made), static_cast<std::uint32_t>(kCmd_vkDestroyInstance)); }
  { Rig rig;
    rig.host().vk_handle(vk_error_incompatible_driver, 0u);
    CHECK_STATUS(rig.chain().make(), Status::invalid_state);
    CHECK(std::strstr(rig.report().text(), "chain.physical_device") == nullptr);
    const std::uint32_t made = rig.kmt().commands();
    CHECK_EQ(made, 1u);
    rig.chain().destroy();
    CHECK_EQ(rig.kmt().commands(), made); } }

TEST(vulkan_device_chain, invariants_refuse_a_handle_held_without_its_parent)
{ Rig rig;
  rig.host().vk_handle(0, ScriptedHost::vk_instance);
  rig.host().vk_count(0, 1u);
  rig.host().vk_physical_devices(0, 1u);
  rig.host().vk_handle(0, ScriptedHost::vk_device);
  CHECK_STATUS(rig.chain().make(), Status::ok);
  VulkanDeviceChainProbe::orphan_device(rig.chain());
  CHECK(!rig.chain().invariants_hold());
  rig.chain().destroy();
  CHECK(rig.chain().invariants_hold());
  rig.host().vk_handle(0, ScriptedHost::vk_instance);
  rig.host().vk_count(0, 1u);
  rig.host().vk_physical_devices(0, 1u);
  rig.host().vk_handle(0, ScriptedHost::vk_device);
  CHECK_STATUS(rig.chain().make(), Status::ok);
  VulkanDeviceChainProbe::orphan_physical_device(rig.chain());
  CHECK(!rig.chain().invariants_hold());
  rig.chain().destroy(); }

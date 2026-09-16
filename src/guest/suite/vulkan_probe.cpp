#include "guest/suite/vulkan_probe.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "guest/suite/adapter_search.hpp"
#include "guest/suite/probe_device.hpp"
#include "guest/suite/probe_outcome.hpp"
#include "guest/suite/vulkan_entry.hpp"

namespace av {

namespace {

constexpr uint32_t low_part_bits = 32u;

uint32_t low_part(uint64_t luid)
{
    AV_SCOPE("low_part");
    return static_cast<uint32_t>(luid & ((uint64_t{1} << low_part_bits) - 1u));
}

}  // namespace

VulkanProbe::VulkanProbe(Kmt &kmt, GuestReport &report, VulkanEntry &vulkan)
    : kmt_(kmt), vulkan_(vulkan), steps_(kmt, report, "vulkan"), adapter_luid_(0u)
    {
        AV_SCOPE("VulkanProbe::VulkanProbe");
    }

void VulkanProbe::run()
{
    AV_SCOPE("VulkanProbe::run");
    {
        ProbeDevice device(vulkan_);
        if (steps_.check_result("instance", device.make_instance()) != Status::ok) return;
        if (adapter_step() != Status::ok || physical_device_step(device) != Status::ok) return;
        if (device_step(device) != Status::ok) return;
        {
            ComputeWork  compute(device);
            ProbeOutcome computed;
            compute.run(computed);
            computed.report(steps_, "compute");
        }
        {
            DrawWork     draw(device);
            ProbeOutcome drawn;
            draw.run(drawn);
            drawn.report(steps_, "draw");
        }
        lost_step(device);
    }
    recreated_step();
}

Status VulkanProbe::adapter_step()
{
    AV_SCOPE("VulkanProbe::adapter_step");
    const PciIdentity rvkgpu(AdapterScenario::virtio_vendor, AdapterScenario::virtio_gpu_device,
                             AdapterScenario::virtio_vendor, AdapterScenario::rvk_subsystem);
    AdapterSearch     search(kmt_);
    Status            found = search.find(rvkgpu);
    if (found == Status::ok) found = kmt_.adapter_luid(search.adapter(), adapter_luid_);
    return steps_.check_measured("adapter_luid", found, low_part(adapter_luid_));
}

Status VulkanProbe::physical_device_step(ProbeDevice &device)
{
    AV_SCOPE("VulkanProbe::physical_device_step");
    const VkResult listed = device.list_physical_devices();
    if (listed != VK_SUCCESS) return steps_.check_result("physical_device", listed);
    if (!device.choose(adapter_luid_))
        return steps_.check_measured("physical_device", Status::not_found, device.listed());
    return steps_.check_measured("physical_device", Status::ok, low_part(adapter_luid_));
}

Status VulkanProbe::device_step(ProbeDevice &device)
{
    AV_SCOPE("VulkanProbe::device_step");
    if (!device.choose_family()) return steps_.check_measured("device", Status::not_found, 0u);
    return steps_.check_result("device", device.make_device());
}

/* The fault is the first call; the device's next call must see the loss too,
 * without the probe telling it anything. */
void VulkanProbe::lost_step(ProbeDevice &device)
{
    AV_SCOPE("VulkanProbe::lost_step");
    const VkResult faulted = vulkan_.force_context_fault(device.instance(), device.physical_device());
    VkResult       unexpected = faulted;
    if (faulted == VK_ERROR_DEVICE_LOST) unexpected = vulkan_.device_wait_idle(device.device());
    if (unexpected == VK_ERROR_DEVICE_LOST) {
        static_cast<void>(steps_.check("lost", Status::ok));
        return;
    }
    static_cast<void>(steps_.check_measured("lost", Status::conflict, static_cast<uint32_t>(unexpected)));
}

void VulkanProbe::recreated_step()
{
    AV_SCOPE("VulkanProbe::recreated_step");
    ProbeDevice  device(vulkan_);
    ProbeOutcome outcome;
    const bool   made = outcome.accept(device.make_instance()) && outcome.accept(device.list_physical_devices());
    if (!made) {
        outcome.report(steps_, "recreated");
        return;
    }
    if (!device.choose(adapter_luid_) || !device.choose_family()) {
        outcome.missing();
        outcome.report(steps_, "recreated");
        return;
    }
    if (outcome.accept(device.make_device())) {
        ComputeWork compute(device);
        compute.run(outcome);
    }
    outcome.report(steps_, "recreated");
}

}  // namespace av

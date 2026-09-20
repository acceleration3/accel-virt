#include "guest/suite/shared_object_probe.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "guest/suite/adapter_search.hpp"
#include "guest/suite/guest_report.hpp"
#include "guest/suite/probe_device.hpp"
#include "guest/suite/vulkan_entry.hpp"

namespace av {

namespace {

constexpr const char *scenario = "shared";
constexpr const char *win32_memory = "VK_KHR_external_memory_win32";

/* A VkResult read as a Status, so a step's report carries the NTSTATUS of the
 * last kernel call rather than the Vulkan number: which of the four D3DKMT
 * calls refused is the thing a guest run is for, and the Vulkan error is the
 * same VK_ERROR_TOO_MANY_OBJECTS whichever of them it was (feature 053). */
Status answered(VkResult result)
{
    AV_SCOPE("answered");
    return result == VK_SUCCESS ? Status::ok : Status::invalid_state;
}

bool names(const VkExtensionProperties &listed, const char *wanted)
{
    AV_SCOPE("names");
    for (uint32_t index = 0u; index < sizeof listed.extensionName; index++) {
        if (listed.extensionName[index] != wanted[index]) return false;
        if (wanted[index] == '\0') return true;
    }
    return false;
}

}  // namespace

SharedObjectProbe::SharedObjectProbe(Kmt &kmt, GuestReport &report, VulkanEntry &vulkan)
    : kmt_(kmt), vulkan_(vulkan), steps_(kmt, report, scenario)
{
    AV_SCOPE("SharedObjectProbe::SharedObjectProbe");
}

bool SharedObjectProbe::reported_step(ProbeDevice &device)
{
    AV_SCOPE("SharedObjectProbe::reported_step");
    uint32_t       count = 0u;
    const VkResult asked = vulkan_.enumerate_device_extensions(device.physical_device(), &count, nullptr);
    if (asked != VK_SUCCESS || count == 0u || count > max_extensions) return false;
    VkExtensionProperties listed[max_extensions] = {};
    const VkResult        got = vulkan_.enumerate_device_extensions(device.physical_device(), &count, listed);
    if (got != VK_SUCCESS && got != VK_INCOMPLETE) return false;
    for (uint32_t index = 0u; index < count; index++)
        if (names(listed[index], win32_memory)) return true;
    return false;
}

bool SharedObjectProbe::exportable_step(ProbeDevice &device, VkDeviceMemory &memory)
{
    AV_SCOPE("SharedObjectProbe::exportable_step");
    uint32_t type = 0u;
    if (!device.memory_type(~uint32_t{0}, 0u, type)) {
        steps_.check("exportable", Status::not_supported);
        return false;
    }
    /* The node that makes the miniport mint: without it this is an ordinary
     * allocation and no identity is asked for. */
    VkExportMemoryAllocateInfo exporting = {};
    exporting.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
    exporting.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
    VkMemoryAllocateInfo allocate = {};
    allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate.pNext = &exporting;
    allocate.allocationSize = bytes;
    allocate.memoryTypeIndex = type;
    const VkResult made = vulkan_.allocate_memory(device.device(), &allocate, &memory);
    /* The driver's NTSTATUS, not this probe's: the kernel call was made inside
     * the ICD, by its own Kmt, and every one of them answers the application
     * the same VK_ERROR_TOO_MANY_OBJECTS. */
    return steps_.check_kernel("exportable", answered(made), vulkan_.last_kernel_status(device.instance())) ==
               Status::ok &&
           memory != VK_NULL_HANDLE;
}

bool SharedObjectProbe::handle_step(ProbeDevice &device, VkDeviceMemory memory, void *&handle)
{
    AV_SCOPE("SharedObjectProbe::handle_step");
    const VkResult got = vulkan_.memory_win32_handle(device.device(), memory, &handle);
    const Status   status = got == VK_SUCCESS && handle != nullptr ? Status::ok : Status::invalid_state;
    return steps_.check_kernel("handle", status, vulkan_.last_kernel_status(device.instance())) == Status::ok;
}

void SharedObjectProbe::released_step(ProbeDevice &device, VkDeviceMemory memory)
{
    AV_SCOPE("SharedObjectProbe::released_step");
    /* Freeing the memory releases the object on the device, closes the NT
     * handle and destroys the carrier allocation the identity was minted into.
     * What says it worked is that the kernel refused none of it. */
    vulkan_.free_memory(device.device(), memory);
    const uint32_t refused = vulkan_.last_kernel_status(device.instance());
    steps_.check_kernel("released", refused == 0u ? Status::ok : Status::invalid_state, refused);
}

void SharedObjectProbe::run()
{
    AV_SCOPE("SharedObjectProbe::run");
    /* The rvkgpu adapter's own physical device: an export on anybody else's
     * would prove nothing about this driver. */
    const PciIdentity rvkgpu(AdapterScenario::virtio_vendor, AdapterScenario::virtio_gpu_device,
                             AdapterScenario::virtio_vendor, AdapterScenario::rvk_subsystem);
    AdapterSearch     search(kmt_);
    uint64_t          luid = 0u;
    Status            found = search.find(rvkgpu);
    if (found == Status::ok) found = kmt_.adapter_luid(search.adapter(), luid);
    if (steps_.check("adapter", found) != Status::ok) return;

    ProbeDevice device(vulkan_);
    if (steps_.check("instance", answered(device.make_instance())) != Status::ok) return;
    if (steps_.check("physical_device", answered(device.list_physical_devices())) != Status::ok) return;
    if (steps_.check("chosen", device.choose(luid) ? Status::ok : Status::not_found) != Status::ok) return;
    if (steps_.check("family", device.choose_family() ? Status::ok : Status::not_found) != Status::ok) return;
    /* A host with no fd sharing offers these to nobody, and there is nothing
     * here to prove on it: the step records what it found and the rest do not
     * run, exactly as the window-system steps do without a window. Asked
     * before the device is made, because a device made without the extension
     * may not be asked for an export. */
    const bool offered = reported_step(device);
    steps_.check("reported", offered ? Status::ok : Status::not_supported);
    if (!offered) return;
    const char *const shared_extensions[] = {win32_memory};
    if (steps_.check("device", answered(device.make_device(shared_extensions, 1u))) != Status::ok) return;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    if (!exportable_step(device, memory)) return;
    void *handle = nullptr;
    static_cast<void>(handle_step(device, memory, handle));
    released_step(device, memory);
}

}  // namespace av

#include "guest/probe/shared_object_probe.hpp"

#include "guest/probe/cl_entry.hpp"

#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"
#include "guest/kmt/adapter_search.hpp"
#include "guest/suite/guest_report.hpp"
#include "guest/probe/probe_commands.hpp"
#include "guest/probe/probe_device.hpp"
#include "guest/probe/probe_outcome.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/allocation_kind.hpp"
#include "proto/resources/scanout_target.hpp"
#include "proto/resources/shared_object_kind.hpp"
#include "proto/resources/shared_object_record.hpp"
#include "proto/resources/surface_layout.hpp"
#include "guest/probe/vulkan_entry.hpp"

namespace av {

namespace {

constexpr const char *scenario = "shared";
constexpr uint64_t external_memory_win32 = 0x2061u;
constexpr uint64_t cl_read_write = 1u;
constexpr uint32_t cl_device_extensions = 0x1030u;
constexpr const char *cl_external_memory = "cl_khr_external_memory_win32";
constexpr uint32_t filled_word = 0xABCD0000u;
constexpr const char *win32_memory = "VK_KHR_external_memory_win32";

Status answered(VkResult result)
{
    AV_SCOPE("answered");
    return result == VK_SUCCESS ? Status::ok : Status::invalid_state;
}

bool offers(const char *listed, const char *name)
{
    AV_SCOPE("offers");
    for (uint32_t at = 0u; listed[at] != '\0';) {
        while (listed[at] == ' ') at++;
        uint32_t end = at;
        while (listed[end] != '\0' && listed[end] != ' ') end++;
        uint32_t letter = 0u;
        while (at + letter < end && name[letter] != '\0' && listed[at + letter] == name[letter]) letter++;
        if (at + letter == end && name[letter] == '\0') return true;
        at = end;
    }
    return false;
}

bool names(const VkExtensionProperties &listed, const char *wanted)
{
    AV_SCOPE("names");
    uint32_t index = 0u;
    for (; wanted[index] != '\0'; index++)
        if (listed.extensionName[index] != wanted[index]) return false;
    return listed.extensionName[index] == '\0';
}

}

SharedObjectProbe::SharedObjectProbe(Kmt &kmt, GuestReport &report, VulkanEntry &vulkan, ClEntry *cl)
    : kmt_(kmt), vulkan_(vulkan), cl_(cl), target_(VK_NULL_HANDLE), target_types_(0u), filled_(false),
      steps_(kmt, report, scenario)
{
    AV_SCOPE("SharedObjectProbe::SharedObjectProbe");
}

void SharedObjectProbe::opencl_steps(void *handle)
{
    AV_SCOPE("SharedObjectProbe::opencl_steps");
    if (cl_ == nullptr || !filled_) return;
    void    *platforms[4] = {};
    uint32_t written = 0u;
    if (steps_.check_result("cl_platform", cl_->platforms(platforms, 4u, written)) != Status::ok) return;
    if (steps_.check("cl_platform_found", written != 0u ? Status::ok : Status::not_found) != Status::ok)
        return;
    void *devices[4] = {};
    if (steps_.check_result("cl_device", cl_->devices(platforms[0], devices, 4u, written)) != Status::ok)
        return;
    if (steps_.check("cl_device_found", written != 0u ? Status::ok : Status::not_found) != Status::ok) return;
    char   listed[cl_extension_bytes] = {};
    size_t needed = 0u;
    if (steps_.check_result("cl_extensions",
                            cl_->device_text(devices[0], cl_device_extensions, listed, sizeof listed, needed)) !=
        Status::ok)
        return;
    if (!offers(listed, cl_external_memory)) {
        steps_.note("cl_external_memory", "this driver reports none: nothing here may name a handle");
        return;
    }
    int32_t     error = 0;
    void *const context = cl_->create_context(devices[0], error);
    if (steps_.check_result("cl_context", context != nullptr ? 0 : error) != Status::ok) return;
    ScopeExit dropping_context([&] { cl_->release_context(context); });
    void *const queue = cl_->create_queue(context, devices[0], error);
    if (steps_.check_result("cl_queue", queue != nullptr ? 0 : error) != Status::ok) return;
    ScopeExit dropping_queue([&] { cl_->release_queue(queue); });

    const uint64_t properties[] = {external_memory_win32,
                                   static_cast<uint64_t>(reinterpret_cast<uintptr_t>(handle)), 0u};
    void *const    shared =
        cl_->create_buffer_with_properties(context, properties, cl_read_write, bytes, error);
    if (steps_.check_result("cl_opened", shared != nullptr ? 0 : error) != Status::ok) return;
    ScopeExit dropping_buffer([&] { cl_->release_buffer(shared); });

    uint32_t read[8] = {};
    const int32_t taken = cl_->read_buffer(queue, shared, sizeof read, read);
    const bool    same = taken == 0 && read[0] == filled_word && read[7] == filled_word;
    steps_.check_result("cl_read", same ? 0 : (taken != 0 ? taken : -1));
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

bool SharedObjectProbe::target_step(ProbeDevice &device)
{
    AV_SCOPE("SharedObjectProbe::target_step");
    VkBufferCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size = bytes;
    info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (steps_.check("target", answered(vulkan_.create_buffer(device.device(), &info, &target_))) != Status::ok)
        return false;
    VkMemoryRequirements wanted = {};
    vulkan_.buffer_memory_requirements(device.device(), target_, &wanted);
    target_types_ = wanted.memoryTypeBits;
    return true;
}

bool SharedObjectProbe::exportable_step(ProbeDevice &device, VkDeviceMemory &memory)
{
    AV_SCOPE("SharedObjectProbe::exportable_step");
    uint32_t type = 0u;
    if (!device.memory_type(target_types_, 0u, type)) {
        steps_.check("exportable", Status::not_supported);
        return false;
    }
    VkExportMemoryAllocateInfo exporting = {};
    exporting.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
    exporting.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
    VkMemoryAllocateInfo allocate = {};
    allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate.pNext = &exporting;
    allocate.allocationSize = bytes;
    allocate.memoryTypeIndex = type;
    const VkResult made = vulkan_.allocate_memory(device.device(), &allocate, &memory);
    const Status judged = made == VK_SUCCESS && memory != VK_NULL_HANDLE ? Status::ok : Status::invalid_state;
    return steps_.check_kernel("exportable", judged,
                               vulkan_.last_kernel_status(device.device())) ==
           Status::ok;
}

void SharedObjectProbe::fill_step(ProbeDevice &device, VkDeviceMemory memory)
{
    AV_SCOPE("SharedObjectProbe::fill_step");
    ProbeOutcome outcome;
    ScopeExit    reporting([&] { outcome.report(steps_, "filled"); });
    if (!outcome.accept(vulkan_.bind_buffer_memory(device.device(), target_, memory))) return;
    ProbeCommands commands(device);
    if (!commands.begin(outcome)) return;
    vulkan_.cmd_fill_buffer(commands.buffer(), target_, bytes, filled_word);
    if (!commands.run(outcome)) return;
    filled_ = true;
    outcome.measure(1u, 1u);
}

bool SharedObjectProbe::handle_step(ProbeDevice &device, VkDeviceMemory memory, void *&handle)
{
    AV_SCOPE("SharedObjectProbe::handle_step");
    const VkResult got = vulkan_.memory_win32_handle(device.device(), memory, &handle);
    const Status   status = got == VK_SUCCESS && handle != nullptr ? Status::ok : Status::invalid_state;
    return steps_.check_kernel("handle", status, vulkan_.last_kernel_status(device.device())) == Status::ok;
}

void SharedObjectProbe::released_step(ProbeDevice &device, VkDeviceMemory memory)
{
    AV_SCOPE("SharedObjectProbe::released_step");
    vulkan_.destroy_buffer(device.device(), target_);
    vulkan_.free_memory(device.device(), memory);
    const uint32_t refused = vulkan_.last_kernel_status(device.device());
    steps_.check_kernel("released", refused == 0u ? Status::ok : Status::invalid_state, refused);
}

void SharedObjectProbe::carrier_size_steps(uint32_t device)
{
    AV_SCOPE("SharedObjectProbe::carrier_size_steps");
    struct Sweep {
        const char *step;
        uint32_t    bytes;
        bool        with_record;
    };
    static const Sweep sweep[] = {
        {"carrier_size_48", 48u, false},       {"carrier_size_56", 56u, false},
        {"carrier_size_64", 64u, false},       {"carrier_size_72", 72u, false},
        {"carrier_size_80", 80u, false},       {"carrier_size_88", 88u, false},
        {"carrier_size_96", 96u, false},       {"carrier_size_128", 128u, false},
        {"carrier_size_96_valid", 96u, true},
    };
    uint8_t                    filler[128] = {};
    uint8_t                    carried[128] = {};
    const AllocationDescriptor carrier(AllocationKind::client, true, SurfaceLayout(),
                                       SharedObjectRecord::page_size, ScanoutTarget());
    static_cast<void>(carrier.encode(filler, AllocationDescriptor::encoded_size));
    static_cast<void>(carrier.encode(carried, AllocationDescriptor::encoded_size));
    static_cast<void>(SharedObjectRecord(0u, SharedObjectKind(SharedObjectKind::memory), 0u, bytes)
                          .encode(carried + AllocationDescriptor::encoded_size,
                                  SharedObjectRecord::encoded_size));
    for (const Sweep &one : sweep) {
        uint32_t             made = 0u;
        const uint8_t *const from = one.with_record ? carried : filler;
        const Status         answered = kmt_.create_allocation(device, from, one.bytes, made);
        steps_.check_measured(one.step, Status::ok, kmt_.last_code());
        if (answered == Status::ok) static_cast<void>(kmt_.destroy_allocation(device, made));
    }
}

void SharedObjectProbe::carrier_steps()
{
    AV_SCOPE("SharedObjectProbe::carrier_steps");
    AdapterSearch search(kmt_);
    if (steps_.check("carrier_adapter", search.find_avgpu(0u)) != Status::ok) return;
    uint32_t device = 0u;
    if (steps_.check("carrier_device", kmt_.create_device(search.adapter(), device)) != Status::ok) return;

    uint8_t     data[AllocationDescriptor::encoded_size + SharedObjectRecord::encoded_size] = {};
    const AllocationDescriptor carrier(AllocationKind::client, true, SurfaceLayout(),
                                       SharedObjectRecord::page_size, ScanoutTarget());
    static_cast<void>(carrier.encode(data, AllocationDescriptor::encoded_size));

    carrier_size_steps(device);

    uint32_t     plain = 0u;
    const Status made = kmt_.create_allocation(device, data, AllocationDescriptor::encoded_size, plain);
    steps_.check("carrier_plain", made);
    if (made == Status::ok) static_cast<void>(kmt_.destroy_allocation(device, plain));

    static_cast<void>(SharedObjectRecord(0u, SharedObjectKind(SharedObjectKind::memory), 0u, bytes)
                          .encode(data + AllocationDescriptor::encoded_size,
                                  SharedObjectRecord::encoded_size));
    uint32_t     copied = 0u;
    const Status longer = kmt_.create_allocation(device, data, sizeof data, copied);
    steps_.check("carrier_long", longer);
    if (longer == Status::ok) static_cast<void>(kmt_.destroy_allocation(device, copied));

    static_cast<void>(SharedObjectRecord(0u, SharedObjectKind(SharedObjectKind::memory), 0u, bytes)
                          .encode(data + AllocationDescriptor::encoded_size,
                                  SharedObjectRecord::encoded_size));
    uint32_t     shared = 0u;
    const Status minted = kmt_.create_shared_allocation(device, data, sizeof data, shared);
    steps_.check("carrier_shared", minted);
    if (minted == Status::ok) {
        SharedObjectRecord read;
        const Status       decoded = read.decode(data + AllocationDescriptor::encoded_size,
                                                 SharedObjectRecord::encoded_size);
        steps_.check("carrier_minted", decoded == Status::ok && read.assigned() ? Status::ok
                                                                               : Status::invalid_state);
        static_cast<void>(kmt_.destroy_shared_allocation(device, shared));
    }
    static_cast<void>(kmt_.destroy_device(device));
}

void SharedObjectProbe::run()
{
    AV_SCOPE("SharedObjectProbe::run");
    AdapterSearch search(kmt_);
    uint64_t      luid = 0u;
    Status        found = search.find_avgpu(0u);
    if (found == Status::ok) found = kmt_.adapter_luid(search.adapter(), luid);
    if (steps_.check("adapter", found) != Status::ok) return;

    carrier_steps();

    ProbeDevice device(vulkan_);
    if (steps_.check("instance", answered(device.make_instance())) != Status::ok) return;
    if (steps_.check("physical_device", answered(device.list_physical_devices())) != Status::ok) return;
    if (steps_.check("chosen", device.choose(luid) ? Status::ok : Status::not_found) != Status::ok) return;
    if (steps_.check("family", device.choose_family() ? Status::ok : Status::not_found) != Status::ok) return;
    const bool offered = reported_step(device);
    steps_.check("reported", offered ? Status::ok : Status::not_supported);
    if (!offered) return;
    const char *const shared_extensions[] = {win32_memory};
    if (steps_.check("device", answered(device.make_device(shared_extensions, 1u))) != Status::ok) return;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    if (!target_step(device)) return;
    if (!exportable_step(device, memory)) return;
    fill_step(device, memory);
    void *handle = nullptr;
    if (handle_step(device, memory, handle)) opencl_steps(handle);
    released_step(device, memory);
}

}

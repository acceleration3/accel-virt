#include "guest/suite/host_visible_import_scenario.hpp"

#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "proto/generated/vulkan/vk_dispatch_client.h"

namespace av {

namespace {

struct SideSteps {
    const char        *chunk;
    const char        *lock;
    const char        *bind;
    const char        *pages;
    const char        *buffer;
    const char        *import;
    const char        *bind_buffer;
    VkBufferUsageFlags usage;
};

constexpr SideSteps side_steps[] = {
    {"src_chunk", "src_lock", "src_bind", "src_pages", "src_buffer", "src_import", "src_bind_buffer",
     VK_BUFFER_USAGE_TRANSFER_SRC_BIT},
    {"dst_chunk", "dst_lock", "dst_bind", "dst_pages", "dst_buffer", "dst_import", "dst_bind_buffer",
     VK_BUFFER_USAGE_TRANSFER_DST_BIT},
};

constexpr uint64_t page_bytes = 4096u;
/* The queue VulkanDeviceChain made its device with. */
constexpr uint32_t queue_family = 0u;

}  // namespace

HostVisibleImportScenario::HostVisibleImportScenario(Kmt &kmt, GuestReport &report, const AdapterSearch &search)
    : steps_(kmt, report, "import"), link_(kmt, search, steps_, SubApi::vulkan), chain_(steps_, link_.transport()),
      pattern_(copy_bytes),
      chunks_{HostVisibleChunk(link_.connection(), link_.transport()),
              HostVisibleChunk(link_.connection(), link_.transport())},
      imports_{BlobImport(link_.transport()), BlobImport(link_.transport())}, buffers_{}, memory_types_{},
      pool_(VK_NULL_HANDLE), command_buffer_(0u), fence_(VK_NULL_HANDLE)
      {
          AV_SCOPE("HostVisibleImportScenario::HostVisibleImportScenario");
      }

void HostVisibleImportScenario::run()
{
    AV_SCOPE("HostVisibleImportScenario::run");
    ScopeExit closing([&] { link_.close(); });
    if (link_.open() != Status::ok) return;
    Status built = chain_.make();
    if (built == Status::ok) built = read_memory_types();
    for (uint32_t side = source; side < sides && built == Status::ok; side++) {
        built = make_chunk(side);
        if (built == Status::ok) built = make_buffer(side);
    }
    if (built == Status::ok) built = copy();
    const Status released = destroy();
    if (built == Status::ok) static_cast<void>(steps_.check("release", released));
}

/* The call answers no result, and a device always has a memory type: none at
 * all means the answer never came. */
Status HostVisibleImportScenario::read_memory_types()
{
    AV_SCOPE("HostVisibleImportScenario::read_memory_types");
    vkproto::rvk_vkGetPhysicalDeviceMemoryProperties(link_.transport(), chain_.physical_device(), &memory_types_);
    const uint32_t count = memory_types_.memoryTypeCount;
    return steps_.check_measured("memory_types", count != 0u ? Status::ok : Status::not_found, count);
}

Status HostVisibleImportScenario::make_chunk(uint32_t side)
{
    AV_SCOPE("HostVisibleImportScenario::make_chunk");
    const SideSteps  &names = side_steps[side];
    HostVisibleChunk &chunk = chunks_[side];
    const Status      allocated = steps_.check(names.chunk, chunk.allocate(chunk_bytes));
    if (allocated != Status::ok) return allocated;
    const Status locked = steps_.check(names.lock, chunk.lock());
    if (locked != Status::ok) return locked;
    const Status bound = chunk.bind();
    if (bound != Status::ok) return steps_.check(names.bind, bound);
    static_cast<void>(steps_.check_measured(names.bind, bound, chunk.resource_id()));
    const uint64_t pages = chunk.pages();
    return steps_.check_measured(names.pages, pages == chunk_bytes / page_bytes ? Status::ok : Status::malformed,
                                 static_cast<uint32_t>(pages));
}

Status HostVisibleImportScenario::make_buffer(uint32_t side)
{
    AV_SCOPE("HostVisibleImportScenario::make_buffer");
    const SideSteps                &names = side_steps[side];
    KmtTransport                   &transport = link_.transport();
    /* No VkExternalMemoryBufferCreateInfo of its own: which host handle the
     * guest's pages reach the GPU through - a host pointer or a dma-buf - is
     * the host's to decide, and the device names it on every buffer made on a
     * device that imports. A guest naming one would name the wrong one. */
    VkBufferCreateInfo buffer_info = {};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = copy_bytes;
    buffer_info.usage = names.usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkBuffer       buffer = VK_NULL_HANDLE;
    const VkResult created = vkproto::rvk_vkCreateBuffer(transport, chain_.device(), &buffer_info, nullptr, &buffer);
    const Status   buffered = steps_.check_result(names.buffer, created);
    if (buffered != Status::ok) return buffered;
    buffers_[side] = buffer;
    const Status imported = import_chunk(side);
    if (imported != Status::ok) return imported;
    const VkDeviceMemory memory = reinterpret_cast<VkDeviceMemory>(imports_[side].memory());
    const VkResult bound = vkproto::rvk_vkBindBufferMemory(transport, chain_.device(), buffers_[side], memory, 0u);
    return steps_.check_result(names.bind_buffer, bound);
}

/* Host-visible types the buffer can live in, lowest first, until the host takes
 * one: a type the host cannot import a host pointer into is refused and the
 * next is tried; any other failure ends the search. The type taken is what the
 * step reports; a search that ends without one reports the host's last status. */
Status HostVisibleImportScenario::import_chunk(uint32_t side)
{
    AV_SCOPE("HostVisibleImportScenario::import_chunk");
    const char *const    step = side_steps[side].import;
    VkMemoryRequirements requirements = {};
    vkproto::rvk_vkGetBufferMemoryRequirements(link_.transport(), chain_.device(), buffers_[side], &requirements);
    Status status = Status::not_found;
    for (uint32_t type = 0u; type < memory_types_.memoryTypeCount; type++) {
        const bool host_visible =
            (memory_types_.memoryTypes[type].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0u;
        if ((requirements.memoryTypeBits >> type & 1u) == 0u || !host_visible) continue;
        status = imports_[side].import(chunks_[side].resource_id(), 0u, copy_bytes, type);
        if (status == Status::ok) return steps_.check_measured(step, status, type);
        if (status != Status::invalid_parameter) break;
    }
    return steps_.check_measured(step, status, imports_[side].host_status());
}

/* The destination is read just before the copy is submitted, so a host that
 * wrote it any earlier - or a view that is the source's - is seen. */
Status HostVisibleImportScenario::copy()
{
    AV_SCOPE("HostVisibleImportScenario::copy");
    pattern_.clear(chunks_[destination].view());
    pattern_.write(chunks_[source].view());
    const Status recorded = record_copy();
    if (recorded != Status::ok) return recorded;
    const uint32_t filled_before = pattern_.filled(chunks_[destination].view());
    const Status   submitted = submit_copy();
    if (submitted != Status::ok) return submitted;
    return judge_copy(filled_before);
}

Status HostVisibleImportScenario::record_copy()
{
    AV_SCOPE("HostVisibleImportScenario::record_copy");
    KmtTransport           &transport = link_.transport();
    const VkDevice          device = chain_.device();
    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = queue_family;
    VkCommandPool pool = VK_NULL_HANDLE;
    const Status  pooled = steps_.check_result(
        "command_pool", vkproto::rvk_vkCreateCommandPool(transport, device, &pool_info, nullptr, &pool));
    if (pooled != Status::ok) return pooled;
    pool_ = pool;
    VkCommandBufferAllocateInfo allocate = {};
    allocate.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate.commandPool = pool_;
    allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate.commandBufferCount = 1u;
    uint64_t     command_buffer = 0u;
    const Status allocated = steps_.check_result(
        "command_buffer", vkproto::rvk_vkAllocateCommandBuffers(transport, device, &allocate, &command_buffer));
    if (allocated != Status::ok) return allocated;
    command_buffer_ = command_buffer;
    const VkCommandBuffer    recording = reinterpret_cast<VkCommandBuffer>(command_buffer_);
    VkCommandBufferBeginInfo begin = {};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VkResult recorded = vkproto::rvk_vkBeginCommandBuffer(transport, recording, &begin);
    if (recorded == VK_SUCCESS) {
        const VkBufferCopy region = {0u, 0u, copy_bytes};
        vkproto::rvk_vkCmdCopyBuffer(transport, recording, buffers_[source], buffers_[destination], 1u, &region);
        recorded = vkproto::rvk_vkEndCommandBuffer(transport, recording);
    }
    return steps_.check_result("record", recorded);
}

/* The queue query answers no result: a null queue means the answer never came. */
Status HostVisibleImportScenario::submit_copy()
{
    AV_SCOPE("HostVisibleImportScenario::submit_copy");
    KmtTransport  &transport = link_.transport();
    const VkDevice device = chain_.device();
    uint64_t       queue = 0u;
    vkproto::rvk_vkGetDeviceQueue(transport, device, queue_family, 0u, &queue);
    const Status queued = steps_.check("queue", queue != 0u ? Status::ok : Status::not_found);
    if (queued != Status::ok) return queued;
    VkFenceCreateInfo fence_info = {};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence      fence = VK_NULL_HANDLE;
    const Status fenced =
        steps_.check_result("fence", vkproto::rvk_vkCreateFence(transport, device, &fence_info, nullptr, &fence));
    if (fenced != Status::ok) return fenced;
    fence_ = fence;
    const VkCommandBuffer submitted = reinterpret_cast<VkCommandBuffer>(command_buffer_);
    VkSubmitInfo          submit = {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1u;
    submit.pCommandBuffers = &submitted;
    return steps_.check_result(
        "submit", vkproto::rvk_vkQueueSubmit(transport, reinterpret_cast<VkQueue>(queue), 1u, &submit, fence_));
}

/* A copy the host has not finished cannot be judged by its bytes, so a wait
 * that never saw the fence fails copied whatever the view holds. */
Status HostVisibleImportScenario::judge_copy(uint32_t filled_before)
{
    AV_SCOPE("HostVisibleImportScenario::judge_copy");
    uint32_t slices = 0u;
    VkResult waited = VK_TIMEOUT;
    while (waited == VK_TIMEOUT && slices < fence_slices) {
        waited = vkproto::rvk_vkWaitForFences(link_.transport(), chain_.device(), 1u, &fence_, VK_TRUE, fence_slice_ns);
        slices++;
    }
    const Status   finished =
        steps_.check_measured("wait", waited == VK_SUCCESS ? Status::ok : Status::invalid_state, slices);
    const uint32_t matched = pattern_.matched(chunks_[destination].view());
    const Status   whole = matched == copy_bytes ? Status::ok : Status::malformed;
    const Status   copied = steps_.check_measured("copied", finished != Status::ok ? finished : whole, matched);
    const Status   unwritten = filled_before == 0u ? Status::ok : Status::malformed;
    return steps_.check_measured("guest_saw_host_write", copied != Status::ok ? copied : unwritten, filled_before);
}

/* Whatever holds the imports goes before them, and the imports before the chunks
 * they name; a null fence, pool or buffer is the API's own no-op, a null command
 * buffer is not. */
Status HostVisibleImportScenario::destroy()
{
    AV_SCOPE("HostVisibleImportScenario::destroy");
    KmtTransport  &transport = link_.transport();
    const VkDevice device = chain_.device();
    if (device != VK_NULL_HANDLE) {
        vkproto::rvk_vkDestroyFence(transport, device, fence_, nullptr);
        const VkCommandBuffer command_buffer = reinterpret_cast<VkCommandBuffer>(command_buffer_);
        if (command_buffer_ != 0u) vkproto::rvk_vkFreeCommandBuffers(transport, device, pool_, 1u, &command_buffer);
        vkproto::rvk_vkDestroyCommandPool(transport, device, pool_, nullptr);
        for (const VkBuffer buffer : buffers_) vkproto::rvk_vkDestroyBuffer(transport, device, buffer, nullptr);
    }
    Status     first = Status::ok;
    const auto keep = [&first](Status status) { first = first == Status::ok ? status : first; };
    for (BlobImport &import : imports_) keep(import.release());
    for (HostVisibleChunk &chunk : chunks_) keep(chunk.release());
    chain_.destroy();
    fence_ = VK_NULL_HANDLE;
    command_buffer_ = 0u;
    pool_ = VK_NULL_HANDLE;
    for (VkBuffer &buffer : buffers_) buffer = VK_NULL_HANDLE;
    uint32_t answered = 0u;
    keep(transport.nop(answered));
    return first;
}

}  // namespace av

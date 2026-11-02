#include "guest/suite/host_visible_import_scenario.hpp"

#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "proto/generated/vulkan/vk_dispatch_client.h"
#include "proto/generated/vulkan/vk_serialize.h"

namespace av {

namespace {

struct SideSteps {
    const char        *chunk;
    const char        *buffer;
    const char        *import;
    const char        *bind_buffer;
    VkBufferUsageFlags usage;
};

constexpr SideSteps side_steps[] = {
    {"src_chunk", "src_buffer", "src_import", "src_bind_buffer", VK_BUFFER_USAGE_TRANSFER_SRC_BIT},
    {"dst_chunk", "dst_buffer", "dst_import", "dst_bind_buffer", VK_BUFFER_USAGE_TRANSFER_DST_BIT},
};

constexpr uint32_t queue_family = 0u;

}

HostVisibleImportScenario::HostVisibleImportScenario(Kmt &kmt, GuestReport &report, const AdapterSearch &search)
    : steps_(kmt, report, "import"), link_(kmt, search, steps_, SubApi::vulkan), chain_(steps_, link_.transport()),
      pattern_(copy_bytes),
      chunks_{HostWindowChunk(link_.connection(), link_.transport()),
              HostWindowChunk(link_.connection(), link_.transport())},
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
    for (uint32_t side = source; side < sides && built == Status::ok; side++) built = make_buffer(side);
    if (built == Status::ok) built = copy();
    const Status released = destroy();
    if (built == Status::ok) static_cast<void>(steps_.check("release", released));
}

Status HostVisibleImportScenario::read_memory_types()
{
    AV_SCOPE("HostVisibleImportScenario::read_memory_types");
    vkproto::av_vkGetPhysicalDeviceMemoryProperties(link_.transport(), chain_.handles(), chain_.physical_device(), &memory_types_);
    const uint32_t count = memory_types_.memoryTypeCount;
    return steps_.check_measured("memory_types", count != 0u ? Status::ok : Status::not_found, count);
}

Status HostVisibleImportScenario::make_buffer(uint32_t side)
{
    AV_SCOPE("HostVisibleImportScenario::make_buffer");
    const SideSteps                &names = side_steps[side];
    KmtTransport                   &transport = link_.transport();
    VkBufferCreateInfo buffer_info = {};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = copy_bytes;
    buffer_info.usage = names.usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkBuffer       buffer = VK_NULL_HANDLE;
    const VkResult created = vkproto::av_vkCreateBuffer(transport, chain_.handles(), chain_.device(), &buffer_info, nullptr, &buffer);
    const Status   buffered = steps_.check_result(names.buffer, created);
    if (buffered != Status::ok) return buffered;
    buffers_[side] = buffer;
    const Status imported = import_chunk(side);
    if (imported != Status::ok) return imported;
    const VkDeviceMemory memory = vkproto::as_handle<VkDeviceMemory>(imports_[side].memory());
    const VkResult bound = vkproto::av_vkBindBufferMemory(transport, chain_.handles(), chain_.device(), buffers_[side], memory, 0u);
    return steps_.check_result(names.bind_buffer, bound);
}

Status HostVisibleImportScenario::import_chunk(uint32_t side)
{
    AV_SCOPE("HostVisibleImportScenario::import_chunk");
    const SideSteps     &names = side_steps[side];
    VkMemoryRequirements requirements = {};
    vkproto::av_vkGetBufferMemoryRequirements(link_.transport(), chain_.handles(), chain_.device(), buffers_[side], &requirements);
    uint32_t type = memory_types_.memoryTypeCount;
    for (uint32_t candidate = memory_types_.memoryTypeCount; candidate-- > 0u;) {
        const bool host_visible =
            (memory_types_.memoryTypes[candidate].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0u;
        if ((requirements.memoryTypeBits >> candidate & 1u) != 0u && host_visible) type = candidate;
    }
    if (type == memory_types_.memoryTypeCount) return steps_.check(names.chunk, Status::not_found);
    HostWindowChunk &chunk = chunks_[side];
    const Status     opened = chunk.open(chunk_bytes, HostMemoryRequest(HostMemoryRequest::Kind::vulkan, type));
    if (opened != Status::ok) return steps_.check(names.chunk, opened);
    static_cast<void>(steps_.check_measured(names.chunk, opened, chunk.resource_id()));
    const Status imported = imports_[side].import(chunk.resource_id());
    if (imported == Status::ok) return steps_.check_measured(names.import, imported, type);
    return steps_.check_measured(names.import, imported, imports_[side].host_status());
}

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
        "command_pool", vkproto::av_vkCreateCommandPool(transport, chain_.handles(), device, &pool_info, nullptr, &pool));
    if (pooled != Status::ok) return pooled;
    pool_ = pool;
    VkCommandBufferAllocateInfo allocate = {};
    allocate.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate.commandPool = pool_;
    allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate.commandBufferCount = 1u;
    uint64_t     command_buffer = 0u;
    const Status allocated = steps_.check_result(
        "command_buffer", vkproto::av_vkAllocateCommandBuffers(transport, chain_.handles(), device, &allocate, &command_buffer));
    if (allocated != Status::ok) return allocated;
    command_buffer_ = command_buffer;
    const VkCommandBuffer    recording = reinterpret_cast<VkCommandBuffer>(command_buffer_);
    VkCommandBufferBeginInfo begin = {};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VkResult recorded = vkproto::av_vkBeginCommandBuffer(transport, chain_.handles(), recording, &begin);
    if (recorded == VK_SUCCESS) {
        const VkBufferCopy region = {0u, 0u, copy_bytes};
        vkproto::av_vkCmdCopyBuffer(transport, chain_.handles(), recording, buffers_[source], buffers_[destination], 1u, &region);
        recorded = vkproto::av_vkEndCommandBuffer(transport, chain_.handles(), recording);
    }
    return steps_.check_result("record", recorded);
}

Status HostVisibleImportScenario::submit_copy()
{
    AV_SCOPE("HostVisibleImportScenario::submit_copy");
    KmtTransport  &transport = link_.transport();
    const VkDevice device = chain_.device();
    uint64_t       queue = 0u;
    vkproto::av_vkGetDeviceQueue(transport, chain_.handles(), device, queue_family, 0u, &queue);
    const Status queued = steps_.check("queue", queue != 0u ? Status::ok : Status::not_found);
    if (queued != Status::ok) return queued;
    VkFenceCreateInfo fence_info = {};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence      fence = VK_NULL_HANDLE;
    const Status fenced =
        steps_.check_result("fence", vkproto::av_vkCreateFence(transport, chain_.handles(), device, &fence_info, nullptr, &fence));
    if (fenced != Status::ok) return fenced;
    fence_ = fence;
    const VkCommandBuffer submitted = reinterpret_cast<VkCommandBuffer>(command_buffer_);
    VkSubmitInfo          submit = {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1u;
    submit.pCommandBuffers = &submitted;
    return steps_.check_result(
        "submit", vkproto::av_vkQueueSubmit(transport, chain_.handles(), reinterpret_cast<VkQueue>(queue), 1u, &submit, fence_));
}

Status HostVisibleImportScenario::judge_copy(uint32_t filled_before)
{
    AV_SCOPE("HostVisibleImportScenario::judge_copy");
    uint32_t slices = 0u;
    VkResult waited = VK_TIMEOUT;
    while (waited == VK_TIMEOUT && slices < fence_slices) {
        waited = vkproto::av_vkWaitForFences(link_.transport(), chain_.handles(), chain_.device(), 1u, &fence_, VK_TRUE, fence_slice_ns);
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

Status HostVisibleImportScenario::destroy()
{
    AV_SCOPE("HostVisibleImportScenario::destroy");
    KmtTransport  &transport = link_.transport();
    const VkDevice device = chain_.device();
    if (device != VK_NULL_HANDLE) {
        vkproto::av_vkDestroyFence(transport, chain_.handles(), device, fence_, nullptr);
        const VkCommandBuffer command_buffer = reinterpret_cast<VkCommandBuffer>(command_buffer_);
        if (command_buffer_ != 0u) vkproto::av_vkFreeCommandBuffers(transport, chain_.handles(), device, pool_, 1u, &command_buffer);
        vkproto::av_vkDestroyCommandPool(transport, chain_.handles(), device, pool_, nullptr);
        for (const VkBuffer buffer : buffers_) vkproto::av_vkDestroyBuffer(transport, chain_.handles(), device, buffer, nullptr);
    }
    Status     first = Status::ok;
    const auto keep = [&first](Status status) { first = first == Status::ok ? status : first; };
    for (BlobImport &import : imports_) keep(import.release());
    for (HostWindowChunk &chunk : chunks_) keep(chunk.release());
    chain_.destroy();
    fence_ = VK_NULL_HANDLE;
    command_buffer_ = 0u;
    pool_ = VK_NULL_HANDLE;
    for (VkBuffer &buffer : buffers_) buffer = VK_NULL_HANDLE;
    uint32_t answered = 0u;
    keep(transport.nop(answered));
    return first;
}

}

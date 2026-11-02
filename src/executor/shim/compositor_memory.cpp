#include "executor/shim/compositor_memory.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/exported_memory.hpp"
#include "executor/owned_fd.hpp"
#include "executor/shim/compositor_device.hpp"

#include <sys/mman.h>
#include <vulkan/vulkan.h>

namespace av {

CompositorMemory::CompositorMemory(CompositorDevice &device) : device_(device)
{
    AV_SCOPE("CompositorMemory::CompositorMemory");
}

void CompositorMemory::memory_properties(VkPhysicalDeviceMemoryProperties &answer)
{
    AV_SCOPE("CompositorMemory::memory_properties");
    answer = VkPhysicalDeviceMemoryProperties{};
    if (device_.opened()) device_.table().memory_properties(answer);
}

bool CompositorMemory::exportable(uint32_t type)
{
    AV_SCOPE("CompositorMemory::exportable");
    ExportedMemory probe;
    if (allocate(type, probe_bytes, probe) != Status::ok) return false;
    void *const mapped = mmap(nullptr, probe_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, probe.fd(), 0);
    if (mapped != MAP_FAILED) munmap(mapped, probe_bytes);
    free(probe);
    return mapped != MAP_FAILED;
}

bool CompositorMemory::imports_host_visible(uint32_t type)
{
    AV_SCOPE("CompositorMemory::imports_host_visible");
    ExportedMemory probe;
    if (allocate(type, probe_bytes, probe) != Status::ok) return false;
    const VulkanDeviceTable &table = device_.table();
    const uint32_t           types = table.dma_buf_types(probe.fd());
    free(probe);
    VkPhysicalDeviceMemoryProperties memory = {};
    table.memory_properties(memory);
    for (uint32_t candidate = 0u; candidate < memory.memoryTypeCount; candidate++) {
        if (((types >> candidate) & 1u) == 0u) continue;
        if ((memory.memoryTypes[candidate].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0u) return true;
    }
    return false;
}

Status CompositorMemory::allocate(uint32_t type, uint64_t size, ExportedMemory &memory)
{
    AV_SCOPE("CompositorMemory::allocate");
    memory = ExportedMemory();
    const VulkanDeviceTable &table = device_.table();
    VkDeviceMemory           made = VK_NULL_HANDLE;
    if (!device_.opened() || !table.allocate_exportable(size, type, made)) return Status::no_memory;
    int     fd = -1;
    void   *pointer = nullptr;
    const bool exported = table.memory_fd(made, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, fd);
    OwnedFd descriptor(fd);
    if (!exported || !table.map_memory(made, pointer)) {
        table.free_memory(made);
        return Status::no_memory;
    }
    memory = ExportedMemory(reinterpret_cast<uint64_t>(made), descriptor.take(), static_cast<uint8_t *>(pointer), size);
    return Status::ok;
}

void CompositorMemory::free(const ExportedMemory &memory)
{
    AV_SCOPE("CompositorMemory::free");
    if (!memory.made()) return;
    OwnedFd exported(memory.fd());
    device_.table().free_memory(reinterpret_cast<VkDeviceMemory>(memory.handle()));
}

}

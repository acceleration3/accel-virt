#include "executor/shim/compositor_memory.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/exported_memory.hpp"
#include "executor/shim/compositor_device.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <vulkan/vulkan.h>

namespace av {

CompositorMemory::CompositorMemory(CompositorDevice &device) : device_(device)
{
    AV_SCOPE("CompositorMemory::CompositorMemory");
}

uint32_t CompositorMemory::memory_types()
{
    AV_SCOPE("CompositorMemory::memory_types");
    return device_.opened() ? device_.table().memory_type_count() : 0u;
}

uint32_t CompositorMemory::memory_flags(uint32_t type)
{
    AV_SCOPE("CompositorMemory::memory_flags");
    return device_.table().memory_flags(type);
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

Status CompositorMemory::allocate(uint32_t type, uint64_t size, ExportedMemory &memory)
{
    AV_SCOPE("CompositorMemory::allocate");
    memory = ExportedMemory();
    const VulkanDeviceTable &table = device_.table();
    VkDeviceMemory           made = VK_NULL_HANDLE;
    if (!device_.opened() || !table.allocate_exportable(size, type, made)) return Status::no_memory;
    int   fd = -1;
    void *pointer = nullptr;
    if (!table.memory_fd(made, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, fd) ||
        !table.map_memory(made, pointer)) {
        if (fd >= 0) close(fd);
        table.free_memory(made);
        return Status::no_memory;
    }
    memory = ExportedMemory(reinterpret_cast<uint64_t>(made), fd, static_cast<uint8_t *>(pointer), size);
    return Status::ok;
}

/* Freeing memory unmaps it too (vkFreeMemory). */
void CompositorMemory::free(const ExportedMemory &memory)
{
    AV_SCOPE("CompositorMemory::free");
    if (!memory.made()) return;
    if (memory.fd() >= 0) close(memory.fd());
    device_.table().free_memory(reinterpret_cast<VkDeviceMemory>(memory.handle()));
}

int CompositorMemory::share(const ExportedMemory &memory)
{
    AV_SCOPE("CompositorMemory::share");
    return memory.fd() >= 0 ? fcntl(memory.fd(), F_DUPFD_CLOEXEC, 0) : -1;
}

}  // namespace av

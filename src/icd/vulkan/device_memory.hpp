#ifndef AV_ICD_DEVICE_MEMORY_HPP
#define AV_ICD_DEVICE_MEMORY_HPP

#include "core/support/status.hpp"
#include "icd/vulkan/memory_type_table.hpp"

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {
class HostAllocation;
class Kmt;
class DriverLog;
class StateWriter;
}

namespace av::vkproto {
class HandleTranslation;
}

namespace av::icd {

class Connection;
struct DeviceMemoryProbe;

class DeviceMemory {
public:
    DeviceMemory(Kmt &kmt, Connection &connection, vkproto::HandleTranslation &handles, DriverLog &log);
    DeviceMemory(const DeviceMemory &) = delete;
    DeviceMemory &operator=(const DeviceMemory &) = delete;

    VkResult allocate(VkPhysicalDevice physical, VkDevice device, const VkMemoryAllocateInfo &info,
                      const VkAllocationCallbacks *allocator, VkDeviceMemory &memory);
    VkResult host_pointer_types(VkPhysicalDevice physical, VkExternalMemoryHandleTypeFlagBits type,
                                const void *pointer, uint32_t &types);
    void     free(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks *allocator);
    VkResult map(VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags,
                 void *&data) const;
    [[nodiscard]] Status release_all();

    void report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    static constexpr uintptr_t host_pointer_alignment = 4096u;

    VkResult           lend(const VkMemoryAllocateInfo &info, const VkImportMemoryHostPointerInfoEXT &import,
                            VkDeviceMemory &memory);
    VkResult           refusal(Status refused) const;
    [[nodiscard]] bool learn(VkPhysicalDevice physical);
    VkResult           refusal() const;
    HostAllocation    *find(VkDeviceMemory memory) const;

    Connection     &connection_;
    vkproto::HandleTranslation &handles_;
    DriverLog      &lines_;
    MemoryTypeTable types_;
    bool            learnt_;

    friend struct DeviceMemoryProbe;
};

}

#endif

#ifndef AV_MEMORY_WIN32_HANDLE_RECORD_HPP
#define AV_MEMORY_WIN32_HANDLE_RECORD_HPP

#include <vulkan/vulkan_core.h>

namespace av::icd {
struct MemoryGetWin32HandleRecord {
    VkStructureType type_;
    const void     *next_;
    VkDeviceMemory  memory_;
    VkFlags         handle_type_;
};

struct ImportMemoryWin32HandleRecord {
    VkStructureType type_;
    const void     *next_;
    VkFlags         handle_type_;
    void           *handle_;
    const void     *name_;
};

inline constexpr VkStructureType import_memory_win32_handle_info =
    static_cast<VkStructureType>(1000073000);
inline constexpr VkStructureType export_memory_win32_handle_info =
    static_cast<VkStructureType>(1000073001);
inline constexpr VkStructureType memory_get_win32_handle_info =
    static_cast<VkStructureType>(1000073003);
inline constexpr VkFlags opaque_win32_memory =
    static_cast<VkFlags>(VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT);

}

#endif

#ifndef AV_SEMAPHORE_WIN32_HANDLE_RECORD_HPP
#define AV_SEMAPHORE_WIN32_HANDLE_RECORD_HPP

#include <vulkan/vulkan_core.h>

namespace av::icd {
struct SemaphoreGetWin32HandleRecord {
    VkStructureType type_;
    const void     *next_;
    VkSemaphore     semaphore_;
    VkFlags         handle_type_;
};

struct ImportSemaphoreWin32HandleRecord {
    VkStructureType type_;
    const void     *next_;
    VkSemaphore     semaphore_;
    VkFlags         flags_;
    VkFlags         handle_type_;
    void           *handle_;
    const void     *name_;
};

inline constexpr VkStructureType import_semaphore_win32_handle_info =
    static_cast<VkStructureType>(1000078000);
inline constexpr VkStructureType export_semaphore_win32_handle_info =
    static_cast<VkStructureType>(1000078001);
inline constexpr VkStructureType d3d12_fence_submit_info =
    static_cast<VkStructureType>(1000078002);
inline constexpr VkStructureType semaphore_get_win32_handle_info =
    static_cast<VkStructureType>(1000078003);
inline constexpr VkFlags opaque_win32_semaphore =
    static_cast<VkFlags>(VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT);

}

#endif

/* The layouts of VK_KHR_external_semaphore_win32's structures (vulkan_win32.h),
 * for the same reason as the memory ones beside them: a HANDLE is Windows' own
 * type (feature 047, contracts/external_objects.md). */
#ifndef AV_SEMAPHORE_WIN32_HANDLE_RECORD_HPP
#define AV_SEMAPHORE_WIN32_HANDLE_RECORD_HPP

#include <vulkan/vulkan_core.h>

namespace av::icd {

/* VkSemaphoreGetWin32HandleInfoKHR: what an application exports with. */
struct SemaphoreGetWin32HandleRecord {
    VkStructureType type_;
    const void     *next_;
    VkSemaphore     semaphore_;
    VkFlags         handle_type_;
};

/* VkImportSemaphoreWin32HandleInfoKHR: what it imports with. `name_` is a
 * named object another process could open, which this path refuses. */
struct ImportSemaphoreWin32HandleRecord {
    VkStructureType type_;
    const void     *next_;
    VkSemaphore     semaphore_;
    VkFlags         flags_;
    VkFlags         handle_type_;
    void           *handle_;
    const void     *name_;
};

/* 1000000000 + (79 - 1) * 1000 + the offset, for
 * VK_KHR_external_semaphore_win32's extension number 79. */
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

}  // namespace av::icd

#endif  // AV_SEMAPHORE_WIN32_HANDLE_RECORD_HPP

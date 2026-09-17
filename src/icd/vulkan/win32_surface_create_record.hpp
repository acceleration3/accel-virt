/* VkWin32SurfaceCreateInfoKHR's layout (vulkan_win32.h), which portable code
 * cannot include: the window types are Windows' own. An instance and a window
 * are opaque pointers here, as they are to the ICD. */
#ifndef AV_WIN32_SURFACE_CREATE_RECORD_HPP
#define AV_WIN32_SURFACE_CREATE_RECORD_HPP

#include <vulkan/vulkan_core.h>

namespace av::icd {

/* The fields keep a member's trailing underscore (tools/header_scan.py); the
 * order and the types are what the Windows structure lays out. */
struct Win32SurfaceCreateRecord {
    VkStructureType type_;
    const void     *next_;
    VkFlags         flags_;
    void           *module_;
    void           *window_;
};

/* VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR. */
inline constexpr VkStructureType win32_surface_create_info = static_cast<VkStructureType>(1000009000);

}  // namespace av::icd

#endif  // AV_WIN32_SURFACE_CREATE_RECORD_HPP

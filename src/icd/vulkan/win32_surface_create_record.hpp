#ifndef AV_WIN32_SURFACE_CREATE_RECORD_HPP
#define AV_WIN32_SURFACE_CREATE_RECORD_HPP

#include <vulkan/vulkan_core.h>

namespace av::icd {
struct Win32SurfaceCreateRecord {
    VkStructureType type_;
    const void     *next_;
    VkFlags         flags_;
    void           *module_;
    void           *window_;
};

inline constexpr VkStructureType win32_surface_create_info = static_cast<VkStructureType>(1000009000);

}

#endif

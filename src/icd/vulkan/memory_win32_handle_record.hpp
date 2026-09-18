/* The layouts of VK_KHR_external_memory_win32's structures (vulkan_win32.h),
 * which portable code cannot include: a HANDLE is Windows' own type and the
 * vendored registry headers carry no platform header at all (feature 047,
 * contracts/external_objects.md).
 *
 * A handle is an opaque pointer here, as it is to the ICD: what it names is a
 * row of ExternalObjectTable, not an object the kernel knows, until feature
 * 053 gives objects a device-global identity. */
#ifndef AV_MEMORY_WIN32_HANDLE_RECORD_HPP
#define AV_MEMORY_WIN32_HANDLE_RECORD_HPP

#include <vulkan/vulkan_core.h>

namespace av::icd {

/* The fields keep a member's trailing underscore (tools/header_scan.py); the
 * order and the types are what the Windows structures lay out. */

/* VkMemoryGetWin32HandleInfoKHR: what an application exports with. */
struct MemoryGetWin32HandleRecord {
    VkStructureType type_;
    const void     *next_;
    VkDeviceMemory  memory_;
    VkFlags         handle_type_;
};

/* VkImportMemoryWin32HandleInfoKHR: what it imports with, in an allocation's
 * chain. `name_` is a named section another process could open, which this
 * path refuses. */
struct ImportMemoryWin32HandleRecord {
    VkStructureType type_;
    const void     *next_;
    VkFlags         handle_type_;
    void           *handle_;
    const void     *name_;
};

/* The structure types: no platform header declares them here, so they are the
 * numbers the registry gives - 1000000000 + (74 - 1) * 1000 + the offset, for
 * VK_KHR_external_memory_win32's extension number 74. The handle-type bits
 * are core, so those are the header's own constants. */
inline constexpr VkStructureType import_memory_win32_handle_info =
    static_cast<VkStructureType>(1000073000);
inline constexpr VkStructureType export_memory_win32_handle_info =
    static_cast<VkStructureType>(1000073001);
/* 1000073002 is VkMemoryWin32HandlePropertiesKHR's, which nothing here
 * declares: that query answers which memory types a handle from *outside* may
 * be imported as, and this path refuses a handle it did not export, so there is
 * no foreign handle to answer for (contracts/external_objects.md). */
inline constexpr VkStructureType memory_get_win32_handle_info =
    static_cast<VkStructureType>(1000073003);
/* The one handle type this path answers; the KMT and D3D bits are deliberately
 * not among them (contracts/external_objects.md). */
inline constexpr VkFlags opaque_win32_memory =
    static_cast<VkFlags>(VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT);

}  // namespace av::icd

#endif  // AV_MEMORY_WIN32_HANDLE_RECORD_HPP

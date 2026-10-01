/* The ICD's WSI (feature 046, contracts/wsi.md): the surfaces and swapchains of
 * the process, and the entries that make, query, present and destroy them. */
#ifndef AV_PRESENTATION_HPP
#define AV_PRESENTATION_HPP

#include "core/support/object_storage.hpp"
#include "icd/vulkan/entry_lookup.hpp"
#include "icd/vulkan/surface_table.hpp"
#include "icd/vulkan/swapchain.hpp"
#include "icd/vulkan/win32_surface_create_record.hpp"

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {
class BlockSource;
class Kmt;
class StateWriter;
}  // namespace av

namespace av::icd {

class IcdRoot;
class LogSink;
class WindowChains;
class WindowSurface;
struct PresentationProbe;

/* One swapchain's storage; a swapchain's handle is its slot's address. A type
 * of its own so the table's slots are a complete type (tools/header_scan.py). */
struct SwapchainSlot {
    ObjectStorage<Swapchain> chain_;
};

class Presentation {
public:
    static constexpr uint32_t max_swapchains = 16u;

    Presentation(BlockSource &memory, Kmt &kmt, WindowSurface &windows, WindowChains &chains, LogSink &log,
                 IcdRoot &root, EntryLookup &entries);
    ~Presentation();
    Presentation(const Presentation &) = delete;
    Presentation &operator=(const Presentation &) = delete;

    VkResult create_surface(VkInstance instance, const Win32SurfaceCreateRecord *info, VkSurfaceKHR *surface);
    void     destroy_surface(VkInstance instance, VkSurfaceKHR surface);
    VkResult surface_support(VkSurfaceKHR surface, VkBool32 *supported);
    VkResult capabilities(VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR *capabilities);
    VkResult capabilities2(const VkPhysicalDeviceSurfaceInfo2KHR *info, VkSurfaceCapabilities2KHR *capabilities);
    VkResult formats(VkSurfaceKHR surface, uint32_t *count, VkSurfaceFormatKHR *formats);
    VkResult formats2(const VkPhysicalDeviceSurfaceInfo2KHR *info, uint32_t *count, VkSurfaceFormat2KHR *formats);
    VkResult present_modes(VkSurfaceKHR surface, uint32_t *count, VkPresentModeKHR *modes);

    VkResult create_swapchain(VkDevice device, const VkSwapchainCreateInfoKHR *info, VkSwapchainKHR *swapchain);
    void     destroy_swapchain(VkDevice device, VkSwapchainKHR swapchain);
    VkResult swapchain_images(VkDevice device, VkSwapchainKHR swapchain, uint32_t *count, VkImage *images);
    /* The next image, with `semaphore` and `fence` signalled for it. */
    VkResult acquire(VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout, VkSemaphore semaphore,
                     VkFence fence, uint32_t *index);
    /* Every swapchain of `info` in turn: each result into pResults, the worst
     * as the answer. */
    VkResult present(VkQueue queue, const VkPresentInfoKHR *info);

    /* What goes with an instance or a device. */
    void forget_instance(VkInstance instance);
    void forget_device(VkDevice device);

    uint32_t surfaces() const;
    uint32_t swapchains() const;
    void     report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    Swapchain *find(VkDevice device, VkSwapchainKHR swapchain);
    void       destroy(SwapchainSlot &slot);
    VkResult   worse(VkResult kept, VkResult result) const;

    BlockSource   &memory_;
    Kmt           &kmt_;
    WindowSurface &windows_;
    WindowChains  &chains_;
    LogSink       &log_;
    IcdRoot       &root_;
    EntryLookup   &entries_;
    SurfaceTable   surfaces_;
    SwapchainSlot  slots_[max_swapchains];

    friend struct PresentationProbe;
};

}  // namespace av::icd

#endif  // AV_PRESENTATION_HPP
